#define BOOST_TEST_MODULE "Raft state-machine"
#include <boost/test/unit_test.hpp>

#include <cstdint>

#include <string>
#include <vector>
#include <functional>
#include <fstream>

#include <json/json.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

namespace fs = boost::filesystem;

#include <json/json.h>
#include "../../common/json_help.hpp"

#include "../raftrpc.hpp"
#include "../raftstate.hpp"

struct disable_logging
{
	disable_logging()
	{
		boost::log::core::get()->set_logging_enabled(false);
	}
};

BOOST_GLOBAL_FIXTURE(disable_logging)

class test_fixture
{
public:
	test_fixture();
	~test_fixture();

	void write_for_stale() const;

	fs::path tmp_log() const;
	bool handler_called() const;

	rpc_handlers handler();

	std::vector<std::tuple<std::string, raft_rpc::append_entries>>
		append_entries_args_;

	std::vector<std::tuple<std::string, raft_rpc::request_vote>>
		request_vote_args_;

	std::vector<uint32_t> request_timeout_args_;

	std::vector<Json::Value> commit_args_;

protected:
	fs::path tmp_log_;
	bool handler_called_;
};

test_fixture::test_fixture()
	:tmp_log_(fs::temp_directory_path() / fs::unique_path()),
	handler_called_(false)
{

}

test_fixture::~test_fixture()
{
	fs::remove(tmp_log_);
}

fs::path test_fixture::tmp_log() const
{
	return tmp_log_;
}

bool test_fixture::handler_called() const
{
	return handler_called_;
}

rpc_handlers test_fixture::handler()
{
	return rpc_handlers(
			[this](const std::string& to, const raft_rpc::append_entries& rpc)
			{
				handler_called_ = true;
				append_entries_args_.push_back(std::make_tuple(to, rpc));
			},
			[this](const std::string& to, const raft_rpc::request_vote& rpc)
			{
				handler_called_ = true;
				request_vote_args_.push_back(std::make_tuple(to, rpc));
			},
			[this](uint32_t timeout)
			{
				handler_called_ = true;
				request_timeout_args_.push_back(timeout);
			},
			[this](const Json::Value& value)
			{
				handler_called_ = true;
				commit_args_.push_back(value);
			});
}

void test_fixture::write_for_stale() const
{
	std::ofstream of(tmp_log().string());
	of << R"({"term":1,"type":"vote","for":"foo"})" << '\n'
		<< R"({"term":1,"type":"entry","index":1,"action":"thud"})" << '\n'
		<< R"({"term":2,"type":"entry","index":2,"action":"thud"})" << '\n';
}

BOOST_FIXTURE_TEST_CASE(starts_as_follower, test_fixture)
{
	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK_MESSAGE(!handler_called(), "Handlers shouldn't be called on startup");
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}


