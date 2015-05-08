#pragma once

#include <unordered_map>
#include <boost/signals2.hpp>

namespace raft
{
	//! Client interface to raft.
	/*!
	 *  This class is responsible for providing the key-value interface onto raft. It
	 *  forwards requests to the leader's instance of this via the standard RPC
	 *  network, which determines what operations conflict and which are allowed.
	 *
	 *  It also provides the latest committed version information for each key in the
	 *  KVS. The values provided by these versions are elsewhere in the system.
	 */
	class Client
	{
	public:
		//! Handler structure for the Client.
		class Handlers
		{
		public:
			typedef std::function<void (const std::string&, const Json::Value&)> send_request_type;
			typedef std::function<void (const Json::Value&)> append_to_log_type;
			typedef std::function<boost::optional<std::string> (void)> leader_type;

			Handlers(const send_request_type& send_request, const append_to_log_type& append_to_log,
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

		//! Construct the client interface.
		/*!
		 *  \param id The ID of this node; for determining when we're leading.
		 *  \param handlers The callbacks for this class to communicate with other
		 *  bits of the system.
		 */
		Client(const std::string& id,
				Handlers& handlers);

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
						BOOST_LOG_TRIVIAL(info) << "Request " << request << " valid";
						apply_to(request, apply_pending);
						handlers_.append_to_log(request);
					}
					else if(request_validity == request_done)
						BOOST_LOG_TRIVIAL(info) << "Request " << request << " done";
					else
						BOOST_LOG_TRIVIAL(info) << "Request " << request << " invalid";
					//else ignore
				}
				else
				{
					pending_version_map_.clear();

					if(valid(request) != request_done)
					{
						BOOST_LOG_TRIVIAL(info) << "Forwarding request to leader: " << request;

						handlers_.send_request(*leader_id, request);
					}
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
			return request_traits<Derived>::valid(request, version_map_,
					pending_version_map_);
		}

		//! Connect a function to be called on commit of a new Update
		/*!
		 *  \param f A callable of function signature void (const std::string&
		 *  from, const std::string& key, *  const std::string& value)
		 */
		template <typename Callable>
		boost::signals2::connection connect_commit_update(Callable&& f)
		{
			return commit_update_.connect(std::forward<Callable>(f));
		}

		//! Connect a function to be called on commit of a new Rename
		template <typename Callable>
		boost::signals2::connection connect_commit_rename(Callable&& f)
		{
			return commit_rename_.connect(std::forward<Callable>(f));
		}

		//! Connect a function to be called on commit of a new Delete
		template <typename Callable>
		boost::signals2::connection connect_commit_delete(Callable&& f)
		{
			return commit_delete_.connect(std::forward<Callable>(f));
		}

		//! Connect a function to be called on commit of a new Add
		template <typename Callable>
		boost::signals2::connection connect_commit_add(Callable&& f)
		{
			return commit_add_.connect(std::forward<Callable>(f));
		}

	protected:
		std::string id_;

		Handlers& handlers_;
		typedef std::unordered_map<std::string,
				boost::optional<std::tuple<std::string, std::string>>> version_map_type;

		//! Latest committed versions. 0: version 1: requesting node
		version_map_type version_map_;

		//! Leader only: latest versions awaiting commit. Same as version_map_
		version_map_type pending_version_map_;

		boost::signals2::signal<void (const request::Update&)> commit_update_;
		boost::signals2::signal<void (const request::Rename&)> commit_rename_;
		boost::signals2::signal<void (const request::Delete&)> commit_delete_;
		boost::signals2::signal<void (const request::Add&)> commit_add_;

		//! Function to provide ad-hoc polymorphism to rpc commit notification,
		//! allowing correct notification in a templated function.
		void commit_notify(const request::Update& rpc);

		//! \overload
		void commit_notify(const request::Rename& rpc);

		//! \overload
		void commit_notify(const request::Delete& rpc);

		//! \overload
		void commit_notify(const request::Add& rpc);

		//! Helper for commit
		template <typename Derived>
		void commit_if_valid(const Derived& entry)
		{
			auto commit_valid = request_traits<Derived>::valid(entry,
					version_map_);

			if(commit_valid == request_invalid)
				throw std::runtime_error("Bad commit: conflicts");

			if(commit_valid == request_valid)
			{
				apply_to(entry, apply_main);
			}

			//Remove the entry from pending if it's in there
			if(pending_version_map_.count(entry.key()) == 1 &&
					std::get<0>(*pending_version_map_[entry.key()]) == entry.version())
				pending_version_map_.erase(entry.key());

			//Notify the commit handlers
			commit_notify(entry);
		}

		//! Specifies apply target to apply_to
		enum apply_target {
			apply_main, //!< Apply to the main version map
			apply_pending //!< Apply to the pending version map
		};

		version_map_type& fetch_map(apply_target target);
		static bool exists_map(const std::string& key, const version_map_type& map) noexcept;

		void apply_to(const raft::request::Update& update, apply_target target);
		void apply_to(const raft::request::Delete& update, apply_target target);
		void apply_to(const raft::request::Rename& update, apply_target target);
		void apply_to(const raft::request::Add& update, apply_target target);

		template <typename Request>
		struct request_traits;
	};

	template <>
	struct Client::request_traits<raft::request::Update>
	{
		typedef raft::request::Update rpc_type;

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map,
				const version_map_type& pendng_map);

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map);

	private:
		static validity valid_impl(const rpc_type& rpc,
				const version_map_type& version_map);
	};

	template <>
	struct Client::request_traits<raft::request::Delete>
	{
		typedef raft::request::Delete rpc_type;

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map,
				const version_map_type& pendng_map);

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map);

	private:
		static validity valid_impl(const rpc_type& rpc,
				const version_map_type& version_map);
	};

	template <>
	struct Client::request_traits<raft::request::Rename>
	{
		typedef raft::request::Rename rpc_type;

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map,
				const version_map_type& pendng_map);

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map);

	private:
		static boost::optional<std::string> most_recent(
				const std::string& key,
				const version_map_type& version_map,
				const version_map_type& pending_map);
	};

	template <>
	struct Client::request_traits<raft::request::Add>
	{
		typedef raft::request::Add rpc_type;

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map,
				const version_map_type& pendng_map);

		static validity valid(const rpc_type& rpc,
				const version_map_type& version_map);
	};
}
