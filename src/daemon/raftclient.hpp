#pragma once

namespace raft
{
	namespace request
	{
		template <typename Derived>
		class Request
		{
		public:
			typedef Derived derived_type;
			typedef Derived& reference;
			typedef const Derived& const_reference;


		protected:
			Request(const std::string& from, const std::string& key)
				:from_(from),
				key_(key)
			{
			}

			Request(const Json::Value& root)
				:from_(checked_from_json<std::string>(root, "from")),
				key_(checked_from_json<std::string>(root, "key"))
			{
			}

			operator Json::Value() const
			{
				Json::Value root;
				root["from"] = from_;
				root["key"] = key_;

				return root;
			}

		public:

			bool operator==(const Request<Derived>& ot) const noexcept
			{
				return from_ == ot.from_ && key_ == ot.key_;
			}

			std::string from() const noexcept
			{
				return from_;
			}

			std::string key() const noexcept
			{
				return key_;
			}

			friend std::ostream& operator<<(std::ostream& os, const Derived& derived)
			{
				return os << json_help::write(derived);
			}

		protected:
			std::string from_;
			std::string key_;

			template <typename T>
			static T checked_from_json(const Json::Value& root, const std::string& key)
			{
				return json_help::checked_from_json<T>(root, key, "Bad json for raft request:");
			}
		};

		class Update : public Request<Update>
		{
		public:
			Update(const std::string& from, const std::string& key,
				   const std::string& old_version, const std::string& new_version);

			Update(const Json::Value& root);

			operator Json::Value() const;

			std::string old_version() const noexcept;
			//! Same as new_version; for compatibility
			std::string version() const noexcept;

			std::string new_version() const noexcept;

		protected:
			std::string old_version_;
			std::string new_version_;
		};

		template <typename Derived>
		class VersionRequest : public Request<Derived>
		{
		protected:
			VersionRequest(const std::string& from, const std::string& key,
					const std::string& version)
				:Request<Derived>::Request(from, key),
				version_(version)
			{
			}

			VersionRequest(const Json::Value& root)
				:Request<Derived>::Request(root),
				version_(Request<Derived>::template checked_from_json<std::string>(root, "version"))
			{
			}

			operator Json::Value() const
			{
				auto root = Request<Derived>::operator Json::Value();

				root["version"] = version_;

				return root;
			}

		public:
			std::string version() const noexcept
			{
				return version_;
			}

		protected:
			std::string version_;
		};

		class Delete : public VersionRequest<Delete>
		{
		public:
			Delete(const std::string& from, const std::string& key,
					const std::string& version);

			Delete(const Json::Value& root);

			operator Json::Value() const;

		};

		class Rename : public VersionRequest<Rename>
		{
		public:
			Rename(const std::string& from, const std::string& key,
					const std::string& new_key, const std::string& version);

			Rename(const Json::Value& root);

			operator Json::Value() const;

			std::string new_key() const noexcept;

		protected:
			std::string new_key_;
		};

		class Add : public VersionRequest<Add>
		{
		public:
			Add(const std::string& from, const std::string& key,
					const std::string& version);
			Add(const Json::Value& root);

			operator Json::Value() const;

		};

	}
}

class ClientHandlers
{
public:
	typedef std::function<void (const std::string&, const Json::Value&)> send_request_type;
	typedef std::function<void (const Json::Value&)> append_to_log_type;
	typedef std::function<boost::optional<std::string> (void)> leader_type;

	ClientHandlers(const send_request_type& send_request, const append_to_log_type& append_to_log,
			const leader_type& leader);

	//! For forwarding requests
	void send_request(const std::string& to, const Json::Value& request) const;

	//! For asking the state machine to append a request
	void append_to_log(const Json::Value& request) const;

	//! For retrieving the current leader
	boost::optional<std::string> leader() const;

protected:
	send_request_type send_request_;
	append_to_log_type append_to_log_;
	leader_type leader_;
};

//! Client interface to raft.
/*!
 *  This class is responsible for providing the key-value interface onto raft. It
 *  forwards requests to the leader's instance of this via the standard RPC
 *  network, which determines what operations conflict and which are allowed.
 *
 *  It also provides the latest committed version information for each key in the
 *  KVS. The values provided by these versions are elsewhere in the system.
 */
class RaftClient
{
public:
	//! Construct the client interface.
	/*!
	 *  \param id The ID of this node; for determining when we're leading.
	 *  \param handlers The callbacks for this class to communicate with other
	 *  bits of the system.
	 */
	RaftClient(const std::string& id,
			ClientHandlers& handlers);