//In follower state
BOOST_FIXTURE_TEST_CASE(stale_append_entries_rejected_with_correct_term, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"foo"})" << '\n'
			<< R"({"term":1,"type":"entry","index":1,"action":"thud"})" << '\n';
	}

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::append_entries request(1, "bar", 1, 1, {}, 1);

	auto result = sut.append_entries(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(!std::get<1>(result));

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(stale_request_vote_rejected_with_correct_term, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"foo"})" << '\n'
			<< R"({"term":2,"type":"vote","for":"foo"})" << '\n';
	}

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::request_vote request(1, "bar", 1, 1);

	auto result = sut.request_vote(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(!std::get<1>(result));

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(append_entries_from_new_term_updates_term, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	raft_rpc::append_entries request(3, "bar", 2, 2, {}, 2);

	auto result = sut.append_entries(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 3);
	BOOST_REQUIRE(std::get<1>(result));

	BOOST_REQUIRE_EQUAL(sut.term(), 3);
	BOOST_REQUIRE_MESSAGE(sut.leader(), "Leader cannot be none for this term.");
	BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(append_entries_with_incorrect_prev_log_term, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	raft_rpc::append_entries request(2, "bar", 1, 2, {}, 1);

	auto result = sut.append_entries(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(!std::get<1>(result));

	//We've received an AppendEntries request with the correct term, so the leader
	//should be "bar"
	BOOST_REQUIRE(sut.leader());
	BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(append_entries_with_incorrect_prev_log_term_with_new_indices, test_fixture)
{
	write_for_stale();

	{
		RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

		raft_rpc::append_entries request(2, "bar", 1, 2,
				{json_help::parse(R"({"foo": "bar"})")}, 1);

		auto result = sut.append_entries(request);

		BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
		BOOST_REQUIRE(!std::get<1>(result));

		//We've received an AppendEntries request with the correct term, so the leader
		//should be "bar"
		BOOST_REQUIRE(sut.leader());
		BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

		//Responses should come back in the return value
		BOOST_CHECK(!handler_called());
		BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
	}

	std::ifstream log(tmp_log().string());
	//ignore the first three lines
	std::string line;
	for(unsigned int i = 0; i < 3; ++i)
		BOOST_REQUIRE(std::getline(log, line));

	//Check there's nothing else
	BOOST_REQUIRE(log.eof());
}

BOOST_FIXTURE_TEST_CASE(append_entries_with_incorrect_prev_log_index_late, test_fixture)
{
	write_for_stale();
	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	raft_rpc::append_entries request(2, "bar", 2, 3, {}, 2);

	auto result = sut.append_entries(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(!std::get<1>(result));

	//We've received an AppendEntries request with the correct term, so the leader
	//should be "bar"
	BOOST_REQUIRE(sut.leader());
	BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(append_entries_with_incorrect_prev_log_index_late_with_new_indices, test_fixture)
{
	write_for_stale();

	{
		RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

		std::vector<Json::Value> entries{json_help::parse(R"({"foo": "bar"})")};

		raft_rpc::append_entries request(2, "bar", 2, 3, entries, 2);

		auto result = sut.append_entries(request);

		BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
		BOOST_REQUIRE(!std::get<1>(result));

		//We've received an AppendEntries request with the correct term, so the leader
		//should be "bar"
		BOOST_REQUIRE(sut.leader());
		BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

		//Responses should come back in the return value
		BOOST_CHECK(!handler_called());
		BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
	}

	std::ifstream log(tmp_log().string());
	//ignore the first three lines
	std::string line;
	for(unsigned int i = 0; i < 3; ++i)
		BOOST_REQUIRE(std::getline(log, line));

	BOOST_REQUIRE(log.eof());
}

BOOST_FIXTURE_TEST_CASE(append_entries_with_correct_prev_log, test_fixture)
{
	write_for_stale();

	{
		RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

		raft_rpc::append_entries request(2, "bar", 2, 2, {}, 2);

		auto result = sut.append_entries(request);

		BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
		BOOST_REQUIRE(std::get<1>(result));

		//We've received an AppendEntries request with the correct term, so the leader
		//should be "bar"
		BOOST_REQUIRE(sut.leader());
		BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

		//Responses should come back in the return value
		BOOST_CHECK(!handler_called());
		BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
	}

	std::ifstream log(tmp_log().string());
	//ignore the first three lines
	std::string line;
	for(unsigned int i = 0; i < 3; ++i)
		BOOST_REQUIRE(std::getline(log, line));

	BOOST_REQUIRE(log.eof());
}

BOOST_FIXTURE_TEST_CASE(append_entries_with_correct_prev_log_ignores_next_timeout, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	raft_rpc::append_entries request(2, "bar", 2, 2, {}, 2);

	auto result = sut.append_entries(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(std::get<1>(result));

	//We've received an AppendEntries request with the correct term, so the leader
	//should be "bar"
	BOOST_REQUIRE(sut.leader());
	BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::follower_state);
	BOOST_CHECK(handler_called());
	BOOST_CHECK_EQUAL(request_timeout_args_.size(), 1);
}

BOOST_FIXTURE_TEST_CASE(append_entries_with_correct_prev_log_appends_entries, test_fixture)
{
	write_for_stale();

	{
		RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

		std::vector<Json::Value> entries{json_help::parse(R"({"foo": "bar"})")};

		raft_rpc::append_entries request(2, "bar", 2, 2, entries, 2);

		auto result = sut.append_entries(request);

		BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
		BOOST_REQUIRE(std::get<1>(result));

		//We've received an AppendEntries request with the correct term, so the leader
		//should be "bar"
		BOOST_REQUIRE(sut.leader());
		BOOST_REQUIRE_EQUAL(*sut.leader(), "bar");

		//Responses should come back in the return value
		BOOST_CHECK(!handler_called());
		BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
	}

	std::ifstream log(tmp_log().string());
	//ignore the first three lines
	std::string line;
	for(unsigned int i = 0; i < 3; ++i)
		BOOST_REQUIRE(std::getline(log, line));

	BOOST_REQUIRE(std::getline(log, line));
	auto log_entry = json_help::parse(line);

	BOOST_REQUIRE_EQUAL(log_entry["term"].asInt(), 2);
	BOOST_REQUIRE_EQUAL(log_entry["type"].asString(), "entry");
	BOOST_REQUIRE_EQUAL(log_entry["index"].asInt(), 3);
	BOOST_REQUIRE_EQUAL(log_entry["action"]["foo"], "bar");
}

BOOST_FIXTURE_TEST_CASE(timeout_switches_to_candidate_state_fires_requests, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"foo"})" << '\n'
			<< R"({"term":1,"type":"entry","index":1,"action":"thud"})" << '\n'
			<< R"({"term":2,"type":"entry","index":2,"action":"thud"})" << '\n'
			<< R"({"term":2,"type":"entry","index":3,"action":"thud"})" << '\n';
	}

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);
	BOOST_REQUIRE_EQUAL(sut.term(), 3);
	BOOST_REQUIRE(handler_called());
	BOOST_REQUIRE_EQUAL(request_timeout_args_.size(), 1);

	BOOST_REQUIRE_EQUAL(request_vote_args_.size(), 2);
	BOOST_REQUIRE(std::get<0>(request_vote_args_[0]) == "foo" ||
			std::get<0>(request_vote_args_[1]) == "foo");
	BOOST_REQUIRE(std::get<0>(request_vote_args_[0]) == "bar" ||
			std::get<0>(request_vote_args_[1]) == "bar");

	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).candidate_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).last_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).last_log_index(), 3);

	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).candidate_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).last_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).last_log_index(), 3);
}

