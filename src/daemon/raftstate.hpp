#pragma once

#include "raftlog.hpp"

//! Class (so I can clean up the interface) providing RPC callbacks
class rpc_handlers
{
public:
	typedef std::function<void (const std::string&, uint32_t, const std::string&, uint32_t, uint32_t, const std::vector<std::string>&)> append_entries_type;
	typedef std::function<void (const std::string&, uint32_t, const std::string&, uint32_t, uint32_t)> request_vote_type;

	rpc_handlers(const append_entries_type& append_entries, const request_vote_type& request_vote);

	void append_entries(const std::string& node, uint32_t term, const std::string&
			leader_id, uint32_t prev_log_index, uint32_t prev_log_term,
			const std::vector<std::string>& entries);

	void request_vote(const std::string& node, uint32_t term, const
			std::string& candidate_id, uint32_t last_log_index, uint32_t last_log_term);
protected:
	append_entries_type append_entries_;
	request_vote_type request_vote_;
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
	RaftState(const std::vector<std::string>& nodes, const std::string& log_file, const rpc_handlers& handlers);

	//! Handler called on timeout.
	/*!
	 *  This is the handler called by the RPC provider when the timeout fires.
	 *
	 *  \returns An unsigned integer providing the length of the next timeout in
	 *  milliseconds; 0 means stop.
	 */
	uint32_t timeout();

	enum State {follower, leader, candidate};

	//! Returns the type of Raft node this instance is currently being.
	State state() const;

	//! AppendEntries RPC from the Raft paper.
	/*!
	 *  This function is used to signify to the RaftState instance that an
	 *  AppendEntries RPC has arrived.
	 *
	 *  \param term The leader's term
	 *  \param leader_id The ID of the leader
	 *  \param prev_log_index The index of the log entry immediately preceeding
	 *  the new ones sent in this RPC.
	 *  \param prev_log_term The term of the previous log entry, so this node can
	 *  verify its log is valid up to that index.
	 *  \param entries The log entries to append.
	 *
	 *  \returns A tuple: the first element is the term of this node, the second
	 *  element is true if this node has an entry that matches prev_log_* -- i.e.
	 *  the logs are consistent up to that index.
	 */
	std::tuple<uint32_t, bool> append_entries(uint32_t term, const std::string& leader_id,
			uint32_t prev_log_index, uint32_t prev_log_term,
			const std::vector<std::string>& entries);

	//! The response handler for append_entries
	void append_entries_response(uint32_t term, bool success);

	//! RequestVote RPC
	/*!
	 *  This function is used to signify to the RaftState instance that a
	 *  RequestVote RPC has arrived.
	 *
	 *  \param term The candidate's term
	 *  \param candidate_id The ID of the candidate
	 *  \param last_log_index The index of the candidate's last log entry -- for
	 *  this node to check the safety condition of voting.
	 *  \param last_log_term The term of the candidate's last log entry (same
	 *  reason).
	 *
	 *  \returns A tuple: the first element is the term of this node (if the
	 *  candidate is out of date, it'll update itself with this); the second is
	 *  true if this node votes for the candidate.
	 */
	std::tuple<uint32_t, bool> request_vote(uint32_t term, const std::string& candidate_id,
			uint32_t last_log_index, uint32_t last_log_term);

	//! The response handler for request_vote
	void request_vote(uint32_t term, bool voted);


protected:
	std::vector<std::string> nodes_;

	RaftLog log_;

	State state_;

	rpc_handlers handlers_;
};
