#pragma once

#include <unordered_map>
#include <list>
#include <deque>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm_ext/erase.hpp>


#define FUSE_USE_VERSION 26
#include <fuse.h>


namespace dfs
{
	std::string encode_path(const std::string& path);
	std::string decode_path(const std::string& path);

	struct ordinary_prepare_tag
	{
	};
	struct rename_prepare_tag
	{
	};

	template <typename Client, typename ChangeTx>
	class basic_state
	{
	public:
		//! Node information stored in the dcache
		struct node_info
		{
			node_info() = default;

			//! Make a directory
			node_info(const std::string& name, uint64_t inode)
				:type(dir),
				state(clean),
				inode(inode),
				name(name)
			{
			}

			node_info(const std::string& name, const std::string& version, bool arrived)
				:type(file),
				state(arrived ? clean : pending),
				inode(0), //in the interests of consistency
				name(name),
				version(version)
			{
			}


			//! The type of the node, to save an extra lookup
			enum node_type {
				dir,	//!< A directory
				file	//!< A file
			};

			//! The state of the node
			/*!
			 *  If the node is active, its version is the version it was derived
			 *  from and the scratch information holds where it's being written
			 *  to. Once FUSE releases it, this should be committed and marked
			 *  dirty.
			 */
			enum state_type {
				clean,	//!< The node is clean -- the same as the sync'd version
				pending,//!< The node is pending -- the current version of this file hasn't arrived
				dirty,	//!< The node is dirty -- it requires syncing, but isn't active.
				active_write,	//!< The node is actively being written to.
				active_read,	//!< The node is actively being read from (i.e. read-only access).
				novel,	//!< The node is new.
				dead	//!< The node has been deleted but the delete is not yet synced
			};

			node_type type;
			state_type state;

			//! Inode number (must be unique, only meaningful on directories).
			uint64_t inode;

			//! Rename information.
			/*!
			 *  If state == dead && rename_info is valid, this is a `from' marker
			 *  that can be cleaned up on commit. If state == novel && rename_info
			 *  is valid, this is a `to' marker and can be set to clean with its
			 *  rename_info reset on commit.
			 */
			boost::optional<std::string> rename_info;

			//! Scratch info, for if we're active
			boost::optional<typename ChangeTx::scratch> scratch_info;

			//! Previous version, if we're pending
			boost::optional<std::string> previous_version;

			//! The name of the node
			/*!
			 *  In the dcache, this is the filename and the full path is implied.
			 *  Elsewhere, this is the full name.
			 */
			std::string name;

			//! The version of the node. Empty for novel files & directories.
			std::string version;
		};

		basic_state(Client& client, ChangeTx& changetx)
			:client_(client),
			changetx_(changetx),
			next_inode_(0)
		{
			//install handlers
			client_.connect_commit_update(std::bind(
						&basic_state<Client, ChangeTx>::commit_update,
						this, std::placeholders::_1));
			client_.connect_commit_delete(std::bind(
						&basic_state<Client, ChangeTx>::commit_delete,
						this, std::placeholders::_1));
			client_.connect_commit_rename(std::bind(
						&basic_state<Client, ChangeTx>::commit_rename,
						this, std::placeholders::_1));
			client_.connect_commit_add(std::bind(
						&basic_state<Client, ChangeTx>::commit_add,
						this, std::placeholders::_1));

			changetx_.connect_arrival_notifications(std::bind(
						&basic_state<Client, ChangeTx>::notify_arrival, this,
						std::placeholders::_1, std::placeholders::_2));
		}

		//! Handler for commit notification from the raft client.
		/*!
		 *  This function handles an incoming committed change to the filesystem.
		 *  It first checks to see if this change is one we've requested (if it
		 *  is, it marks those changes as synced), then checks to see if the
		 *  change conflicts before caching the information it provides & handling
		 *  any conflicts it causes.
		 */
		void commit_update(const raft::request::Update& rpc);

