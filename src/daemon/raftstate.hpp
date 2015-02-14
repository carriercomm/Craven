#pragma once

#include <unordered_map>

#include "raftlog.hpp"

namespace raft
{

	//! Class providing the volatile state & RPC handling for Raft
	class State
	{
	public:
		//! Class (so I can clean up the interface) providing RPC callbacks
		class Handlers
		{
		public:
			//! Specifies what timeout the state machine wants
			enum timeout_length : bool {
				leader_timeout, //!< Setup a timeout suitable for a leader
				election_timeout //!< Setup a timeout suitable to trigger a new election
			};

			typedef std::function<void (const std::string&, const raft::rpc::append_entries&)> append_entries_type;
			typedef std::function<void (const std::string&, const raft::rpc::request_vote&)> request_vote_type;
			typedef std::function<void (timeout_length)> timeout_type;
			typedef std::function<void (const Json::Value&)> commit_type;

			Handlers() = default;
			Handlers(const append_entries_type& append_entries, const
					request_vote_type& request_vote, const timeout_type& request_timeout,
					const commit_type& commit);

			void append_entries(const std::string& endpoint, const raft::rpc::append_entries& rpc);

			void request_vote(const std::string& endpoint, const raft::rpc::request_vote& rpc);

			void request_timeout(timeout_length length);

			void commit(const Json::Value& value);
		protected:
			append_entries_type append_entries_;
			request_vote_type request_vote_;
			timeout_type request_timeout_;
			commit_type commit_;
		};

		//! Constructor for the raft::State instance.
		/*!
		 *  \param id The ID of this node
		 *  \param nodes The list of nodes in the Raft system (not including this one).
		 *  \param log_file The path to the raft write-ahead log file
		 *  \param handlers A structure containing the RPC callbacks for this instance
		 *  to communicate with others (possibly over the network -- that detail is
		 *  hidden from this class). The callback additionally provide neat type
		 *  erasure, allowing untemplated testing.
		 *  \param transfer_limit The maximum number of logs to transfer in one
		 *  RPC
		 */
		State(const std::string& id, const std::vector<std::string>& nodes,
				const std::string& log_file, Handlers& handlers,
				uint32_t transfer_limit=50);

		//! Handler called on timeout.
		/*!
		 *  This is the handler called by the RPC provider when the timeout fires.
		 */
		void timeout();

		enum Status {follower_state, candidate_state, leader_state};

		//! Returns the type of Raft node this instance is currently being.
		Status state() const;

		//! AppendEntries RPC from the Raft paper.
		/*!
		 *  This function is used to signify to the raft::State instance that an
		 *  AppendEntries RPC has arrived.
		 *
		 *  \param rpc The RPC's arguments
		 *
		 *  \returns A tuple: the first element is the term of this node, the second
		 *  element is true if this node has an entry that matches prev_log_* -- i.e.
		 *  the logs are consistent up to that index.
		 */
		std::tuple<uint32_t, bool> append_entries(const raft::rpc::append_entries& rpc);

		//! The response handler for append_entries
		void append_entries_response(const std::string& from,
				const raft::rpc::append_entries_response& rpc);

		//! RequestVote RPC
		/*!
		 *  This function is used to signify to the raft::State instance that a
		 *  RequestVote RPC has arrived.
		 *
		 *  \param rpc The rpc's arguments
		 *
		 *  \returns A tuple: the first element is the term of this node (if the
		 *  candidate is out of date, it'll update itself with this); the second is
		 *  true if this node votes for the candidate.
		 */
		std::tuple<uint32_t, bool> request_vote(const raft::rpc::request_vote& rpc);

		//! The response handler for request_vote
		void request_vote_response(const std::string& from,
				const raft::rpc::request_vote_response& rpc);

		std::string id() const;

		std::vector<std::string> nodes() const;

		//! Retrieve the current term
		uint32_t term() const;

		//! Return the current leader, if their identity is known.
		/*!
		 *  \returns boost::none if the leader for this term is not known or has not
		 *  yet been elected (which is indistinguishable).
		 */
		boost::optional<std::string> leader() const;

		const Log& log() const;

		void append(const Json::Value& root);

	protected:
		const uint32_t transfer_limit_;
		const std::string id_;
		std::vector<std::string> nodes_;
		boost::optional<std::string> leader_;

		Log log_;

		Status state_;

		Handlers& handlers_;

		//volatile state on all servers
		uint32_t last_applied_;

		//volatile state on candidates
		std::set<std::string> votes_;

		//volatile state on leaders

		//! A map to each node's next_index and match_index
		std::unordered_map<std::string, std::tuple<uint32_t, uint32_t, bool>> client_index_;

		//! Helper function to apply all committed log entries
		void commit_available();

		//! Term update handler
		void term_update(uint32_t term);

		//! Checks to see if any more entries can be committed and does so.
		/*!
		 *  This function is only invoked while the machine is running as a leader;
		 *  otherwise the commit_index can only be updated by the term's leader.
		 */
		void check_commit();

		//! Performs a global heartbeat: up-to-date nodes are sent empty
		//! append_requests while others are sent the next batch of entries.
		void heartbeat();

		//! Performs a heartbeat at a single node.
		void heartbeat(const std::string& node);

		//! Handles transition to follower
		void transition_follower();

		//! Handles transition to candidate
		void transition_candidate();

		//! Handles transition to leader
		void transition_leader();

		//! Calculates the number of nodes required for a majority
		uint32_t calculate_majority();
	};
}
