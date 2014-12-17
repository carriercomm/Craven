#define BOOST_TEST_MODULE "Raft write-ahead logging tests"
#include <boost/test/unit_test.hpp>

#include <cstdint>

#include <string>
#include <exception>
#include <fstream>
#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

#include <json/json.h>

#include "../raftlog.hpp"

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

	void write_simple() const;

	fs::path tmp_log() const;
protected:
	fs::path tmp_log_;
};

test_fixture::test_fixture()
	:tmp_log_(fs::temp_directory_path() / fs::unique_path())
{

}

test_fixture::~test_fixture()
{
	fs::remove(tmp_log_);
}

void test_fixture::write_simple() const
{
	std::ofstream of(tmp_log_.string());

	//Term starts at 1 because we initialise to 0; the first term with an election
	//is 1.
	of << R"({"term":1,"type":"vote","for":"endpoint1"})"
		//first index of the log is 1.
		<< R"({"term":1,"type":"entry","index":1,"action":"thud"})"
		<< R"({"term":1,"type":"entry","index":2,"action":"thud"})"
		<< std::endl;

}

fs::path test_fixture::tmp_log() const
{
	return tmp_log_;
}

BOOST_FIXTURE_TEST_CASE(empty_log_correct_term, test_fixture)
{
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_EQUAL(sut.term(), 0);
}

BOOST_FIXTURE_TEST_CASE(empty_log_correct_vote, test_fixture)
{
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE(!sut.last_vote());
}

BOOST_FIXTURE_TEST_CASE(empty_log_correct_last_index, test_fixture)
{
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_EQUAL(sut.last_index(), 0);
}


BOOST_FIXTURE_TEST_CASE(plain_recovery_correct_term_explicit, test_fixture)
{
	write_simple();

	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_EQUAL(sut.term(), 1);
}

BOOST_FIXTURE_TEST_CASE(plain_recovery_correct_term_implicit, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"endpoint1"})"
			<< R"({"term":1,"type":"entry","index":1,"action":"thud"})"
			<< R"({"term":2,"type":"entry","index":2,"action":"thud"})"
			<< std::endl;

	}

	RaftLog sut(tmp_log().string());
	BOOST_REQUIRE_EQUAL(sut.term(), 2);
}


BOOST_FIXTURE_TEST_CASE(plain_recovery_correct_vote_explicit, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE(sut.last_vote());
	BOOST_REQUIRE_EQUAL(sut.last_vote().get(), "endpoint1");
}

BOOST_FIXTURE_TEST_CASE(plain_recovery_correct_vote_implicit, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"entry","index":1,"action":"thud"})"
			<< R"({"term":1,"type":"entry","index":2,"action":"thud"})"
			<< std::endl;

	}

	RaftLog sut(tmp_log().string());
	BOOST_REQUIRE(!sut.last_vote());
}

BOOST_FIXTURE_TEST_CASE(plain_recovery_correct_vote_implicit_overwrite, test_fixture)
{
	{
		std::ofstream of(tmp_log().string());
		of << R"({"term":1,"type":"vote","for":"endpoint1"})"
			<< R"({"term":1,"type":"entry","index":1,"action":"thud"})"
			<< R"({"term":2,"type":"entry","index":2,"action":"thud"})"
			<< std::endl;
	}

	RaftLog sut(tmp_log().string());
	BOOST_REQUIRE(!sut.last_vote());
}

BOOST_FIXTURE_TEST_CASE(plain_recovery_correct_entry_index, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_EQUAL(sut.last_index(), 2);
}


BOOST_FIXTURE_TEST_CASE(log_entries_append, test_fixture)
{
	write_simple();
	{
		RaftLog sut(tmp_log().string());
		sut.write(raft_log::LogEntry(2, 3, Json::Value("fnord")));
		sut.write(raft_log::LogEntry(2, 4, Json::Value("thud")));
		sut.write(raft_log::Vote(3, "eris"));
	}

	std::ifstream log(tmp_log().string());

	std::string line;
	Json::Value root;
	Json::Reader r;

	std::getline(log, line);
	r.parse(line, root);
	BOOST_REQUIRE_EQUAL(root["term"].asInt(), 1);
	BOOST_REQUIRE_EQUAL(root["type"].asString(), "vote");
	BOOST_REQUIRE_EQUAL(root["for"].asString(), "endpoint1");

	std::getline(log, line);
	r.parse(line, root);
	BOOST_REQUIRE_EQUAL(root["term"].asInt(), 1);
	BOOST_REQUIRE_EQUAL(root["type"].asString(), "entry");
	BOOST_REQUIRE_EQUAL(root["index"].asInt(), 1);
	BOOST_REQUIRE_EQUAL(root["action"].asString(), "fnord");

	std::getline(log, line);
	r.parse(line, root);
	BOOST_REQUIRE_EQUAL(root["term"].asInt(), 1);
	BOOST_REQUIRE_EQUAL(root["type"].asString(), "entry");
	BOOST_REQUIRE_EQUAL(root["index"].asInt(), 2);
	BOOST_REQUIRE_EQUAL(root["action"].asString(), "thud");

	std::getline(log, line);
	r.parse(line, root);
	BOOST_REQUIRE_EQUAL(root["term"].asInt(), 2);
	BOOST_REQUIRE_EQUAL(root["type"].asString(), "entry");
	BOOST_REQUIRE_EQUAL(root["index"].asInt(), 3);
	BOOST_REQUIRE_EQUAL(root["action"].asString(), "fnord");

	std::getline(log, line);
	r.parse(line, root);
	BOOST_REQUIRE_EQUAL(root["term"].asInt(), 2);
	BOOST_REQUIRE_EQUAL(root["type"].asString(), "entry");
	BOOST_REQUIRE_EQUAL(root["index"].asInt(), 4);
	BOOST_REQUIRE_EQUAL(root["action"].asString(), "thud");

	std::getline(log, line);
	r.parse(line, root);
	BOOST_REQUIRE_EQUAL(root["term"].asInt(), 3);
	BOOST_REQUIRE_EQUAL(root["type"].asString(), "vote");
	BOOST_REQUIRE_EQUAL(root["for"].asString(), "eris");

	BOOST_REQUIRE(log.eof());
}