	//! Make a request.
	/*!
	 *  If this is called on a follower, it is ignored if the change has already
	 *  been committed and otherwise forwarded to the leader (if known; if not
	 *  it's dropped).
	 *
	 *  If this is called on any node that doesn't know of a leader, it's dropped
	 *  silently.
	 *
	 *  If this is called on a leader, the request is checked for conflict with
	 *  the last uncommitted request for this key (or the last committed, if none
	 *  remain uncommitted). If there is no conflict, it's added to the Raft log;
	 *  otherwise it's ignored.
	 */
	template <typename Derived>
	void request(const Derived& request)
	{
		const boost::optional<std::string> leader_id = handlers_.leader();
		if(leader_id)
		{
			if(*leader_id == id_)
			{
				auto request_validity = valid(request);
				if(request_validity == request_valid)
				{
					BOOST_LOG_TRIVIAL(info) << "Request valid";
					apply_to(request, pending_version_map_);
					handlers_.append_to_log(request);
				}
				else if(request_validity == request_done)
					BOOST_LOG_TRIVIAL(info) << "Request done";
				else
					BOOST_LOG_TRIVIAL(info) << "Request invalid";
				//else ignore
			}
			else
			{
				pending_version_map_.clear();

				if(!done(request, version_map_))
					handlers_.send_request(*leader_id, request);
			}
		}
		//else ignore
	}

	//! Commit an entry
	/*!
	 *  This handler is called by the Raft machine when an entry is suitable for
	 *  application -- it's been replicated on a majority of nodes. There should
	 *  be no conflicts, but conflict checks are made anyway and this function
	 *  throws if they occur (for debug purposes, mainly).
	 *
	 *  The contents of the entry are applied to this instance's
	 *  last-known-version store.
	 */
	void commit_handler(const Json::Value& root);

	//! Check that a key exists.
	bool exists(const std::string& key) const noexcept;

	//! Access the last version for key, throwing if it does not exist.
	/*!
	 *  \returns A tuple giving the version information: the first element is the
	 *  version; the second is the requesting instance.
	 */
	std::tuple<std::string, std::string> operator [](const std::string& key) noexcept(false);

	//! Enum defining if a request is valid
	enum validity {request_invalid = 0, //!< The request conflicts with the log
		request_valid, //!< The request does not conflict
		request_done //!< The request has already been fulfilled
	};

	//! Checks if a request is valid
	/*!
	 *  \param request An instance of one of
	 *  raft::request::{Add,Delete,Rename,Update}
	 */
	template <typename Derived>
	validity valid(const Derived& request) const noexcept
	{
		validity commit_valid = valid(request, version_map_);
		validity pending_valid = valid(request, pending_version_map_);

		auto leader_id = handlers_.leader();
		if(leader_id && *leader_id == id_)
		{
			switch(pending_valid)
			{
			case request_valid:
			case request_done:
				return pending_valid;
				break;

			default:
				return commit_valid;
			}

		}
		else
			return commit_valid;
	}

protected:
	std::string id_;

	ClientHandlers& handlers_;
	typedef std::unordered_map<std::string,
			std::tuple<std::string, std::string>> version_map_type;

	//! Latest committed versions. 0: version 1: requesting node
	version_map_type version_map_;

	//! Leader only: latest versions awaiting commit. Same as version_map_
	version_map_type pending_version_map_;

	//! Helper for commit
	template <typename Derived>
	void commit_if_valid(const Derived& entry)
	{
		auto commit_valid = valid(entry, version_map_);
		if(commit_valid == request_invalid)
			throw std::runtime_error("Bad commit: conflicts");

		if(commit_valid == request_valid)
		{
			apply_to(entry, version_map_);

			//Remove the entry from pending if it's in there
			if(pending_version_map_.count(entry.key()) == 1 &&
					std::get<0>(pending_version_map_[entry.key()]) == entry.version())
				pending_version_map_.erase(entry.key());

		}
	}

	void apply_to(const raft::request::Update& update, version_map_type& version_map);
	void apply_to(const raft::request::Delete& update, version_map_type& version_map);
	void apply_to(const raft::request::Rename& update, version_map_type& version_map);
	void apply_to(const raft::request::Add& update, version_map_type& version_map);


	bool check_conflict(const raft::request::Update& update) const;
	bool check_conflict(const raft::request::Delete& del) const;
	bool check_conflict(const raft::request::Rename& rename) const;
	bool check_conflict(const raft::request::Add& add) const;

	//! Checks a request is valid against a map
	template <typename Derived>
	validity valid(const Derived& request, const version_map_type& version_map) const noexcept
	{
		try
		{
			if(done(request, version_map))
				return request_done;

			return check_conflict(request) ? request_invalid : request_valid;
		}
		catch(std::exception& ex)
		{
			BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::valid(). Ignoring. Details: "
				<< ex.what();
		}
		catch(...)
		{
			BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::valid(). Ignoring.";
		}
		return request_invalid;
	}

	//! Check if the request has already been applied in map
	/*!
	 *  \param request The request to check with
	 *  \param version_map The version map to check in
	 */
	bool done(raft::request::Update::const_reference request, const version_map_type& version_map) const noexcept;

	//! \overload
	bool done(raft::request::Delete::const_reference request, const version_map_type& version_map) const noexcept;

	//! \overload
	bool done(raft::request::Rename::const_reference request, const version_map_type& version_map) const noexcept;

	//! \overload
	bool done(raft::request::Add::const_reference request, const version_map_type& version_map) const noexcept;
};
