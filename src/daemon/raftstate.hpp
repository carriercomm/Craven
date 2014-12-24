#pragma once

#include <unordered_map>

#include "raftlog.hpp"

//! Class (so I can clean up the interface) providing RPC callbacks
class rpc_handlers
{
public:
	typedef std::function<void (const std::string&, const raft_rpc::append_entries&)> append_entries_type;
	typedef std::function<void (const std::string&, const raft_rpc::request_vote&)> request_vote_type;
	typedef std::function<void (uint32_t milliseconds)> timeout_type;
	typedef std::function<void (const Json::Value&)> commit_type;

	rpc_handlers() = default;
	rpc_handlers(const append_entries_type& append_entries, const
			request_vote_type& request_vote, const timeout_type& request_timeout,
			const commit_type& commit);

	void append_entries(const std::string& endpoint, const raft_rpc::append_entries& rpc);

	void request_vote(const std::string& endpoint, const raft_rpc::request_vote& rpc);

	void request_timeout(uint32_t milliseconds);

	void commit(const Json::Value& value);
protected:
	append_entries_type append_entries_;
	request_vote_type request_vote_;
	timeout_type request_timeout_;
	commit_type commit_;
};

//! Class providing the volatile state & RPC handling for Raft
class RaftState
{
public:
	//! Constructor for the RaftState instance.
	/*!
	 *  \param nodes The list of nodes in the Raft system.
	 *  \param log_file The path to the raft write-ahead log file
	 *  \param handlers A structure containing the RPC callbacks for this instance
	 *  to communicate with others (possibly over the network -- that detail is
	 *  hidden from this class). The callback additionally provide neat type
	 *  erasure, allowing untemplated testing.
	 */
	RaftState(const std::string& id, const std::vector<std::string>& nodes,
			const std::string& log_file, const rpc_handlers& handlers);

	//! Handler called on timeout.
	/*!
	 *  This is the handler called by the RPC provider when the timeout fires.
	 */
	void timeout();

	enum State {follower_state, candidate_state, leader_state};

	//! Returns the type of Raft node this instance is currently being.
	State state() const;

	//! AppendEntries RPC from the Raft paper.
	/*!
	 *  This function is used to signify to the RaftState instance that an
	 *  AppendEntries RPC has arrived.
	 *
	 *  \param rpc The RPC's arguments
	 *
	 *  \returns A tuple: the first element is the term of this node, the second
	 *  element is true if this node has an entry that matches prev_log_* -- i.e.
	 *  the logs are consistent up to that index.
	 */
	std::tuple<uint32_t, bool> append_entries(const raft_rpc::append_entries& rpc);

	//! The response handler for append_entries
	void append_entries_response(const std::string& from,
			const raft_rpc::append_entries_response& rpc);

	//! RequestVote RPC
	/*!
	 *  This function is used to signify to the RaftState instance that a
	 *  RequestVote RPC has arrived.
	 *
	 *  \param rpc The rpc's arguments
	 *
	 *  \returns A tuple: the first element is the term of this node (if the
	 *  candidate is out of date, it'll update itself with this); the second is
	 *  true if this node votes for the candidate.
	 */
	std::tuple<uint32_t, bool> request_vote(const raft_rpc::request_vote& rpc);

	//! The response handler for request_vote
	void request_vote_response(const std::string& from,
			const raft_rpc::request_vote_response& rpc);

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

	void client_request(const Json::Value& action,
			const std::function<void (bool)>& result_callback);


protected:
	std::string id_;
	std::vector<std::string> nodes_;
	boost::optional<std::string> leader_;

	RaftLog log_;

	State state_;

	rpc_handlers handlers_;

	//volatile state on all servers
	uint32_t commit_index_;
	uint32_t last_applied_;

	//volatile state on leaders

	//! A map to each node's next_index and match_index
	std::unordered_map<std::string, std::tuple<uint32_t, uint32_t>> client_index_;

	//! Helper function to apply all committed log entries
	void commit_available();

	//! Term update handler
	void term_update(uint32_t term);
};