		//! \overload
		void commit_delete(const raft::request::Delete& rpc);

		//! \overload
		void commit_rename(const raft::request::Rename& rpc);

		//! \overload
		void commit_add(const raft::request::Add& rpc);

		//! Handler for arrival notifications from the change_transfer instance
		void notify_arrival(const std::string& key, const std::string& version);

		//! Get the attributes of a path
		int getattr(const std::string& path, struct stat* stat_info)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Make a directory at path.
		//! \returns 0 on success, a system-like error number otherwise.
		int mkdir(const std::string& path)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Remove a directory at path
		int rmdir(const std::string& path)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Remove a regular file
		int unlink(const std::string& path)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Rename a file or directory
		int rename(const std::string& from, const std::string& to)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Resize a file
		int truncate(const std::string& path, off_t newsize)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Open a file
		int open(const std::string& path, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Read from an open file
		int read(const std::string& path, char* buf, std::size_t size, off_t offset, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Write to an open file
		int write(const std::string& path, char* buf, std::size_t size, off_t offset, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Release -- close & commit an open file
		int release(const std::string& path, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Enum signifying where the translation table is pointing (public for
		//! test reasons)
		enum redirect_to {
			dcache,	//!< Look in the dcache
			rcache	//!< Look in the readcache
		};

	protected:
		//! Helper to ensure all parent directories exist for a path.
		void make_directories(const boost::filesystem::path& path)
		{
			boost::filesystem::path progress;
			for(auto piece : path)
			{
				progress /= piece;
				if(dcache_.count(progress.string()) == 0)
				{
					//add it, it/. and ../it
					node_info piece_info{piece.string(), next_inode_++};

					//../it
					if(progress.string() != "/")
						dcache_[progress.parent_path().string()].push_back(piece_info);

					piece_info.name = ".";

					//it & it/.
					dcache_[progress.string()] = {piece_info};
				}
			}
		}

		//! Helper to clean up empty directories along a path
		void clean_directories(boost::filesystem::path path)
		{
			for(; path != "/"; path = path.parent_path())
			{
				if(dcache_.count(path.string()) == 1)
				{

					if(dcache_[path.string()].size() == 1)
					{
						//Remove the directory marker
						dcache_.erase(path.string());

						//Remove the '../dir' marker
						if(dcache_.count(path.parent_path().string()))
						{
							boost::range::remove_erase_if(
									dcache_[path.parent_path().string()],
									[path](const node_info& value) -> bool
									{
										return value.name == path.filename().string();
									});
						}

					}
					else //none above
						break;
				}
			}
		}

		//! Alter the given path for conflict management
		boost::filesystem::path recover_path(boost::filesystem::path path) const
		{
			return path += boost::filesystem::unique_path(".%%%%-%%%%");
		}

		//! Ensures everything is set up properly and checks if a conflict check
		//! is required.
		template <typename Rpc>
		bool conflict_check_required(const Rpc& rpc, const boost::filesystem::path& path);

		//! Handles conflict management for the sync cache
		template <typename Rpc>
		void manage_sync_cache(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& recovery);

		template <typename Rpc>
		void prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& parent, ordinary_prepare_tag);

		template <typename Rpc>
		void prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& parent, rename_prepare_tag);

		template <typename Rpc>
		void manage_dcache(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& recovery);

		template <typename Rpc>
		void manage_commit(const Rpc& rpc);

		Client& client_;
		ChangeTx& changetx_;


		//! Functor to check for the name of a node_info
		class check_name
		{
		public:
			check_name(const std::string& name)
				:name_(name)
			{
			}

			bool operator()(const node_info& value)
			{
				return value.name == name_;
			}

		protected:
			std::string name_;
		};



		//! Directory cache. Single map of list so we can represent empty dirs
		std::unordered_map<std::string, std::list<node_info>> dcache_;

		//! Read cache. Used for storing the key & version of files opened for
		//! reading when updates came in. Cleaned up when those files are released
		std::unordered_map<std::string, std::tuple<std::string, std::string>> rcache_;

		//! The latest allocated inode number. Increases monotonically.
		uint64_t next_inode_;


		//! Translation table for fuse -> raft
		std::unordered_map<std::string, std::tuple<redirect_to, std::string>> fusetl_;

		typedef std::unordered_map<std::string, std::deque<node_info>> sync_cache_type;
		//! Holds node_info entries that need syncing.
		/*!
		 *  The name entry of these nodes is the full path, not the truncated path
		 *  provided in the dcache.
		 *
		 *  The cache is stored as an associative array of queues because only the
		 *  first item of each queue needs to be sent to Raft (the rest will be
		 *  rejected). This also makes conflict management & completion detection
		 *  easier.
		 */
		sync_cache_type sync_cache_;
	};

	template <typename Rpc>
	struct rpc_traits;


	template <>
	struct rpc_traits<raft::request::Update>
	{
		typedef raft::request::Update rpc_type;
		typedef ordinary_prepare_tag prepare_tag;

		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			return ni.version == rpc.version()
				&& ni.state == node_info::dirty;
		}

		template<typename node_info>
		static void cleanup(const rpc_type&, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type& rpc,
				const boost::filesystem::path& path, const boost::filesystem::path& parent,
				const ChangeTx& changetx)
		{
			node_info ni{path.filename().string(), rpc.version(),
				changetx.exists(rpc.key(), rpc.version())};
			if(ni.state == node_info::pending)
				ni.previous_version = rpc.old_version();
			return std::make_tuple(parent.string(), ni);
		}

	};

	template <>
	struct rpc_traits<raft::request::Delete>
	{
		typedef raft::request::Delete rpc_type;
		typedef ordinary_prepare_tag prepare_tag;

		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			return ni.version == rpc.version()
				&& ni.state == node_info::dead
				&& !ni.rename_info;
		}

		template<typename node_info>
		static void cleanup(const rpc_type&, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type&,
				const boost::filesystem::path&, const boost::filesystem::path&,
				const ChangeTx&)
		{
			return std::make_tuple("", node_info{});
		}
	};