BOOST_FIXTURE_TEST_CASE(request_vote_already_voted_different_endpoint_reject, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"foo"})" << '\n'
			<< R"({"term":1,"type":"entry","index":1,"action":"thud"})" << '\n'
			<< R"({"term":2,"type":"vote":,"for":"foo"})" << '\n';
	}

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::request_vote request(2, "bar", 1, 1);

	auto result = sut.request_vote(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(!std::get<1>(result));

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(request_vote_already_voted_same_endpoint_repeat, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"foo"})" << '\n'
			<< R"({"term":1,"type":"entry","index":1,"action":"thud"})" << '\n'
			<< R"({"term":2,"type":"vote":,"for":"foo"})" << '\n';
	}

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::request_vote request(2, "foo", 1, 1);

	auto result = sut.request_vote(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(std::get<1>(result));

	//Responses should come back in the return value
	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(request_vote_first_come_first_served, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::request_vote request(3, "foo", 2, 3);
	raft_rpc::request_vote request2(3, "bar", 2, 4);

	auto result = sut.request_vote(request);

	BOOST_REQUIRE_EQUAL(sut.term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<0>(result), 3);
	BOOST_REQUIRE(std::get<1>(result));

	result = sut.request_vote(request2);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 3);
	BOOST_REQUIRE(!std::get<1>(result));

	//Check the leader is still
	BOOST_REQUIRE(!sut.leader());
}

