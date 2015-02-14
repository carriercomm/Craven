#include <string>
#include <functional>
#include <fstream>
#include <set>
#include <vector>

#include <boost/asio.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include <json/json.h>
#include <json_help.hpp>

#include "raftrpc.hpp"
#include "raftrequest.hpp"
#include "raftclient.hpp"
#include "raftstate.hpp"
#include "dispatch.hpp"

#include "raftctl.hpp"

namespace raft
{

	Controller::TimerLength::TimerLength(uint32_t seed, uint32_t leader, uint32_t election, uint32_t fuzz)
		:gen_(seed),
		leader_(leader - fuzz/2, leader + fuzz/2),
		election_(election - fuzz/2, election + fuzz/2)
	{
	}

	uint32_t Controller::TimerLength::operator()(State::Handlers::timeout_length length)
	{
		if(length == State::Handlers::leader_timeout)
			return leader_(gen_);
		else
			return election_(gen_);
	}

	Controller::Controller(boost::asio::io_service& io, dispatch_type& dispatch, const TimerLength& tl,
				const std::string& id, const std::vector<std::string>& nodes,
				const std::string& log_file)
		:io_(io),
		tl_(tl),
		t_(io_),

		//Set up the state handlers
		state_handlers_(
				[this](const std::string& endpoint, const raft::rpc::append_entries& rpc)
				{state_rpc_(endpoint, "raftstate", rpc);}, //rpc contextually-converted to Json::Value
				[this](const std::string& endpoint, const raft::rpc::request_vote& rpc)
				{state_rpc_(endpoint, "raftstate", rpc);},
				std::bind(&Controller::async_reset_timer, this, std::placeholders::_1),
				[this](const Json::Value& value)
				{
					io_.post([this, value]
							{
								try
								{
									client_.commit_handler(value);
								}
								catch(const std::exception& ex)
								{
									BOOST_LOG_TRIVIAL(error) << "Error in raft commit: " << ex.what();
								}
							});
				}
				),

		//Set up the client handlers
		client_handlers_(
				[this](const std::string& endpoint, const Json::Value& request)
				{client_rpc_(endpoint, "raftclient", request);},
				std::bind(&State::append, &state_, std::placeholders::_1),
				//leader. The bind is fine as long as this callback isn't called
				//until state_ is initialised
				std::bind(&State::leader, &state_)
				),

		//Register the dispatch function for raftstate
		state_rpc_(dispatch.connect_dispatcher("raftstate", std::bind(&Controller::dispatch_state, this,
						std::placeholders::_1, std::placeholders::_2))),

		//Register the dispatch for raftclient
		client_rpc_(dispatch.connect_dispatcher("raftclient", std::bind(&Controller::dispatch_client, this,
						std::placeholders::_1))),

		//Set up raft.
		state_(id, nodes, log_file, state_handlers_),
		client_(id, client_handlers_)
	{
	}

	State& Controller::state()
	{
		return state_;
	}

	const State& Controller::state() const
	{
		return state_;
	}

	void Controller::dispatch_state(const Json::Value& msg, typename dispatch_type::Callback cb)
	{
		std::string type = json_help::checked_from_json<std::string>(msg, "type", "Bad json in raft::State dispatch:");

		if(type == "append_entries")
		{
			rpc::append_entries ae(msg);
			//Perform the RPC
			std::tuple<uint32_t, bool> ret = state_.append_entries(ae);
			//Marshal the response
			rpc::append_entries_response aer(ae, std::get<0>(ret), std::get<1>(ret));
			//send
			cb(aer);
		}
		else if(type == "append_entries_response")
		{
			rpc::append_entries_response aer(msg);

			state_.append_entries_response(cb.endpoint(), aer);
		}
		else if(type == "request_vote")
		{
			rpc::request_vote rv(msg);

			std::tuple<uint32_t, bool> ret = state_.request_vote(rv);
			//Marshal the response
			rpc::request_vote_response rvr(rv, std::get<0>(ret), std::get<1>(ret));
			//send
			cb(rvr);
		}
		else if(type == "request_vote_response")
		{
			rpc::request_vote_response rvr(msg);

			state_.request_vote_response(cb.endpoint(), rvr);
		}
		else
			throw std::runtime_error(
					"Bad json in raft::State dispatch: unknown type " + type);
	}


	Client& Controller::client()
	{
		return client_;
	}

	const Client& Controller::client() const
	{
		return client_;
	}

	void Controller::dispatch_client(const Json::Value& msg)
	{
		std::string type = json_help::checked_from_json<std::string>(msg, "type", "Bad json in raft::Client dispatch:");

		if(type == "update")
		{
			request::Update request(msg);
			client_.request(request);
		}
		else if(type == "delete")
		{
			request::Delete request(msg);
			client_.request(request);
		}
		else if(type == "rename")
		{
			request::Rename request(msg);
			client_.request(request);
		}
		else if(type == "add")
		{
			request::Add request(msg);
			client_.request(request);
		}
		else
			throw std::runtime_error("Bad json in raft::Client dispatch: unknown rpc type " + type);

	}

	void Controller::async_reset_timer(State::Handlers::timeout_length length)
	{
		uint32_t to = tl_(length);
		BOOST_LOG_TRIVIAL(trace) << "Timeout requested of length: "
			<< (length == State::Handlers::election_timeout ? "election" : "leader")
			<<  ", true: " << to;

		//Post the reset so this can be called from the timer's handler
		io_.post([this, to]()
				{
					//Cancels the timeouts too
					t_.expires_from_now(boost::posix_time::milliseconds(to));

					//Setup the wait handler
					t_.async_wait([this](const boost::system::error_code& ec)
							{
								if(ec != boost::asio::error::operation_aborted)
								{
									BOOST_LOG_TRIVIAL(trace) << "Raft timeout fired.";
									state_.timeout();
								}
							});
				});
	}
}