	template <>
	struct rpc_traits<raft::request::Rename>
	{
		typedef raft::request::Rename rpc_type;
		typedef rename_prepare_tag prepare_tag;

		//! Note that this isn't sufficient: need to check the other signpost
		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			return ni.version == rpc.version()
				&& ni.state == node_info::dead
				&& static_cast<bool>(ni.rename_info)
				&& *ni.rename_info == decode_path(rpc.new_key());
		}

		template<typename node_info>
		static void cleanup(const rpc_type& rpc, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			std::string new_path = decode_path(rpc.new_key());
			//find the other end of the signpost
			if(sync_cache.count(new_path) && ni.rename_info)
			{
				//The other side may not be first
				auto other = boost::range::find_if(sync_cache.at(new_path),
						[&ni](const node_info& value)
						{
							return value.state == node_info::novel
								&& value.rename_info
								&& *value.rename_info == ni.name;
						});

				if(other != sync_cache.at(new_path).end())
				{
					if(other->rename_info &&
							*(other->rename_info) == ni.name)
						sync_cache.at(new_path).erase(other);
					else
						BOOST_LOG_TRIVIAL(warning) << "Malformed rename targets in sync cache for "
							<< ni.name << " and " << other->name;
				}
				else
					BOOST_LOG_TRIVIAL(warning)
						<< "Dangling rename pointer for path " << ni.name;
			}
			else
				BOOST_LOG_TRIVIAL(warning)
					<< "Dangling rename pointer for path " << ni.name;

			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type& rpc,
				const boost::filesystem::path&, const boost::filesystem::path&,
				const ChangeTx& changetx)
		{
			boost::filesystem::path to_path = decode_path(rpc.new_key());
			return std::make_tuple(to_path.parent_path().string(),
				node_info{to_path.filename().string(), rpc.version(),
				changetx.exists(rpc.new_key(), rpc.version())});
		}
	};