BOOST_FIXTURE_TEST_CASE(log_entries_appended_recoverable, test_fixture)
{
	write_simple();
	{
		RaftLog sut(tmp_log().string());
		sut.write(raft_log::LogEntry(2, 3, Json::Value("fnord")));
		sut.write(raft_log::LogEntry(2, 4, Json::Value("thud")));
		sut.write(raft_log::Vote(3, "eris"));
	}

	RaftLog sut(tmp_log().string());

	BOOST_CHECK_EQUAL(sut.term(), 3);
	BOOST_CHECK_EQUAL(sut.last_vote().get(), "eris");
	BOOST_CHECK_EQUAL(sut.last_index(), 4);

	BOOST_CHECK_EQUAL(sut.log(1).index(), 1);
	BOOST_CHECK_EQUAL(sut.log(1).term(), 1);
	BOOST_CHECK_EQUAL(sut.log(1).action().asString(), "thud");

	BOOST_CHECK_EQUAL(sut.log(2).index(), 2);
	BOOST_CHECK_EQUAL(sut.log(2).term(), 1);
	BOOST_CHECK_EQUAL(sut.log(2).action().asString(), "thud");

	BOOST_CHECK_EQUAL(sut.log(3).index(), 3);
	BOOST_CHECK_EQUAL(sut.log(3).term(), 2);
	BOOST_CHECK_EQUAL(sut.log(3).action().asString(), "fnord");

	BOOST_CHECK_EQUAL(sut.log(4).index(), 4);
	BOOST_CHECK_EQUAL(sut.log(4).term(), 2);
	BOOST_CHECK_EQUAL(sut.log(4).action().asString(), "thud");
}

BOOST_FIXTURE_TEST_CASE(last_known_entry_index_correct, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_EQUAL(sut.last_index(), sut.log(sut.last_index()).index());
}

BOOST_FIXTURE_TEST_CASE(invalidated_entries_overwritten_on_recovery, test_fixture)
{
	write_simple();

	std::ofstream log(tmp_log().string());

	log << R"({"term":1,"type":"entry","index":3,"action":"hail"})"
		<< R"({"term":3,"type":"entry","index":1,"action":"hail"})"
		<< R"({"term":3,"type":"entry","index":2,"action":"discordia"})";

	RaftLog sut(tmp_log().string());

	BOOST_CHECK_EQUAL(sut.last_index(), 2);
	BOOST_CHECK_EQUAL(sut.term(), 3);
	BOOST_CHECK_EQUAL(sut[1].term(), 3);
	BOOST_CHECK_EQUAL(sut[1].action().asString(), "hail");
	BOOST_CHECK_EQUAL(sut[2].action().asString(), "discordia");
}

BOOST_FIXTURE_TEST_CASE(overwriting_valid_log_entry_throws, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_THROW(sut.write(raft_log::LogEntry(2, 1, Json::Value("fnord"))), raft_log::exceptions::entry_exists);
}

BOOST_FIXTURE_TEST_CASE(invalidating_log_entry_invalidates_all_following, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	sut.invalidate(1);

	BOOST_REQUIRE_EQUAL(sut.last_index(), 0);
}

BOOST_FIXTURE_TEST_CASE(overwriting_invalid_log_entry_ok, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	sut.invalidate(2);

	sut.write(raft_log::LogEntry(2, 2, Json::Value("foo")));
}

BOOST_FIXTURE_TEST_CASE(writing_stale_vote_throws, test_fixture)
{
	write_simple();
	std::ofstream log(tmp_log().string());

	log << R"({"term":1,"type":"entry","index":3,"action":"hail"})"
		<< R"({"term":3,"type":"entry","index":4,"action":"hail"})"
		<< R"({"term":3,"type":"entry","index":5,"action":"discordia"})";

	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_THROW(sut.write(raft_log::Vote(2, "eris")), raft_log::exceptions::invalid_vote);

}

BOOST_FIXTURE_TEST_CASE(writing_vote_for_same_term_throws, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	BOOST_REQUIRE_THROW(sut.write(raft_log::Vote(1, "eris")), raft_log::exceptions::vote_exists);
}

BOOST_FIXTURE_TEST_CASE(writing_duplicate_vote_silent, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	sut.write(raft_log::Vote(1, "endpoint1"));
}

BOOST_FIXTURE_TEST_CASE(writing_new_vote_for_term_ok, test_fixture)
{
	write_simple();
	RaftLog sut(tmp_log().string());

	sut.write(raft_log::Vote(2, "eris"));
}
