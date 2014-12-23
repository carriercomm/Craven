#include <string>
#include <vector>
#include <functional>
#include <fstream>

#include <json/json.h>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include "raftrpc.hpp"
#include "raftstate.hpp"

rpc_handlers::rpc_handlers(const append_entries_type& append_entries,
		const request_vote_type& request_vote, const timeout_type& request_timeout)
	:append_entries_(append_entries),
	request_vote_(request_vote),
	request_timeout_(request_timeout)
{
}

void rpc_handlers::append_entries(const std::string& endpoint, const raft_rpc::append_entries& rpc)
{
	append_entries_(endpoint, rpc);
}

void rpc_handlers::request_vote(const std::string& endpoint, const raft_rpc::request_vote& rpc)
{
	request_vote_(endpoint, rpc);
}

RaftState::RaftState(const std::string& id, const std::vector<std::string>& nodes,
		const std::string& log_file, const rpc_handlers& handlers)
	:id_(id),
	nodes_(nodes),
	log_(log_file),
	state_(follower_state),
	handlers_(handlers)
{
	throw std::runtime_error("Not yet implemented.");
}

void RaftState::timeout()
{
	throw std::runtime_error("Not yet implemented.");
}

RaftState::State RaftState::state() const
{
	throw std::runtime_error("Not yet implemented.");
}

std::tuple<uint32_t, bool> RaftState::append_entries(const raft_rpc::append_entries& rpc)
{
	throw std::runtime_error("Not yet implemented.");
}

void RaftState::append_entries_response(const std::string& from, uint32_t term, bool success)
{
	throw std::runtime_error("Not yet implemented.");
}


std::tuple<uint32_t, bool> RaftState::request_vote(const raft_rpc::request_vote& rpc)
{
	throw std::runtime_error("Not yet implemented.");
}

void RaftState::request_vote_response(const std::string& from, uint32_t term, bool voted)
{
	throw std::runtime_error("Not yet implemented.");
}

std::string RaftState::id() const
{
	throw std::runtime_error("Not yet implemented.");
}

std::vector<std::string> RaftState::nodes() const
{
	throw std::runtime_error("Not yet implemented.");
}

uint32_t RaftState::term() const
{
	throw std::runtime_error("Not yet implemented.");
}

boost::optional<std::string> RaftState::leader() const
{
	throw std::runtime_error("Not yet implemented.");
}