BOOST_FIXTURE_TEST_CASE(request_vote_last_log_term_lower_reject, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::request_vote request(3, "foo", 1, 2);

	auto result = sut.request_vote(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 3);
	BOOST_REQUIRE(!std::get<1>(result));
}

BOOST_FIXTURE_TEST_CASE(request_vote_last_log_index_lower_reject, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"foo"})" << '\n'
			<< R"({"term":1,"type":"entry","index":1,"action":"thud"})" << '\n'
			<< R"({"term":2,"type":"entry","index":2,"action":"thud"})" << '\n'
			<< R"({"term":2,"type":"entry","index":3,"action":"thud"})" << '\n';
	}

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::request_vote request(3, "foo", 2, 2);

	auto result = sut.request_vote(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 3);
	BOOST_REQUIRE(!std::get<1>(result));
}

BOOST_FIXTURE_TEST_CASE(request_vote_last_log_later_accept, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	BOOST_CHECK(!handler_called());
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

	raft_rpc::request_vote request(3, "foo", 2, 3);

	auto result = sut.request_vote(request);

	BOOST_REQUIRE_EQUAL(std::get<0>(result), 2);
	BOOST_REQUIRE(std::get<1>(result));
}

//In candidate state
BOOST_FIXTURE_TEST_CASE(candidate_receiving_majority_votes_switches_to_leader_fires_appends_new_term, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	//Only need one for a majority
	sut.request_vote_response("bar", 3, true);

	BOOST_CHECK_EQUAL(sut.state(), RaftState::leader_state);
	BOOST_CHECK(sut.leader());
	BOOST_CHECK_EQUAL(*sut.leader(), "eris");

	BOOST_REQUIRE_EQUAL(append_entries_args_.size(), 2);
	BOOST_REQUIRE(std::get<0>(append_entries_args_[0]) == "foo" ||
			std::get<0>(append_entries_args_[1]) == "foo");
	BOOST_REQUIRE(std::get<0>(append_entries_args_[0]) == "bar" ||
			std::get<0>(append_entries_args_[1]) == "bar");

	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).leader_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).prev_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).prev_log_index(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).entries().size(), 0);

	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).leader_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).prev_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).prev_log_index(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).entries().size(), 0);
}

BOOST_FIXTURE_TEST_CASE(receive_append_for_current_term_respond_and_switch_to_follower, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	raft_rpc::append_entries rpc(3, "foo", 2, 2, {}, 2);

	BOOST_CHECK(sut.leader());
	BOOST_CHECK_EQUAL(*sut.leader(), "foo");
	BOOST_CHECK_EQUAL(sut.term(), 3);
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
}

BOOST_FIXTURE_TEST_CASE(receive_append_for_later_term_respond_and_switch_to_follower, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	raft_rpc::append_entries rpc(4, "foo", 2, 2, {}, 2);

	BOOST_CHECK(sut.leader());
	BOOST_CHECK_EQUAL(*sut.leader(), "foo");
	BOOST_CHECK_EQUAL(sut.term(), 3);
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

}

BOOST_FIXTURE_TEST_CASE(receive_vote_request_for_later_term_respond_and_switch_to_follower, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	raft_rpc::request_vote rpc(4, "foo", 2, 2);

	BOOST_CHECK(sut.leader());
	BOOST_CHECK_EQUAL(*sut.leader(), "foo");
	BOOST_CHECK_EQUAL(sut.term(), 3);
	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);

}

