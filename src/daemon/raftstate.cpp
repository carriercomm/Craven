#include <string>
#include <vector>
#include <functional>

#include "raftstate.hpp"

rpc_handlers::rpc_handlers(const append_entries_type& append_entries, const request_vote_type& request_vote)
	:append_entries_(append_entries),
	request_vote_(request_vote)
{
}

void rpc_handlers::append_entries(const std::string& node, uint32_t term,
		const std::string& leader_id, uint32_t prev_log_index, uint32_t
		prev_log_term, const std::vector<std::string>& entries)
{
	append_entries_(node, term, leader_id, prev_log_index, prev_log_term, entries);
}

void rpc_handlers::request_vote(const std::string& node, uint32_t term, const
		std::string& candidate_id, uint32_t last_log_index, uint32_t last_log_term)
{
	request_vote_(node, term, candidate_id, last_log_index, last_log_term);
}

RaftState::RaftState(const std::vector<std::string>& nodes, const std::string& log_file,
		const rpc_handlers& handlers)
	:nodes_(nodes),
	log_(log_file),
	state_(follower),
	handlers_(handlers)
{
	throw std::runtime_error("Not yet implemented.");
}

uint32_t RaftState::timeout()
{
	throw std::runtime_error("Not yet implemented.");
}

State RaftState::state() const
{
	throw std::runtime_error("Not yet implemented.");
}

std::tuple<uint32_t, bool> RaftState::append_entries(uint32_t term, const std::string&
		leader_id, uint32_t prev_log_index, uint32_t prev_log_term, const
		std::vector<std::string>& entries)
{
	throw std::runtime_error("Not yet implemented.");
}

void RaftState::append_entries_response(uint32_t term, bool success)
{
	throw std::runtime_error("Not yet implemented.");
}


std::tuple<uint32_t, bool> RaftState::request_vote(uint32_t term, const std::string& candidate_id,
		uint32_t last_log_index, uint32_t last_log_term)
{
	throw std::runtime_error("Not yet implemented.");
}

void RaftState::request_vote(uint32_t term, bool voted)
{
	throw std::runtime_error("Not yet implemented.");
}
