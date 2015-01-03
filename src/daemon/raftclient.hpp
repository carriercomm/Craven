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

		//! Connect a function to be called on commit of a new key/version
		/*!
		 *  \param f A callable of function signature void (const std::string&
		 *  from, const std::string& key, *  const std::string& value)
		 */
		template <typename Callable>
		boost::signals2::connection connect_commit(Callable&& f)
		{
			return commit_.connect(std::forward<Callable>(f));
		}

	protected:
		std::string id_;

		Handlers& handlers_;
		typedef std::unordered_map<std::string,
				std::tuple<std::string, std::string>> version_map_type;

		//! Latest committed versions. 0: version 1: requesting node
		version_map_type version_map_;

		//! Leader only: latest versions awaiting commit. Same as version_map_
		version_map_type pending_version_map_;

		boost::signals2::signal<void (const std::string&, const std::string&, const std::string&)> commit_;

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

				//Notify the commit handlers
				commit_(entry.from(), entry.key(), entry.version());

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
				BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::valid(). Ignoring. Details: "
					<< ex.what();
			}
			catch(...)
			{
				BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::valid(). Ignoring.";
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
}
