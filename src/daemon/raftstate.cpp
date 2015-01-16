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

raft::State::Handlers::Handlers(const append_entries_type& append_entries,
		const request_vote_type& request_vote, const timeout_type& request_timeout,
		const commit_type& commit)
	:append_entries_(append_entries),
	request_vote_(request_vote),
	request_timeout_(request_timeout),
	commit_(commit)
{
}

void raft::State::Handlers::append_entries(const std::string& endpoint, const raft::rpc::append_entries& rpc)
{
	append_entries_(endpoint, rpc);
}

void raft::State::Handlers::request_vote(const std::string& endpoint, const raft::rpc::request_vote& rpc)
{
	request_vote_(endpoint, rpc);
}

void raft::State::Handlers::request_timeout(timeout_length length)
{
	request_timeout_(length);
}

void raft::State::Handlers::commit(const Json::Value& value)
{
	commit_(value);
}

raft::State::State(const std::string& id, const std::vector<std::string>& nodes,
		const std::string& log_file, State::Handlers& handlers)
	:id_(id),
	nodes_(nodes),
	log_(log_file, std::bind(&raft::State::term_update, this, std::placeholders::_1)),
	state_(follower_state),
	handlers_(handlers),
	commit_index_(0),
	last_applied_(0)
{
	transition_follower();
}

void raft::State::timeout()
{
	if(follower_state == state_ || candidate_state == state_)
	{
		BOOST_LOG_TRIVIAL(trace) << "Broadcasting candidacy for new election term.";
		//update the term
		log_.write(log_.term() + 1);
		//Perform the transition (which will also ask for the timeout)
		transition_candidate();
	}
	else
	{
		heartbeat();
		handlers_.request_timeout(State::Handlers::leader_timeout);
	}
}

raft::State::Status raft::State::state() const
{
	return state_;
}

std::tuple<uint32_t, bool> raft::State::append_entries(const raft::rpc::append_entries& rpc)
{
	//Stale
	if(rpc.term() < log_.term())
	{
		BOOST_LOG_TRIVIAL(warning) << "Received stale append_entries request from " << rpc.leader_id();
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
		handlers_.request_timeout(State::Handlers::election_timeout);

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
				BOOST_LOG_TRIVIAL(trace) << "Added " << rpc.entries().size() << " log entries from "
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
		{
			BOOST_LOG_TRIVIAL(trace) << "No match in log.";
			return std::make_tuple(log_.term(), false);
		}
	}

	//Shouldn't get here, but just in case
	return std::make_tuple(log_.term(), false);
}

void raft::State::append_entries_response(const std::string& from,
		const raft::rpc::append_entries_response& rpc)
{
	if(rpc.term() == log_.term())
	{
		if(leader_state == state_)
		{
			if(rpc.success())
			{
				uint32_t last_index = rpc.request().prev_log_index() + rpc.request().entries().size();
				BOOST_LOG_TRIVIAL(trace) << "Successfully added " << rpc.request().entries().size()
					<< " entries to the log of node " << from;

				//We know last_index is the oldest replicated on the other
				//machine, so the next to send to them is last_index + 1.
				BOOST_LOG_TRIVIAL(trace) << "Updating info for node " << from << " to: (" <<
					last_index + 1 << ", " << last_index << ", false)";
				client_index_[from] = std::make_tuple(last_index + 1, last_index, false);

				check_commit();

				//If there are remaining entries, pass them on.
				if(std::get<0>(client_index_[from]) < log_.last_index())
				{
					BOOST_LOG_TRIVIAL(trace) << "Responding to succesful append_entries with the remaining log entries.";
					heartbeat(from);
				}
			}
			else
			{
				//Decrement next_index_
				--std::get<0>(client_index_[from]);
				heartbeat(from);
			}
		}
		else
			BOOST_LOG_TRIVIAL(trace) << "Ignoring append_entries response because we're not leading";
	}
	else if(rpc.term() > log_.term())
		//a new term has started
	{
		//and the new term handler will sort the rest
		log_.write(rpc.term());
	}
	//else ignore it; it's stale
}


