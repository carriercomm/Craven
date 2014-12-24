#include <string>
#include <vector>
#include <functional>
#include <fstream>

#include <json/json.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include "raftrpc.hpp"
#include "raftstate.hpp"

rpc_handlers::rpc_handlers(const append_entries_type& append_entries,
		const request_vote_type& request_vote, const timeout_type& request_timeout,
		const commit_type& commit)
	:append_entries_(append_entries),
	request_vote_(request_vote),
	request_timeout_(request_timeout),
	commit_(commit)
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

void rpc_handlers::request_timeout(uint32_t milliseconds)
{
	request_timeout_(milliseconds);
}

void rpc_handlers::commit(const Json::Value& value)
{
	commit_(value);
}

RaftState::RaftState(const std::string& id, const std::vector<std::string>& nodes,
		const std::string& log_file, const rpc_handlers& handlers)
	:id_(id),
	nodes_(nodes),
	log_(log_file, std::bind(&RaftState::term_update, this, std::placeholders::_1)),
	state_(follower_state),
	handlers_(handlers),
	commit_index_(0),
	last_applied_(0)
{
}

void RaftState::timeout()
{
	throw std::runtime_error("Not yet implemented.");
}

RaftState::State RaftState::state() const
{
	return state_;
}

std::tuple<uint32_t, bool> RaftState::append_entries(const raft_rpc::append_entries& rpc)
{
	//Stale
	if(rpc.term() < log_.term())
	{
		BOOST_LOG_TRIVIAL(info) << "Received stale append_entries request from " << rpc.leader_id();
		return std::make_tuple(log_.term(), false);
	}

	//We need to become a follower -- our term <= their term
	if(candidate_state == state_)
	{
		BOOST_LOG_TRIVIAL(info) << "Stepped down as candidate for term " << rpc.term()
			<< " deferring to " << rpc.leader_id() << ".";

		state_ = follower_state;
		//No term update -- leave that for the follower handling.

	} // no else because we need to deal with this as a follower

	//If later term, step down. Otherwise, log a warning (and step down).
	if(leader_state == state_)
	{
		//Shouldn't happen
		if(rpc.term() == log_.term())
			BOOST_LOG_TRIVIAL(error) << "Two leaders for term " << rpc.term() << ": "
				<< id_ << " and " << rpc.leader_id();

		state_ = follower_state;
	}

	if(follower_state == state_)
	{
		if(rpc.term() > log_.term())
		{
			raft_log::NewTerm nt(rpc.term());
			log_.write(nt);
			leader_ = rpc.leader_id();
		}

		//We've found the last consistent point in our log, so get adding.
		if(log_.match(rpc.prev_log_term(), rpc.prev_log_index()))
		{
			try
			{
				for(unsigned int i = 0; i < rpc.entries().size(); ++i)
				{
					std::tuple<uint32_t, Json::Value> entry = rpc.entries()[i];

					raft_log::LogEntry log_entry(std::get<0>(entry),
							//Indexes start from one after prev_log_index
							i + 1 + rpc.prev_log_index(),
							std::get<1>(entry));

					log_.write(log_entry);
				}
				BOOST_LOG_TRIVIAL(info) << "Added " << rpc.entries().size() << " log entries from "
					<< rpc.leader_id() << ".";
			}
			catch(std::runtime_error& ex)
			{
				BOOST_LOG_TRIVIAL(error) << "Unexpected failure on log append: " << ex.what();
				return std::make_tuple(log_.term(), false);
			}

			if(rpc.leader_commit() > commit_index_)
			{
				commit_index_ = rpc.leader_commit();
				commit_available();
			}

		}
		else
			return std::make_tuple(log_.term(), false);
	}

}

void RaftState::append_entries_response(const std::string& from,
		const raft_rpc::append_entries_response& rpc)
{
	if(rpc.term() == log_.term())
	{
		if(leader_state == state_)
		{

		}
		else
			BOOST_LOG_TRIVIAL(info) << "Ingoring append_entries response because we're not leading";
	}
}


std::tuple<uint32_t, bool> RaftState::request_vote(const raft_rpc::request_vote& rpc)
{
	throw std::runtime_error("Not yet implemented.");
}

void RaftState::request_vote_response(const std::string& from,
		const raft_rpc::request_vote_response& rpc)
{
	throw std::runtime_error("Not yet implemented.");
}

std::string RaftState::id() const
{
	return id_;
}

std::vector<std::string> RaftState::nodes() const
{
	return nodes_;
}

uint32_t RaftState::term() const
{
	return log_.term();
}

boost::optional<std::string> RaftState::leader() const
{
	return leader_;
}

void RaftState::commit_available()
{
	throw std::runtime_error("Not yet implemented.");
}

void RaftState::term_update(uint32_t term)
{
	throw std::runtime_error("Not yet implemented.");
}