	template <>
	struct rpc_traits<raft::request::Add>
	{
		typedef raft::request::Add rpc_type;
		typedef ordinary_prepare_tag prepare_tag;

		//! Check if the given RPC matches the given node info (keys should
		//! already have been checked).
		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			if(ni.version == rpc.version())
			{
				//if it's marked as dirty, we'll recover but log the
				//anomaly
				if(ni.state != node_info::novel)
					BOOST_LOG_TRIVIAL(warning) << "Invalid state on completed add entry for path "
						<< ni.name << ": " << ni.state;

				return true;
			}
			return false;
		}

		template<typename node_info>
		static void cleanup(const rpc_type&, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type& rpc,
				const boost::filesystem::path& path, const boost::filesystem::path& parent,
				const ChangeTx& changetx)
		{
			return std::make_tuple(parent.string(),
					node_info{path.filename().string(), rpc.version(),
				changetx.exists(rpc.key(), rpc.version())});
		}

	};

	template <typename Rpc>
	struct log_on_missing_parent
	{
		enum {value = 1};
	};

	template <>
	struct log_on_missing_parent<raft::request::Add>
	{
		enum {value = 0};
	};

	template <typename Rpc>
	struct rpc_adds_entry
	{
		enum {value = 1};
	};

	template <>
	struct rpc_adds_entry<raft::request::Delete>
	{
		enum {value = 0};
	};
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
	else //conflict check
		conflict = true;

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

		//Should be true; worth checking that
		assert(!sync_cache_[path.string()].empty());

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
					BOOST_LOG_TRIVIAL(warning) << "Dangling rename pointer in sync cache for "
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
					BOOST_LOG_TRIVIAL(warning) << "Dangling rename pointer in sync cache for "
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
void dfs::basic_state<Client, ChangeTx>::prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
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
		auto recovery = recover_path(path);

		manage_sync_cache(rpc, path, recovery);

		manage_dcache(rpc, path, recovery);
	}
}


template <typename Client, typename ChangeTx>
template <typename Rpc>
void dfs::basic_state<Client, ChangeTx>::prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
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

	//conflict manage
	if(conflict)
	{
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
}

template <typename Client, typename ChangeTx>
template <typename Rpc>
void dfs::basic_state<Client, ChangeTx>::manage_commit(const Rpc& rpc)
{
	boost::filesystem::path path = decode_path(rpc.key());
	boost::filesystem::path parent = path.parent_path();

	//Conflict management
	prepare_apply(rpc, path, parent, typename rpc_traits<Rpc>::prepare_tag());

	if(rpc_adds_entry<Rpc>::value)
	{
		std::string apply_key;
		node_info new_file;

		std::tie(apply_key, new_file) = rpc_traits<Rpc>::template generate<node_info>(rpc, path,
				parent, changetx_);

		//remove any existing entries
		boost::range::remove_erase_if(dcache_[apply_key],
				check_name(new_file.name));

		dcache_[apply_key].push_back(new_file);
	}
	else
	{
		//totally not a hack...
		boost::range::remove_erase_if(dcache_[parent.string()],
				check_name(path.filename().string()));
	}


	clean_directories(parent);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_update(const raft::request::Update& rpc)
{
	manage_commit(rpc);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_delete(const raft::request::Delete& rpc)
{
	manage_commit(rpc);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_rename(const raft::request::Rename& rpc)
{
	manage_commit(rpc);
}

template <typename Client, typename ChangeTx>
void dfs::basic_state<Client, ChangeTx>::commit_add(const raft::request::Add& rpc)
{
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
						&& value.state == node_info::pending
						&& value.version == version;
				});

		if(subject != dcache_[path.parent_path().string()].end())
		{
			subject->state = node_info::clean;
			subject->previous_version = boost::none;
		}
	}
}