std::tuple<uint32_t, bool> raft::State::request_vote(const raft::rpc::request_vote& rpc)
{
	//Stale
	if(rpc.term() < log_.term())
	{
		BOOST_LOG_TRIVIAL(warning) << "Received stale request_vote from " << rpc.candidate_id();
		return std::make_tuple(log_.term(), false);
	}

	//If it's later, we need to vote
	if(rpc.term() > log_.term())
	{
		BOOST_LOG_TRIVIAL(trace) << "Received vote request from new term: " << rpc.term();

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
	else //no vote; we're a leader or candidate in the current term
		return std::make_tuple(log_.term(), false);

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

void raft::State::request_vote_response(const std::string& from,
		const raft::rpc::request_vote_response& rpc)
{
	if(rpc.term() == log_.term())
	{
		if(candidate_state == state_)
		{
			if(rpc.vote_granted())
			{
				BOOST_LOG_TRIVIAL(trace) << "Received a vote from " << from
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

std::string raft::State::id() const
{
	return id_;
}

std::vector<std::string> raft::State::nodes() const
{
	return nodes_;
}

uint32_t raft::State::term() const
{
	return log_.term();
}

boost::optional<std::string> raft::State::leader() const
{
	return leader_;
}

const raft::Log& raft::State::log() const
{
	return log_;
}

void raft::State::append(const Json::Value& root)
{
	if(static_cast<bool>(leader_) && *leader_ == id_)
	{
		raft::log::LogEntry entry(log_.term(), log_.last_index() + 1, root);
		log_.write(entry);
	}
	else
		throw std::logic_error("This node (" + id_ + ") is not the leader: "
				+ (leader_ ? *leader_ : "no leader") + ".");
}

void raft::State::commit_available()
{
	for(; last_applied_ < commit_index_ && last_applied_ < log_.last_index();
			++last_applied_)
		handlers_.commit(log_[last_applied_ + 1].action());
}

void raft::State::term_update(uint32_t term)
{
	assert(term == log_.term());
	BOOST_LOG_TRIVIAL(trace) << "Raft state machine updating to new term: " << term;
	transition_follower();
}


void raft::State::check_commit()
{
	uint32_t trial_index = commit_index_ + 1;
	while(trial_index <= log_.last_index())
	{
		if(log_[trial_index].term() == log_.term())
		{
			uint32_t number_have = 1;
			for(const std::pair<std::string, std::tuple<uint32_t, uint32_t, bool>> client
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

void raft::State::heartbeat()
{
	for(const std::string& node : nodes_)
		heartbeat(node);
}

void raft::State::heartbeat(const std::string& node)
{
	if(std::get<0>(client_index_[node]) == log_.last_index() + 1)
	{
		BOOST_LOG_TRIVIAL(trace) << "Empty heartbeat to " << node;
		//send an empty heartbeat
		std::tuple<uint32_t, uint32_t> entry_info(0, 0);

		if(log_.last_index() > 0)
		{
			auto last_entry = log_[log_.last_index()];
			entry_info = std::make_tuple(last_entry.term(), last_entry.index());
		}

		raft::rpc::append_entries msg(log_.term(), id_,
				std::get<0>(entry_info), std::get<1>(entry_info),
				{}, commit_index_);

		//send the message
		handlers_.append_entries(node, msg);
	}
	else if(std::get<2>(client_index_[node]))
	{
		BOOST_LOG_TRIVIAL(trace) << "Finding match_index for " << node;
		//Still trying to work out the match_index
		uint32_t next_index = std::get<0>(client_index_[node]);
		if(next_index > log_.last_index())
		{
			BOOST_LOG_TRIVIAL(warning) << "Information for follower " << node
				<< " is nonsensical, resetting.";
			client_index_[node] = std::make_tuple(log_.last_index() + 1, 0, true);
		}
		else
		{
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
	}
	else //plain old update
	{
		std::vector<std::tuple<uint32_t, Json::Value>> entries;
		uint32_t from_log = std::get<0>(client_index_[node]);
		entries.reserve(log_.last_index() - from_log);

		BOOST_LOG_TRIVIAL(trace) << "Update to " << node
			<< ", adding logs: " << from_log << "--" << log_.last_index();

		for(unsigned int i = from_log; i <= log_.last_index(); ++i)
			entries.push_back(std::make_tuple(log_[i].term(), log_[i].action()));

		std::tuple<uint32_t, uint32_t> prev_log{0, 0};
		if(from_log > 1)
		{
			auto entry = log_[from_log - 1];
			prev_log = std::make_tuple(entry.term(), entry.index());
		}

		raft::rpc::append_entries msg(log_.term(), id_,
				std::get<0>(prev_log), std::get<1>(prev_log),
				entries, commit_index_);

		handlers_.append_entries(node, msg);
	}
}

void raft::State::transition_follower()
{
	state_ = follower_state;
	leader_ = boost::none;
	handlers_.request_timeout(Handlers::election_timeout);
}

void raft::State::transition_candidate()
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
	handlers_.request_timeout(Handlers::election_timeout);
}

void raft::State::transition_leader()
{
	state_ = leader_state;
	client_index_.clear();
	leader_ = id_;

	//Initialise the index
	for(const std::string& node : nodes_)
		client_index_[node] = std::make_tuple(log_.last_index() + 1, 0, true);

	heartbeat();
	handlers_.request_timeout(Handlers::leader_timeout);
}

uint32_t raft::State::calculate_majority()
{
	//nodes_.size() + 1 because we're not stored in nodes
	return ((nodes_.size() + 1) / 2) + 1;
	//( ... / 2) +1 because integer division floors & we'd like the majority
}
