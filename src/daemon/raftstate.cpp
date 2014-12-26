#include <string>
#include <vector>
#include <set>
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

void rpc_handlers::append_entries(const std::string& endpoint, const raft::rpc::append_entries& rpc)
{
	append_entries_(endpoint, rpc);
}

void rpc_handlers::request_vote(const std::string& endpoint, const raft::rpc::request_vote& rpc)
{
	request_vote_(endpoint, rpc);
}

void rpc_handlers::request_timeout(timeout_length length)
{
	request_timeout_(length);
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
	if(follower_state == state_ || candidate_state == state_)
	{
		BOOST_LOG_TRIVIAL(info) << "Broadcasting candidacy for new election term.";
		//update the term
		log_.write(log_.term() + 1);
		//Perform the transition
		transition_candidate();
		handlers_.request_timeout(rpc_handlers::election_timeout);
	}
	else
	{
		heartbeat();
		handlers_.request_timeout(rpc_handlers::leader_timeout);
	}
}

RaftState::State RaftState::state() const
{
	return state_;
}

std::tuple<uint32_t, bool> RaftState::append_entries(const raft::rpc::append_entries& rpc)
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

		transition_follower();
		//No term update -- leave that for the follower handling.

	} // no else because we need to deal with this as a follower

	//If later term, step down. Otherwise, log a warning (and step down).
	if(leader_state == state_)
	{
		//Shouldn't happen
		if(rpc.term() == log_.term())
			BOOST_LOG_TRIVIAL(error) << "Two leaders for term " << rpc.term() << ": "
				<< id_ << " and " << rpc.leader_id();

		transition_follower();
	}

	if(follower_state == state_)
	{
		//Reset the timeout
		handlers_.request_timeout(rpc_handlers::election_timeout);

		if(rpc.term() > log_.term())
		{
			raft::log::NewTerm nt(rpc.term());
			log_.write(nt);
		}

		if(!leader_)
			leader_ = rpc.leader_id();

		//We've found the last consistent point in our log, so get adding.
		if(log_.match(rpc.prev_log_term(), rpc.prev_log_index()))
		{
			try
			{
				for(unsigned int i = 0; i < rpc.entries().size(); ++i)
				{
					std::tuple<uint32_t, Json::Value> entry = rpc.entries()[i];

					raft::log::LogEntry log_entry(std::get<0>(entry),
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

			//Success!
			return std::make_tuple(log_.term(), true);
		}
		else
			return std::make_tuple(log_.term(), false);
	}

	//Shouldn't get here, but just in case
	return std::make_tuple(log_.term(), false);
}

void RaftState::append_entries_response(const std::string& from,
		const raft::rpc::append_entries_response& rpc)
{
	if(rpc.term() == log_.term())
	{
		if(leader_state == state_)
		{
			if(rpc.success())
			{
				uint32_t last_index = rpc.request().prev_log_index() + rpc.request().entries().size();
				BOOST_LOG_TRIVIAL(info) << "Successfully added " << rpc.request().entries().size()
					<< " entries to the log of node " << from;

				//We know last_index is the oldest replicated on the other
				//machine, so the next to send to them is last_index + 1.
				client_index_[from] = std::make_tuple(last_index + 1, last_index);

				check_commit();

				//If there are remaining entries, pass them on.
				if(std::get<0>(client_index_[from]) < log_.last_index())
					heartbeat(from);
			}
			else
			{
				//Decrement next_index_
				--std::get<0>(client_index_[from]);
				heartbeat(from);
			}
		}
		else
			BOOST_LOG_TRIVIAL(info) << "Ignoring append_entries response because we're not leading";
	}
	else if(rpc.term() > log_.term())
		//a new term has started
	{
		//and the new term handler will sort the rest
		log_.write(rpc.term());
	}
	//else ignore it; it's stale
}


std::tuple<uint32_t, bool> RaftState::request_vote(const raft::rpc::request_vote& rpc)
{
	//Stale
	if(rpc.term() < log_.term())
	{
		BOOST_LOG_TRIVIAL(info) << "Received stale request_vote from " << rpc.candidate_id();
		return std::make_tuple(log_.term(), false);
	}

	//If it's later, we need to vote
	if(rpc.term() > log_.term())
	{
		BOOST_LOG_TRIVIAL(info) << "Received vote request from new term: " << rpc.term();

		//Write an explicit new term
		log_.write(rpc.term());

		//We obviously haven't voted yet, so we don't need to check for that. We
		//do need to check if the candidate is eligible by our log -- do we have a
		//more up-to-date log?

	}
	else if(follower_state == state_)
	{
		if(log_.last_vote())
		{
			//Same node, so vote again
			if(*log_.last_vote() == rpc.candidate_id())
				return std::make_tuple(log_.term(), true);
			else //different node; reject
				return std::make_tuple(log_.term(), false);
		}
		//else normal voting rules below
	}
	bool vote = false;

	if(log_.last_index() != 0)
	{
		const uint32_t last_term = log_[log_.last_index()].term();
		if(last_term < rpc.last_log_term())
			vote = true;
		if(last_term == rpc.last_log_term())
			vote = rpc.last_log_index() >= log_.last_index();
	}
	else
		vote = true;

	if(vote)
	{
		raft::log::Vote vote(rpc.term(), rpc.candidate_id());
		log_.write(vote);
	}

	//at this point, log_.term() == rpc.term() if we're voting yes
	return std::make_tuple(log_.term(), vote);
}

void RaftState::request_vote_response(const std::string& from,
		const raft::rpc::request_vote_response& rpc)
{
	if(rpc.term() == log_.term())
	{
		if(candidate_state == state_)
		{
			if(rpc.vote_granted())
			{
				BOOST_LOG_TRIVIAL(info) << "Received a vote from " << from
					<< " for term " << log_.term();
				votes_.insert(from);
			}

			if(votes_.size() >= calculate_majority())
			{
				BOOST_LOG_TRIVIAL(info) << "Received a majority of votes, becoming leader.";
				transition_leader();
			}
		}
	}
	else if(rpc.term() > log_.term())
		//a new term has started
	{
		//and the new term handler will sort the rest
		log_.write(rpc.term());
	} //else ignore it; it's stale
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

const RaftLog& RaftState::log() const
{
	return log_;
}

void RaftState::commit_available()
{
	for(; last_applied_ < commit_index_ && last_applied_ < log_.last_index();
			++last_applied_)
		handlers_.commit(log_[last_applied_ + 1].action());
}

void RaftState::term_update(uint32_t term)
{
	assert(term == log_.term());
	BOOST_LOG_TRIVIAL(info) << "Raft state machine updating to new term: " << term;
	transition_follower();
}


void RaftState::check_commit()
{
	uint32_t trial_index = commit_index_ + 1;
	while(trial_index < log_.last_index())
	{
		if(log_[trial_index].term() == log_.term())
		{
			uint32_t number_have = 1;
			for(const std::pair<std::string, std::tuple<uint32_t, uint32_t>> client
					: client_index_)
			{
				if(std::get<1>(std::get<1>(client)) >= trial_index)
					++number_have;
			}

			if(number_have >= calculate_majority())
				commit_index_ = trial_index++;
			else
				break;
		}
		else
			++trial_index;
	}
}

void RaftState::heartbeat()
{
	for(const std::string& node : nodes_)
		heartbeat(node);
}

void RaftState::heartbeat(const std::string& node)
{
	if(std::get<0>(client_index_[node]) == log_.last_index() + 1)
	{
		BOOST_LOG_TRIVIAL(info) << "Empty heartbeat to " << node;
		//send an empty heartbeat
		auto last_entry = log_[log_.last_index()];

		raft::rpc::append_entries msg(log_.term(), id_,
				last_entry.term(), last_entry.index(),
				{}, commit_index_);

		//send the message
		handlers_.append_entries(node, msg);
	}
	else if(std::get<1>(client_index_[node]) == 0)
	{
		BOOST_LOG_TRIVIAL(info) << "Finding match_index for " << node;
		//Still trying to work out the match_index
		uint32_t next_index = std::get<0>(client_index_[node]);
		std::tuple<uint32_t, uint32_t> prev_log{0, 0};
		if(next_index > 1)
		{
			auto entry = log_[next_index - 1];
			prev_log = std::make_tuple(entry.term(), entry.index());
		}

		raft::rpc::append_entries msg(log_.term(), id_,
				std::get<0>(prev_log), std::get<1>(prev_log),
				{}, commit_index_);
		handlers_.append_entries(node, msg);

	}
	else //plain old update
	{
		BOOST_LOG_TRIVIAL(info) << "Empty heartbeat to " << node;
		std::vector<std::tuple<uint32_t, Json::Value>> entries;
		uint32_t from_log = std::get<0>(client_index_[node]);
		entries.reserve(log_.last_index() - from_log);

		for(unsigned int i = from_log; i < log_.last_index(); ++i)
			entries.push_back(std::make_tuple(log_[i].term(), log_[i].action()));

		auto prev_log = log_[from_log - 1];

		raft::rpc::append_entries msg(log_.term(), id_,
				prev_log.term(), prev_log.index(),
				entries, commit_index_);

		handlers_.append_entries(node, msg);
	}
}

void RaftState::transition_follower()
{
	state_ = follower_state;
	leader_ = boost::none;
}

void RaftState::transition_candidate()
{
	state_ = candidate_state;
	votes_.clear();

	//vote for yourself
	votes_.insert(id_);

	//Make a note of that
	log_.write(raft::log::Vote(log_.term(), id_));

	//Send off vote requests.
	for(const std::string& node : nodes_)
	{
		std::tuple<uint32_t, uint32_t> last_log(0, 0);
		if(log_.last_index() > 0)
			last_log = std::make_tuple(log_[log_.last_index()].term(), log_.last_index());

		raft::rpc::request_vote msg(log_.term(), id_,
				std::get<0>(last_log), std::get<1>(last_log));

		handlers_.request_vote(node, msg);
	}
}

void RaftState::transition_leader()
{
	state_ = leader_state;
	client_index_.clear();
	leader_ = id_;

	//Initialise the index
	for(const std::string& node : nodes_)
		client_index_[node] = std::make_tuple(log_.last_index() + 1, 0);

	heartbeat();
}

uint32_t RaftState::calculate_majority()
{
	//nodes_.size() + 1 because we're not stored in nodes
	return ((nodes_.size() + 1) / 2) + 1;
	//( ... / 2) +1 because integer division floors & we'd like the majority
}