BOOST_FIXTURE_TEST_CASE(timeout_from_candidate_creates_new_vote_term, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);
	BOOST_REQUIRE_EQUAL(sut.term(), 3);

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);
	BOOST_REQUIRE_EQUAL(sut.term(), 4);

	BOOST_REQUIRE_EQUAL(request_vote_args_.size(), 2);
	BOOST_REQUIRE(std::get<0>(request_vote_args_[0]) == "foo" ||
			std::get<0>(request_vote_args_[1]) == "foo");
	BOOST_REQUIRE(std::get<0>(request_vote_args_[0]) == "bar" ||
			std::get<0>(request_vote_args_[1]) == "bar");

	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).term(), 4);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).candidate_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).last_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[0]).last_log_index(), 2);

	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).term(), 4);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).candidate_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).last_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(request_vote_args_[1]).last_log_index(), 2);

}

//as leader
BOOST_FIXTURE_TEST_CASE(leader_timeout_sends_heartbeats, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	//Only need one for a majority
	sut.request_vote_response("bar", 3, true);

	BOOST_CHECK_EQUAL(sut.state(), RaftState::leader_state);

	//Reset the append_entries handler
	append_entries_args_.clear();

	sut.timeout();

	//check it's still a leader
	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::leader_state);

	BOOST_REQUIRE_EQUAL(append_entries_args_.size(), 2);
	BOOST_REQUIRE(std::get<0>(append_entries_args_[0]) == "foo" ||
			std::get<0>(append_entries_args_[1]) == "foo");
	BOOST_REQUIRE(std::get<0>(append_entries_args_[0]) == "bar" ||
			std::get<0>(append_entries_args_[1]) == "bar");

	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).leader_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).prev_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).prev_log_index(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).entries().size(), 0);

	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).leader_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).prev_log_term(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).prev_log_index(), 2);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[1]).entries().size(), 0);

}

BOOST_FIXTURE_TEST_CASE(leader_heartbeat_response_arrives_does_nothing_if_up_to_date, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	//Only need one for a majority
	sut.request_vote_response("bar", 3, true);

	BOOST_CHECK_EQUAL(sut.state(), RaftState::leader_state);

	append_entries_args_.clear();

	sut.append_entries_response("foo", 3, true);

	BOOST_CHECK_EQUAL(append_entries_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(leader_heartbeat_response_decrement_next_index_on_failure_with_correct_term, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	//Only need one for a majority
	sut.request_vote_response("bar", 3, true);

	BOOST_CHECK_EQUAL(sut.state(), RaftState::leader_state);

	append_entries_args_.clear();

	sut.append_entries_response("foo", 3, false);

	BOOST_REQUIRE_EQUAL(append_entries_args_.size(), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(append_entries_args_[0]), "foo");

	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).term(), 3);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).leader_id(), "eris");
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).prev_log_term(), 1);
	BOOST_REQUIRE_EQUAL(std::get<1>(append_entries_args_[0]).prev_log_index(), 1);
	BOOST_REQUIRE(std::get<1>(append_entries_args_[0]).entries().empty());
}

BOOST_FIXTURE_TEST_CASE(leader_heartbeat_response_fallback_to_follower_on_newer_term, test_fixture)
{
	write_for_stale();

	RaftState sut("eris", {"foo", "bar"}, tmp_log().string(), handler());

	sut.timeout();

	BOOST_REQUIRE_EQUAL(sut.state(), RaftState::candidate_state);

	//Only need one for a majority
	sut.request_vote_response("bar", 3, true);

	BOOST_CHECK_EQUAL(sut.state(), RaftState::leader_state);

	append_entries_args_.clear();

	sut.append_entries_response("foo", 4, false);

	BOOST_CHECK_EQUAL(sut.state(), RaftState::follower_state);
	BOOST_CHECK(!sut.leader());
}

BOOST_FIXTURE_TEST_CASE(leader_matchindex_changes_commit_updates, test_fixture)
{
	BOOST_REQUIRE(false);
}

BOOST_FIXTURE_TEST_CASE(leader_client_request_appends_entry, test_fixture)
{
	BOOST_REQUIRE(false);
}

