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

			bool operator==(const Request<Derived>& ot) const
			{
				return from_ == ot.from_ && key_ == ot.key_;
			}

			std::string from() const
			{
				return from_;
			}

			std::string key() const
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
			T checked_from_json(const Json::Value& root, const std::string& key) const
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

			std::string old_version() const;
			std::string new_version() const;

		protected:
			std::string old_version_;
			std::string new_version_;
		};

		class Delete : public Request<Delete>
		{
		public:
			Delete(const std::string& from, const std::string& key,
					const std::string& version);

			Delete(const Json::Value& root);

			operator Json::Value() const;

			std::string version() const;

		protected:
			std::string version_;
		};

		class Rename : public Request<Rename>
		{
		public:
			Rename(const std::string& from, const std::string& key,
					const std::string& new_key, const std::string& version);

			Rename(const Json::Value& root);

			operator Json::Value() const;

			std::string new_key() const;
			std::string version() const;

		protected:
			std::string new_key_;
			std::string version_;
		};

		class Add : public Request<Add>
		{
		public:
			Add(const std::string& from, const std::string& key,
					const std::string& version);
			Add(const Json::Value& root);

			operator Json::Value() const;

			std::string version() const;

		protected:
			std::string version_;
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
		throw std::runtime_error("Not yet implemented");
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

protected:
	std::string id_;

	ClientHandlers& handlers_;

	std::unordered_map<std::string,
		std::tuple<std::string, std::string>> version_map_;
};
