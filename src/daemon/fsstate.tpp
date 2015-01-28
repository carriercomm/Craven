template <typename Client, typename ChangeTx>
const std::map<typename dfs::basic_state<Client, ChangeTx>::node_info::state_type,
	  std::string> dfs::basic_state<Client, ChangeTx>::node_info::state_type_tl_ =
{
	{clean, "clean"},
	{pending, "pending"},
	{dirty, "dirty"},
	{active_write, "active_write"},
	{active_read, "active_read"},
	{novel, "novel"},
	{dead, "dead"}
};

template <typename Client, typename ChangeTx>
template <typename Rpc>
void dfs::basic_state<Client, ChangeTx>::handle_request(const Rpc& rpc,
		std::deque<node_info>& cache_queue)
{
	//check the request is not done (ignore invalid) & request
	if(client_.valid(rpc) == std::remove_reference<decltype(client_)>::type::request_done)
		cache_queue.pop_front();
	else
		client_.request(rpc);
}


template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::tick_handle_rename(node_info& ni,
		std::deque<node_info>& cache_queue)
{
	//Only process the from
	if(ni.state == node_info::dead)
	{
		//find the to & check it's first
		std::deque<node_info>& ot_queue = sync_cache_.at(*ni.rename_info);
		if(ot_queue.empty())
			BOOST_LOG_TRIVIAL(warning) << "Malformed rename marker for " << ni.name
				<< " -> " << *ni.rename_info;
		else
		{
			node_info& front_info = ot_queue.front();
			if(front_info.state == node_info::novel
					&& front_info.rename_info
					&& *front_info.rename_info == ni.name
					&& front_info.version == ni.version)
			{
				raft::request::Rename req{id_,
							encode_path(ni.name),
							encode_path(front_info.name),
							ni.version};

				//if the request is done, remove the markers
				if(client_.valid(req) == std::remove_reference<decltype(client_)>::type::request_done)
				{
					ot_queue.pop_front();
					cache_queue.pop_front();
				}
				else //fire request
					client_.request(req);
			}
			//else not front, so don't do anything
			else
				BOOST_LOG_TRIVIAL(info) << "Ignoring rename request until other end is at front";
		}
	}
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::tick()
{
	std::list<std::string> erase_keys;
	for(typename sync_cache_type::value_type& entry : sync_cache_)
	{
		if(std::get<1>(entry).empty())
			erase_keys.push_back(std::get<0>(entry));
		else
		{
			node_info& top = std::get<1>(entry).front();
			switch(top.state)
			{
			case node_info::dirty:
				//determine the current version and fire an update
				if(client_.exists(encode_path(std::get<0>(entry))))
				{
					auto version_info = client_[encode_path(std::get<0>(entry))];
					handle_request(raft::request::Update{id_,
								encode_path(std::get<0>(entry)),
								std::get<0>(version_info),
								top.version}, std::get<1>(entry));
				}
				else //recover to novel
					handle_request(raft::request::Add{id_,
								encode_path(std::get<0>(entry)),
								top.version}, std::get<1>(entry));
				break;

			case node_info::novel:
				//if there're no rename markers, fire an add
				if(top.rename_info)
					tick_handle_rename(top, std::get<1>(entry));
				else
					handle_request(raft::request::Add{id_,
								encode_path(std::get<0>(entry)),
								top.version}, std::get<1>(entry));
				break;

			case node_info::dead:
				//if there're no rename markers, fire a delete
				if(top.rename_info)
					tick_handle_rename(top, std::get<1>(entry));
				else
					handle_request(raft::request::Delete{id_,
								encode_path(std::get<0>(entry)),
								top.version}, std::get<1>(entry));
				break;

			default:
				BOOST_LOG_TRIVIAL(warning) << "Unexpected state in sync cache: "
					<< top.state;
			}
		}
	}

	//Clean up empty queues
	for(const std::string& key : erase_keys)
		sync_cache_.erase(key);
}

template <typename Client, typename ChangeTx>
template <typename Rpc>
bool dfs::basic_state<Client, ChangeTx>::dcache_conflict(const Rpc& /*rpc*/, const boost::filesystem::path& path,
		ordinary_prepare_tag)
{
	bool conflict = false;
	if(dcache_.count(path.parent_path().string()))
	{
		auto ni_it = boost::range::find_if(dcache_.at(path.parent_path().string()),
				check_name(path.filename().string()));

		if(ni_it != dcache_.at(path.parent_path().string()).end())
		{
			conflict = ni_it->state == node_info::active_read
					|| ni_it->state == node_info::active_write;
		}
	}

	return conflict;
}

template <typename Client, typename ChangeTx>
template <typename Rpc>
bool dfs::basic_state<Client, ChangeTx>::dcache_conflict(const Rpc& rpc, const boost::filesystem::path& path,
		rename_prepare_tag)
{
	//create add & delete RPCs and check dcache conflicts with those
	raft::request::Add add(rpc.from(), rpc.new_key(),
			rpc.version());

	raft::request::Delete del(rpc.from(), rpc.key(),
			rpc.version());

	return dcache_conflict(add, decode_path(rpc.new_key()),
			typename rpc_traits<raft::request::Add>::prepare_tag{})
		|| dcache_conflict(del, path,
			typename rpc_traits<raft::request::Delete>::prepare_tag{});
}


template <typename Client, typename ChangeTx>
template <typename Rpc>
bool dfs::basic_state<Client, ChangeTx>::conflict_check_required(const Rpc& rpc, const boost::filesystem::path& path)
{
	bool conflict = false;
	//look for this action in the sync cache
	if(sync_cache_.count(path.string()))
	{
		auto ni = sync_cache_.at(path.string()).front();
		conflict = !rpc_traits<Rpc>::completed(rpc, ni);
		if(!conflict)
			//clean up the completed item
			rpc_traits<Rpc>::cleanup(rpc, ni, sync_cache_);
	}
	else
		conflict = dcache_conflict(rpc, path, typename rpc_traits<Rpc>::prepare_tag{});

	return conflict;
}


template <typename Client, typename ChangeTx>
template <typename Rpc>
void dfs::basic_state<Client, ChangeTx>::manage_sync_cache(const Rpc& rpc, const boost::filesystem::path& path,
		const boost::filesystem::path& recovery)
{
	//check the sync cache & recover if necessary
	if(sync_cache_.count(path.string()))
	{ //if we're here, it's because the sync cache clashes
		node_info& ni = sync_cache_[path.string()].front();
		if(ni.state == node_info::dead)
		{
			//Handle rename markers
			if(ni.rename_info)
			{
				//go change the other end of this rename marker into a
				//new. dcache will be handled later
				if(sync_cache_.count(*ni.rename_info))
				{
					assert(!sync_cache_[*ni.rename_info].empty());

					//find the other marker
					//The other side may not be first
					auto other = boost::range::find_if(sync_cache_.at(*ni.rename_info),
							[&ni](const node_info& value)
							{
								return value.state == node_info::novel
									&& value.rename_info
									&& *value.rename_info == ni.name;
							});

					if(other != sync_cache_.at(*ni.rename_info).end())
						other->rename_info = boost::none;
					else
						BOOST_LOG_TRIVIAL(warning) << "Malformed rename targets in sync cache for "
							<< path.string();
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Dangling rename from-marker in sync cache for "
						<< path.string();
			}

			//remove the delete
			sync_cache_.at(path.string()).pop_front();

			if(sync_cache_.at(path.string()).empty())
				sync_cache_.erase(path.string());
			else // conflict handle the remaining entries
				manage_sync_cache(rpc, path, recovery);
		}
		else
		{
			//handle rename markers
			if(ni.state == node_info::novel && ni.rename_info)
			{
				//go change the other end of this rename marker into a
				//new. dcache will be handled later
				if(sync_cache_.count(*ni.rename_info))
				{
					assert(!sync_cache_[*ni.rename_info].empty());

					//find the other marker
					//The other side may not be first
					auto other = boost::range::find_if(sync_cache_.at(*ni.rename_info),
							[&ni](const node_info& value)
							{
								return value.state == node_info::dead
									&& value.rename_info
									&& *value.rename_info == ni.name;
							});

					if(other != sync_cache_.at(*ni.rename_info).end())
						other->rename_info = recovery.string();
					else
						BOOST_LOG_TRIVIAL(warning) << "Malformed rename targets in sync cache for "
							<< path.string();
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Dangling rename to-marker in sync cache for "
						<< path.string();
			}

			if(ni.state == node_info::dirty
					|| ni.state == node_info::novel)
				ni.state = node_info::novel; //'cos it's new!
			else
				BOOST_LOG_TRIVIAL(warning) << "Unexpected state in sync cache: " << ni.state;

			//move
			sync_cache_[recovery.string()].swap(sync_cache_.at(path.string()));

			//delete the sync for the previous key
			sync_cache_.erase(path.string());

			//update the cache's names & copy their versions
			for(node_info& ni : sync_cache_[recovery.string()])
			{
				ni.name = recovery.string();
				changetx_.copy(encode_path((path.parent_path() / ni.name).string()), ni.version,
						encode_path(recovery.string()));
			}
		}
	}

}

template <typename Client, typename ChangeTx>
template <typename Rpc>
void dfs::basic_state<Client, ChangeTx>::manage_dcache(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& recovery)
{
	auto parent = path.parent_path();

	auto existing = boost::range::find_if(dcache_[parent.string()],
			check_name(path.filename().string()));

	if(existing != dcache_[parent.string()].end())
	{
		switch(existing->state)
		{
		//if it's clean/pending there's no conflict
		case node_info::clean:
		case node_info::pending:
			dcache_[parent.string()].erase(existing);
			break;

			//active_write needs a move & tl
		case node_info::active_write:
			//add the translation and slide into the move case
			//below
			fusetl_[path.string()] = std::make_tuple(dcache, recovery.string());

			//if it's dirty or novel, move it (sync handled above)
		case node_info::dirty:
		case node_info::novel:

			//handle rename markers
			if(existing->state == node_info::novel && existing->rename_info)
			{
				boost::filesystem::path rename_path = *(existing->rename_info);
				//go change the other end of this rename marker into a
				//new. dcache will be handled later
				if(dcache_.count(rename_path.parent_path().string()))
				{
					auto other = boost::range::find_if(dcache_[rename_path.parent_path().string()],
							check_name(rename_path.filename().string()));

					if(other != dcache_[rename_path.parent_path().string()].end())
					{
						if(other->state == node_info::dead
								&& other->rename_info
								&& other->rename_info == path.string())
							other->rename_info = recovery.filename().string();
						else
							BOOST_LOG_TRIVIAL(warning) << "Malformed rename targets in sync cache for "
								<< path.string() << " and " << other->name;

					}
					else
						BOOST_LOG_TRIVIAL(warning) << "Dangling rename pointer in sync cache for "
							<< path.string();
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Dangling rename pointer in sync cache for "
						<< path.string();
			}

			//copy version
			if(existing->state == node_info::active_write)
			{
				if(existing->scratch_info)
					existing->scratch_info = changetx_.move(
							encode_path(recovery.string()), *existing->scratch_info);
				else
					BOOST_LOG_TRIVIAL(error) << "Node set to active_write with no scratch info.";
			}
			else
				changetx_.copy(encode_path((parent / existing->name).string()), existing->version,
						encode_path(recovery.string()));
			//move
			existing->name = recovery.filename().string();
			//fix state
			if(existing->state == node_info::dirty)
				existing->state = node_info::novel;
			break;

			//active_read needs an entry in the rcache and tl
		case node_info::active_read:
			rcache_[path.string()] = std::make_tuple(rpc.key(), existing->version);
			fusetl_[path.string()] = std::make_tuple(rcache, path.string());
			dcache_[parent.string()].erase(existing);
			break;

			//if it's dead, delete it after checking rename
		case node_info::dead:
			if(existing->rename_info)
			{
				boost::filesystem::path rename_path = *(existing->rename_info);
				//go change the other end of this rename marker into a
				//new. dcache will be handled later
				if(dcache_.count(rename_path.parent_path().string()))
				{
					auto other = boost::range::find_if(dcache_[rename_path.parent_path().string()],
							check_name(rename_path.filename().string()));

					if(other != dcache_[rename_path.parent_path().string()].end())
					{
						if(other->state == node_info::novel &&
								other->rename_info && other->rename_info == path.string())
							other->rename_info = boost::none;
						else
							BOOST_LOG_TRIVIAL(warning) << "Malformed rename targets in sync cache for "
								<< path.string() << " and " << other->name;

					}
					else
						BOOST_LOG_TRIVIAL(warning) << "Dangling rename pointer in sync cache for "
							<< path.string();
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Dangling rename pointer in sync cache for "
						<< path.string();
			}
			dcache_[parent.string()].erase(existing);
			break;

			//Shouldn't reach here
		default:
			BOOST_LOG_TRIVIAL(warning) << "Unhandled state in conflict recovery: "
				<< existing->state;
		}
	}
}


template <typename Client, typename ChangeTx>
template <typename Rpc>
bool dfs::basic_state<Client, ChangeTx>::prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
		const boost::filesystem::path& parent, ordinary_prepare_tag)
{
	bool conflict = false;
	//Ensure the parent exists
	if(dcache_.count(parent.string()) == 0)
	{
		//Make all of the directories along the path
		make_directories(parent.string());
		if(dfs::log_on_missing_parent<Rpc>::value)
			BOOST_LOG_TRIVIAL(warning) << "Parent missing for committed rpc: "
				"recovering, but this shouldn't happen";
	}
	else //check for completion and conflict
		conflict = conflict_check_required(rpc, path);

	//conflict manage
	if(conflict)
	{
		BOOST_LOG_TRIVIAL(info) << "Commit-conflict for local state of " << path;
		auto recovery = recover_path(path);

		manage_sync_cache(rpc, path, recovery);

		manage_dcache(rpc, path, recovery);

	}
	//want to clobber if there's no conflict
	return !conflict;
}


template <typename Client, typename ChangeTx>
template <typename Rpc>
bool dfs::basic_state<Client, ChangeTx>::prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
		const boost::filesystem::path& parent, rename_prepare_tag)
{
	boost::filesystem::path to_path = decode_path(rpc.new_key());

	bool conflict = false;
	//Ensure the parent exists
	if(dcache_.count(to_path.parent_path().string()) == 0)
	{
		//Make all of the directories along the path
		make_directories(parent.string());
	}

	//check the from parent
	if(dcache_.count(parent.string()) == 0)
		BOOST_LOG_TRIVIAL(warning) << "Parent missing for committed rpc: "
			"recovering, but this shouldn't happen";
		//but don't create it, because we don't care

	//check for completion and conflict
	conflict = conflict_check_required(rpc, path);

	//additionally check the to node
	if(!conflict && sync_cache_.count(to_path.string()) == 1)
	{
		if(!sync_cache_.at(to_path.string()).empty())
		{
			auto head_ni = sync_cache_.at(to_path.string()).front();
			conflict = !(head_ni.state == node_info::dead
					&& head_ni.version == rpc.version()
					&& head_ni.rename_info
					&& *head_ni.rename_info == path);
		}
		else
			sync_cache_.erase(to_path.string());
	}

	//conflict manage
	if(conflict)
	{
		BOOST_LOG_TRIVIAL(info) << "Commit-conflict for local state of " << path;
		auto recovery_add = recover_path(to_path);
		raft::request::Add add(rpc.from(), rpc.new_key(),
				rpc.version());

		manage_sync_cache(add, to_path, recovery_add);
		manage_dcache(add, to_path, recovery_add);

		auto recovery_del = recover_path(path);
		raft::request::Delete del(rpc.from(), rpc.key(),
				rpc.version());

		manage_sync_cache(del, path, recovery_del);
		manage_dcache(del, path, recovery_del);

	}
	else //clean out the from
	{
		boost::range::remove_erase_if(dcache_[path.parent_path().string()],
				[path](const node_info& value)
				{
					return value.name == path.filename().string();
				});
	}

	//want to clobber if there's no conflict
	return !conflict;
}

template <typename Client, typename ChangeTx>
template <typename Rpc>
void dfs::basic_state<Client, ChangeTx>::manage_commit(const Rpc& rpc)
{
	boost::filesystem::path path = decode_path(rpc.key());
	boost::filesystem::path parent = path.parent_path();

	//Conflict management
	bool clobber = prepare_apply(rpc, path, parent, typename rpc_traits<Rpc>::prepare_tag{});

	if(rpc_adds_entry<Rpc>::value)
	{
		std::string apply_key;
		node_info new_file;

		std::tie(apply_key, new_file) = rpc_traits<Rpc>::template generate<node_info>(rpc, path,
				parent, changetx_);

		//if there is no existing dcache entry, add it
		auto it = boost::range::find_if(dcache_[apply_key],
				check_name(new_file.name));

		if(it == dcache_[apply_key].end())
			dcache_[apply_key].push_back(new_file);
		else if(clobber)
			*it = new_file;
	}
	else
	{
		//totally not a hack...
		boost::range::remove_erase_if(dcache_[parent.string()],
				[this, &path, &rpc](const node_info& value)
				{
					return value.name == path.filename().string()
						&& ( value.state == node_info::dead
								|| value.state == node_info::clean)
						&& value.version == rpc.version();
				});
	}


	clean_directories(parent);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_update(const raft::request::Update& rpc)
{
	BOOST_LOG_TRIVIAL(trace) << "Filesystem handling update: " << rpc;
	manage_commit(rpc);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_delete(const raft::request::Delete& rpc)
{
	BOOST_LOG_TRIVIAL(trace) << "Filesystem handling delete: " << rpc;
	manage_commit(rpc);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_rename(const raft::request::Rename& rpc)
{
	BOOST_LOG_TRIVIAL(trace) << "Filesystem handling rename: " << rpc;
	manage_commit(rpc);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_add(const raft::request::Add& rpc)
{
	BOOST_LOG_TRIVIAL(trace) << "Filesystem handling add: " << rpc;
	manage_commit(rpc);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::notify_arrival(const std::string& key, const std::string& version)
{
	boost::filesystem::path path = decode_path(key);
	if(dcache_.count(path.parent_path().string()) == 1)
	{
		auto subject = boost::range::find_if(dcache_[path.parent_path().string()],
				[&path, &version](const node_info& value)
				{
					return value.name == path.filename()
						&& value.version == version;
				});

		if(subject != dcache_[path.parent_path().string()].end())
		{
			if(subject->state == node_info::pending)
				subject->state = node_info::clean;
			subject->previous_version = boost::none;
		}
	}
}

template <typename Client, typename ChangeTx>
bool dfs::basic_state<Client, ChangeTx>::exists(const boost::filesystem::path& path) const
{
	//check the translation table
	if(fusetl_.count(path.string()))
	{
		auto tl = fusetl_.at(path.string());
		//check the dcache
		if(std::get<0>(tl) == dcache)
			return exists(std::get<1>(tl));
		else if(std::get<0>(tl) == rcache)
			return rcache_.count(std::get<1>(tl)) == 1;
		else
		{
			BOOST_LOG_TRIVIAL(error) << "Unknown translation table direction: "
				<< std::get<0>(tl);

			return false;
		}

	}
	else if(dcache_.count(path.string()) == 0)
	{
		if(dcache_.count(path.parent_path().string()) == 1)
		{
			auto it = boost::find_if(dcache_.at(path.parent_path().string()),
					check_name(path.filename().string()));

			return it != dcache_.at(path.parent_path().string()).end();
		}
		else
			return false;
	}
	return true;
}

template <typename Client, typename ChangeTx>
bool dfs::basic_state<Client, ChangeTx>::in_rcache(const boost::filesystem::path& path) const
{
	if(fusetl_.count(path.string()))
	{
		auto tl = fusetl_.at(path.string());
		return std::get<0>(tl) == rcache
			&& rcache_.count(path.string());
	}
	return false;
}

template <typename Client, typename ChangeTx>
typename dfs::basic_state<Client, ChangeTx>::node_info&
	dfs::basic_state<Client, ChangeTx>::get(const boost::filesystem::path& path)
{
	//special case: root
	if(path == "/")
	{
		auto it = boost::find_if(dcache_["/"],
				check_name("."));

		if(it != dcache_["/"].end())
			return *it;
		else
			throw std::logic_error("Root entry does not exist.");
	}
	else
	{
		//check the translation table
		if(fusetl_.count(path.string()))
		{
			std::tuple<redirect_to, std::string>& tl = fusetl_.at(path.string());
			//check the dcache
			if(std::get<0>(tl) == dcache)
			{
				return get(std::get<1>(tl));
			}
			else if(std::get<0>(tl) == rcache)
			{
				throw std::logic_error("Entry is in the rcache");
			}
			else
			{
				BOOST_LOG_TRIVIAL(error) << "Unknown translation table direction: "
					<< std::get<0>(tl);

				throw std::runtime_error("Unknown translation table direction: "
						+ std::to_string(std::get<0>(tl)));
			}
		}
		else if(dcache_.count(path.string()) == 0)
		{
			if(dcache_.count(path.parent_path().string()) == 1)
			{
				auto it = boost::find_if(dcache_[path.parent_path().string()],
						check_name(path.filename().string()));

				if(it != dcache_[path.parent_path().string()].end())
					return *it;
				else
					throw std::logic_error("Entry does not exist: " + path.string());
			}
			else
				throw std::logic_error("Entry does not exist: " + path.string());
		}
		else
		{
			auto it = boost::find_if(dcache_[path.parent_path().string()],
					check_name("."));

			if(it != dcache_[path.parent_path().string()].end())
				return *it;
			else
				throw std::logic_error("Malformed directory missing '.': " + path.string());
		}
	}
}

template <typename Client, typename ChangeTx>
std::tuple<std::string, std::string>
	dfs::basic_state<Client, ChangeTx>::get_rcache(const boost::filesystem::path& path) const
{
	return rcache_.at(path.string());
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::rename_impl(const boost::filesystem::path& from,
		const boost::filesystem::path& to)
{
	if(in_rcache(from))
		return -EBUSY;

	if(!exists(to.parent_path()))
		return -ENOENT;

	node_info& node = get(from);

	if(node.type == node_info::file)
	{
		//check it's not dead
		if(node.state == node_info::dead)
			return -ENOENT;

		//if it exists, we need to do an update & delete
		if(exists(to))
		{
			//update to
			node_info& to_node = get(to);
			if(to_node.type == node_info::dir)
				return -EISDIR;

			if(node.state == node_info::pending)
			{
				if(node.previous_version)
					to_node.version = *node.previous_version;
				else //we're a pending add; not visible
					return -ENOENT;
			}
			else
				to_node.version = node.version;

			//copy the version
			changetx_.copy(encode_path(from.string()), node.version,
						encode_path(to.string()));

			//active write; setup new scratch after killing old one
			if(to_node.state == node_info::active_write)
			{
				//delete old scratch
				changetx_.kill(*to_node.scratch_info);

				//set up new scratch
				to_node.scratch_info = changetx_.open(
						encode_path(to.string()), node.version);
			}
			else if(to_node.state != node_info::active_read)
				to_node.state = node_info::dirty;
			//else nothing extra

			sync_cache_[to.string()].push_back(to_node);
			sync_cache_[to.string()].back().name = to.string();

			//delete from if it's not pending; otherwise it stays
			if(node.state != node_info::pending)
			{
				node.state = node_info::dead;
				sync_cache_[from.string()].push_back(node);
				sync_cache_[from.string()].back().name = from.string();
			}
		}
		else
		{
			//copy the version
			changetx_.copy(encode_path(from.string()), node.version,
						encode_path(to.string()));

			//perform the rename
			node_info to_node(to.filename().string(), node.version,
					true);

			//Handle the pending info
			if(node.state == node_info::pending)
			{
				if(node.previous_version)
					to_node.version = *node.previous_version;
				else //we're a pending add; not visible
					return -ENOENT;
			}
			else
				to_node.version = node.version;

			//set up rename state
			to_node.state = node_info::novel;
			to_node.rename_info = from.string();

			//add it to the dcache
			dcache_[to.parent_path().string()].push_back(to_node);

			//add it to the sync cache
			sync_cache_[to.string()].push_back(to_node);
			sync_cache_[to.string()].back().name = to.string();

			//handle the from marker
			node.state = node_info::dead;
			node.rename_info = to.string();

			sync_cache_[from.string()].push_back(node);
			sync_cache_[from.string()].back().name = from.string();
		}

		return 0;
	}
	else //directory
	{
		//check that if the to path exists, it's a directory and is empty
		if(exists(to))
		{
			if(in_rcache(to))
				return -ENOTDIR;

			node_info& to_node = get(to);

			if(to_node.type != node_info::dir)
				return -ENOTDIR;

			if(dcache_[to.string()].size() > 1)
				return -ENOTEMPTY;
		}
		else //create the to path if it does not exist
			make_directories(to);

		//recurse on directory contents
		for(const node_info& ni : dcache_[from.string()])
		{
			//Dead nodes: I want to delete them. They're in the sync cache; just
			//ignore them. Pending new nodes should be moved as normal (might need
			//extra checks though).
			if(ni.state != node_info::dead)
			{
				int retcode = rename_impl(from / ni.name, to / ni.name);
				if(retcode != 0)
					return retcode;
			}
		}

		//delete from path
		clean_directories(from);
	}

	return 0;
}


template <typename Client, typename ChangeTx>
std::string dfs::basic_state<Client, ChangeTx>::get_key(const boost::filesystem::path& path) const
{
	if(fusetl_.count(path.string()))
	{
		auto tl = fusetl_.at(path.string());
		if(std::get<0>(tl) == rcache)
			return std::get<0>(rcache_.at(std::get<1>(tl)));
		else
			return encode_path(std::get<1>(tl));
	}
	else
		return encode_path(path.string());
}


template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::getattr(const boost::filesystem::path& path, struct stat* stat_info)
{
	//initialise stat structure
	memset(stat_info, 0, sizeof(struct stat));
	if(exists(path))
	{
		stat_info->st_uid = uid_;
		stat_info->st_gid = gid_;
		stat_info->st_nlink = 1;

		try
		{
			if(in_rcache(path))
			{
				auto node = get_rcache(path);

				stat_info->st_mode = S_IFREG | 0644;
				stat_info->st_size = boost::filesystem::file_size(
						changetx_(std::get<0>(node), std::get<1>(node)));

				return 0;
			}
			else
			{
				auto node = get(path);

				if(node.type == node_info::dir)
				{
					stat_info->st_mode = S_IFDIR | 0755;
					stat_info->st_ino = node.inode;
					return 0;
				}
				else
				{
					stat_info->st_mode = S_IFREG | 0644;

					boost::optional<std::string> active_version = node.active_version();
					if(active_version)
					{
						stat_info->st_size = boost::filesystem::file_size(
								changetx_(get_key(path), *active_version));
					}
					else if(node.state == node_info::active_write
							&& node.scratch_info)
					{// use the scratch info

						boost::filesystem::path scratch_path = (*node.scratch_info)();
						if(boost::filesystem::exists(path))
							stat_info->st_size = boost::filesystem::file_size(
									path);
						else
							stat_info->st_size = 0;
					}
					else if(node.state == node_info::dead)
						return -ENOENT;
					else
					{
						BOOST_LOG_TRIVIAL(warning) << "Unexpected logic path in getattr";
						return -ENOENT;
					}

					return 0;
				}
			}
		}
		catch(std::exception& ex)
		{
			BOOST_LOG_TRIVIAL(error) << "Exception in getattr: " << ex.what();

			return -ENOENT;
		}
	}
	else
		return -ENOENT;

	return -ENOENT;
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::mkdir(const boost::filesystem::path& path, mode_t)
{
	if(exists(path))
		return -EEXIST;

	//check parent exists & is a directory
	if(exists(path.parent_path()))
	{
		auto parent_info = get(path.parent_path());
		if(parent_info.type != node_info::dir)
			return -ENOTDIR;

		//add directory
		make_directories(path);

		return 0;
	}
	else
		return -ENOENT;

	throw std::runtime_error("Not yet implemented");
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::rmdir(const boost::filesystem::path& path)
{
	//check exists and is a directory
	if(!exists(path))
		return -ENOENT;

	if(in_rcache(path))
		return -ENOTDIR;

	node_info& node = get(path);

	if(node.type != node_info::dir)
		return -ENOTDIR;

	if(dcache_.at(path.string()).size() > 1)
	{
		//check the number of untombstoned files
		auto it = boost::range::find_if(dcache_.at(path.string()),
				[](const node_info& value)
				{
					return value.name != "." && value.state != node_info::dead;
				});
		if(it != dcache_.at(path.string()).end())
			return -ENOTEMPTY;
	}

	//delete the directory
	dcache_.erase(path.string());

	//remove ../dir
	if(path.string() != "/")
		boost::range::remove_erase_if(dcache_[path.parent_path().string()],
				check_name(path.filename().string()));

	return 0;
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::unlink(const boost::filesystem::path& path)
{
	//check exists & is not a directory
	if(!exists(path))
		return -ENOENT;

	if(in_rcache(path))
		return -EBUSY;

	node_info& node = get(path);

	if(node.type == node_info::dir)
		return -EISDIR;

	if(node.state == node_info::active_read
			|| node.state == node_info::active_write)
		return -EBUSY;

	if(node.state == node_info::dead)
		return -ENOENT;

	//tombstone & queue delete
	node.state = node_info::dead;
	sync_cache_[path.string()].push_back(node);
	sync_cache_[path.string()].back().name = path.string();

	return 0;
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::create_impl(node_info& ni, const boost::filesystem::path& path,
		struct fuse_file_info *fi)
{
	ni.fds = 1;
	if((fi->flags & O_ACCMODE) == O_RDONLY)
	{
		BOOST_LOG_TRIVIAL(trace) << "Create read-only file: " << path;
		ni.state = node_info::active_read;
		auto si = changetx_.add(encode_path(path.string()));
		ni.version = changetx_.close(si);
	}
	else
	{
		BOOST_LOG_TRIVIAL(trace) << "Create read-write file: " << path;
		ni.state = node_info::active_write;
		ni.scratch_info = changetx_.add(encode_path(path.string()));
	}
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::create(const boost::filesystem::path& path,
		mode_t, struct fuse_file_info *fi)
{
	if(!exists(path.parent_path()))
		return -ENOTDIR;

	if(exists(path))
	{
		//get and modify it
		node_info& ni = get(path);
		if(ni.state == node_info::dead)
			create_impl(ni, path, fi);
		else
			return -EEXIST;
	}
	else
	{
		node_info ni;
		ni.type = node_info::file;
		ni.name = path.filename().string();
		ni.inode = 0;

		//Perform the add
		create_impl(ni, path, fi);

		//add to the dcache
		dcache_[path.parent_path().string()].push_back(ni);
	}

	return 0;
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::rename(const boost::filesystem::path& from,
		const boost::filesystem::path& to)
{
	//Child because recursion
	return rename_impl(from, to);
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::truncate(const boost::filesystem::path& path, off_t newsize)
{
	//check exists & is not a directory
	if(!exists(path))
		return -ENOENT;

	//! Doesn't seem to be an applicable error
	if(in_rcache(path))
		return -EIO;

	node_info& node = get(path);

	boost::optional<typename ChangeTx::scratch> si;
	bool our_si = false;
	if(node.state == node_info::active_write)
		si = node.scratch_info;
	else
	{
		our_si = true;
		si = changetx_.open(get_key(path), node.version);
	}

	boost::filesystem::path scratch_path = (*si)();

	//if the scratch doesn't exist, make it
	if(!boost::filesystem::exists(scratch_path))
		boost::filesystem::ofstream os(scratch_path);

	//want the system truncate
	if(::truncate(scratch_path.c_str(), newsize) != 0)
	{
		//delete the scratch
		if(our_si)
			changetx_.kill(*si);

		return -errno;
	}

	//don't want to commit if we're open
	if(node.state != node_info::active_write)
	{
		//commit the scratch
		node.version = changetx_.close(*si);
		node.scratch_info = boost::none;
		node.state = node_info::dirty;

		sync_cache_[path.string()].push_back(node);
		sync_cache_[path.string()].back().name = path.string();
	}

	return 0;
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::open(const boost::filesystem::path& path, fuse_file_info* fi)
{
	//Check the parent path exists
	if(!exists(path.parent_path()))
		return -ENOTDIR;

	//check it's not a directory
	if(dcache_.count(path.string()))
		return -EISDIR;

	//check if it's in the rcache, in which case writes are out
	if(in_rcache(path))
	{
		if((fi->flags & O_ACCMODE) == O_RDONLY)
			return -EACCES;
		else
			return 0;
	}

	if(exists(path))
	{

		node_info& node = get(path);
		++node.fds;
		//switch to active_{read,write}
		if((fi->flags & O_ACCMODE) == O_RDONLY)
		{
			if(node.state != node_info::active_write)
				node.state = node_info::active_read;
		}
		else
		{
			//check read/write
			if(!((fi->flags & O_ACCMODE) == O_RDWR
						|| (fi->flags & O_ACCMODE) == O_WRONLY))
				BOOST_LOG_TRIVIAL(warning) << "Error in reading open mode flags";
			//set up a scratch
			if(node.fds == 1 && node.state != node_info::active_write)
			{
				node.scratch_info = changetx_.open(get_key(path),
						node.version);

				node.state = node_info::active_write;
			}
		}
	}
	else
		return -ENOENT;

	return 0;
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::read(const boost::filesystem::path& path, char* buf, std::size_t size, off_t offset, fuse_file_info* /*fi*/)
{
	//check it's not in the readcache
	boost::filesystem::path true_path;
	if(in_rcache(path))
	{
		auto details = rcache_.at(path.string());
		true_path = changetx_(std::get<0>(details), std::get<1>(details));
	}
	else
	{ //check in active_{read,write}
		node_info& node = get(path);
		if(node.state == node_info::active_write
				&& node.scratch_info)
			true_path = (*node.scratch_info)();
		else if(node.state == node_info::active_read)
			true_path = changetx_(get_key(path),
					node.version);
		else
		{
			BOOST_LOG_TRIVIAL(error) << "Read called for "
			   << path << " which is in the wrong state: " << node.state;
			return -EBADF;
		}

	}

	boost::filesystem::ifstream is(true_path);
	is.seekg(offset);
	is.read(buf, size);

	if(is.bad())
		return -EIO;

	return is.gcount();
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::write(const boost::filesystem::path& path, const char* buf, std::size_t size, off_t offset, fuse_file_info* /*fi*/)
{
	node_info& node = get(path);
	if(node.state == node_info::active_write)
	{
		boost::filesystem::path target = (*node.scratch_info)();

		std::ios::openmode mode = std::ios::out | std::ios::binary;

		if(boost::filesystem::exists(target))//don't truncate if it exists
			mode |= std::ios::in;

		boost::filesystem::ofstream of((*node.scratch_info)(), mode);

		of.seekp(offset);
		of.write(buf, size);

		if(of.bad())
		{
			BOOST_LOG_TRIVIAL(warning) << "Bad file during write";
			return -EIO;
		}

		return size;
	}
	else
	{
		BOOST_LOG_TRIVIAL(warning) << "File " << path
			<< " in wrong state for write: " << node.state;
		return -EBADF;
	}
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::release(const boost::filesystem::path& path, fuse_file_info* /*fi*/)
{
	if(in_rcache(path))
	{
		rcache_.erase(path.string());
		if(fusetl_.count(path.string()))
			fusetl_.erase(path.string());

		return 0;
	}

	node_info& node = get(path);
	if(node.fds == 0)
		BOOST_LOG_TRIVIAL(warning) << "Release on file " << path
			<< " which has no (tracked) open file handlers";

	if(node.fds == 0 || --node.fds == 0)
	{
		if(node.state == node_info::active_read)
		{
			//restore state
			if(!client_.exists(get_key(path)))
				node.state = node_info::novel;
			else
			{
				auto version_info = client_[encode_path(path.string())];
				if(std::get<0>(version_info) == node.version)
				{
					if(node.previous_version && !changetx_.exists(encode_path(path.string()),
								node.version))
						node.state = node_info::pending;
					else
						node.state = node_info::clean;
				}
				else
					node.state = node_info::dirty;
			}
		}
		else if(node.state == node_info::active_write)
		{
			//close the scratch & set up the state for syncing
			node.version = changetx_.close(*node.scratch_info);
			if(!client_.exists(get_key(path)))
				node.state = node_info::novel;
			else
				node.state = node_info::dirty;

			//get the true path & add to sync cache
			boost::filesystem::path true_path;
			if(fusetl_.count(path.string()))
				true_path = std::get<1>(fusetl_[path.string()]);
			else
				true_path = path;

			sync_cache_[true_path.string()].push_back(node);
			sync_cache_[true_path.string()].back().name = true_path.string();
		}
		else
			BOOST_LOG_TRIVIAL(warning) << "File " << path
				<< " is in an invalid state for closing: " << node.state;

		if(fusetl_.count(path.string()))
			fusetl_.erase(path.string());
	}

	return 0;
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::readdir(const boost::filesystem::path& path, void *buf,
		fuse_fill_dir_t filler, off_t /*offset*/,
		struct fuse_file_info* /*fi*/)
{
	//check the directory exists
	if(!dcache_.count(path.string()))
		return -ENOENT;

	std::list<node_info>& entries = dcache_[path.string()];
	for(const node_info& ni : entries)
	{
		//Don't show dead entries or those that are new & pending
		if(ni.state != node_info::dead
				&& !(ni.state == node_info::pending && !ni.previous_version))
				//second condition refers to novel pending
		{
			filler(buf, ni.name.c_str(), NULL, 0);
			if(ni.name == ".")
				filler(buf, "..", NULL, 0);
		}
	}

	return 0;
}

template <typename Client, typename ChangeTx>
int dfs::basic_state<Client, ChangeTx>::flush(const boost::filesystem::path& /*path*/,
		struct fuse_file_info* /*fi*/)
{
	//Flush doesn't make much sense in the circumstance; just return 0.
	return 0;
}
