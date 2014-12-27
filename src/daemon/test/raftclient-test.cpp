#define BOOST_TEST_MODULE "Raft client test"
#include <boost/test/unit_test.hpp>

#include <string>
#include <functional>
#include <fstream>
#include <set>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include <json/json.h>
#include "../../common/json_help.hpp"

#include "../raftrpc.hpp"
#include "../raftstate.hpp"
#include "../raftclient.hpp"

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

	boost::optional<std::string> leader_;

	std::vector<std::tuple<std::string, Json::Value>> send_request_args_;
	std::vector<Json::Value> append_to_log_args_;

	raft::Client::Handlers handler_;
};

test_fixture::test_fixture()
	:leader_("discordia"),
	handler_(
			[this](const std::string& to, const Json::Value& request)
			{
				send_request_args_.push_back(std::make_tuple(to, request));
			},
			[this](const Json::Value& request)
			{
				append_to_log_args_.push_back(request);
			},
			[this]() -> boost::optional<std::string>
			{
				return leader_;
			})
{
}

BOOST_FIXTURE_TEST_CASE(commit_handler_add_on_non_existent_key_success, test_fixture)
{
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));


	BOOST_REQUIRE(sut.exists("fnord"));
	BOOST_REQUIRE_EQUAL(std::get<0>(sut["fnord"]), "bar");
	BOOST_REQUIRE_EQUAL(std::get<1>(sut["fnord"]), "foo");
}

BOOST_FIXTURE_TEST_CASE(commit_handler_add_on_existing_key_different_version_fail, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	//The failure
	BOOST_REQUIRE_THROW(sut.commit_handler(raft::request::Add("foo", "fnord", "baz")), std::runtime_error);
}

BOOST_FIXTURE_TEST_CASE(commit_handler_add_on_existing_key_same_version_success, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	//Exercise
	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));
}


BOOST_FIXTURE_TEST_CASE(commit_handler_delete_on_existing_key_same_version_success, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	sut.commit_handler(raft::request::Delete("foo", "fnord", "bar"));

	BOOST_REQUIRE(!sut.exists("fnord"));

}

BOOST_FIXTURE_TEST_CASE(commit_handler_delete_on_existing_key_different_version_fail, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	BOOST_REQUIRE_THROW(sut.commit_handler(raft::request::Delete("foo", "fnord", "baz")), std::runtime_error);

}

BOOST_FIXTURE_TEST_CASE(commit_handler_delete_on_missing_key_nop, test_fixture)
{
	raft::Client sut("eris", handler_);

	BOOST_REQUIRE(!sut.exists("fnord"));

	sut.commit_handler(raft::request::Delete("foo", "fnord", "bar"));

	BOOST_REQUIRE(!sut.exists("fnord"));
}


BOOST_FIXTURE_TEST_CASE(commit_handler_update_on_missing_key_fail, test_fixture)
{
	raft::Client sut("eris", handler_);

	BOOST_REQUIRE(!sut.exists("fnord"));

	//exercise
	BOOST_REQUIRE_THROW(sut.commit_handler(raft::request::Update("foo", "fnord", "bar", "baz")), std::runtime_error);

	BOOST_REQUIRE(!sut.exists("fnord"));
}

BOOST_FIXTURE_TEST_CASE(commit_handler_update_on_key_wrong_version_fail, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	//exercise
	BOOST_REQUIRE_THROW(sut.commit_handler(raft::request::Update("foo", "fnord", "thud", "baz")), std::runtime_error);

	BOOST_REQUIRE(sut.exists("fnord"));
	BOOST_REQUIRE_EQUAL(std::get<0>(sut["fnord"]), "bar");
	BOOST_REQUIRE_EQUAL(std::get<1>(sut["fnord"]), "foo");
}

BOOST_FIXTURE_TEST_CASE(commit_handler_update_on_key_right_version_success, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	//exercise
	sut.commit_handler(raft::request::Update("foo", "fnord", "bar", "baz"));

	BOOST_REQUIRE(sut.exists("fnord"));
	BOOST_REQUIRE_EQUAL(std::get<0>(sut["fnord"]), "baz");
	BOOST_REQUIRE_EQUAL(std::get<1>(sut["fnord"]), "foo");
}


BOOST_FIXTURE_TEST_CASE(commit_handler_rename_on_missing_key_but_new_key_exists_and_correct_version_success, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	//exercise
	sut.commit_handler(raft::request::Rename("foo", "thud", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));
	BOOST_REQUIRE_EQUAL(std::get<0>(sut["fnord"]), "bar");
	BOOST_REQUIRE_EQUAL(std::get<1>(sut["fnord"]), "foo");

}

BOOST_FIXTURE_TEST_CASE(commit_handler_rename_on_missing_key_missing_new_key_fail, test_fixture)
{
	raft::Client sut("eris", handler_);

	BOOST_REQUIRE(!sut.exists("fnord"));

	//exercise
	BOOST_REQUIRE_THROW(sut.commit_handler(raft::request::Rename("foo", "thud", "fnord", "bar")),
				std::runtime_error);

	BOOST_REQUIRE(!sut.exists("fnord"));
	BOOST_REQUIRE(!sut.exists("thud"));
}

BOOST_FIXTURE_TEST_CASE(commit_handler_rename_on_key_wrong_version_fail, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	//exercise
	BOOST_REQUIRE_THROW(sut.commit_handler(raft::request::Rename("foo", "fnord", "thud", "baz")),
			std::runtime_error);

	BOOST_REQUIRE(sut.exists("fnord"));
	BOOST_REQUIRE_EQUAL(std::get<0>(sut["fnord"]), "bar");
	BOOST_REQUIRE_EQUAL(std::get<1>(sut["fnord"]), "foo");

}

BOOST_FIXTURE_TEST_CASE(commit_handler_rename_on_key_right_version_success, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	//exercise
	sut.commit_handler(raft::request::Rename("foo", "fnord", "thud", "bar"));

	BOOST_REQUIRE(!sut.exists("fnord"));
	BOOST_REQUIRE(sut.exists("thud"));
	BOOST_REQUIRE_EQUAL(std::get<0>(sut["thud"]), "bar");
	BOOST_REQUIRE_EQUAL(std::get<1>(sut["thud"]), "foo");
}

BOOST_FIXTURE_TEST_CASE(update_forwarded_to_leader, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.request(raft::request::Update("eris", "foo", "bar", "baz"));

	//check
	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(send_request_args_[0]), "discordia");

	raft::request::Update update(std::get<1>(send_request_args_[0]));
	BOOST_REQUIRE_EQUAL(update.from(), "eris");
	BOOST_REQUIRE_EQUAL(update.key(), "foo");
	BOOST_REQUIRE_EQUAL(update.old_version(), "bar");
	BOOST_REQUIRE_EQUAL(update.new_version(), "baz");

	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(delete_forwarded_to_leader, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);
	sut.commit_handler(raft::request::Add("foo", "foo", "bar"));

	sut.request(raft::request::Delete("eris", "foo", "bar"));

	//check
	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(send_request_args_[0]), "discordia");

	raft::request::Delete del(std::get<1>(send_request_args_[0]));
	BOOST_REQUIRE_EQUAL(del.from(), "eris");
	BOOST_REQUIRE_EQUAL(del.key(), "foo");
	BOOST_REQUIRE_EQUAL(del.version(), "bar");

	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(rename_forwarded_to_leader, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.request(raft::request::Rename("eris", "foo", "thud", "bar"));

	//check
	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(send_request_args_[0]), "discordia");

	raft::request::Rename rename(std::get<1>(send_request_args_[0]));
	BOOST_REQUIRE_EQUAL(rename.from(), "eris");
	BOOST_REQUIRE_EQUAL(rename.key(), "foo");
	BOOST_REQUIRE_EQUAL(rename.new_key(), "thud");
	BOOST_REQUIRE_EQUAL(rename.version(), "bar");

	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(add_forwarded_to_leader, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.request(raft::request::Add("eris", "foo", "bar"));

	//check
	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(send_request_args_[0]), "discordia");

	raft::request::Add add(std::get<1>(send_request_args_[0]));
	BOOST_REQUIRE_EQUAL(add.from(), "eris");
	BOOST_REQUIRE_EQUAL(add.key(), "foo");
	BOOST_REQUIRE_EQUAL(add.version(), "bar");

	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}


BOOST_FIXTURE_TEST_CASE(non_leader_ignore_committed_add, test_fixture)
{
	//Setup the existing key
	raft::Client sut("eris", handler_);

	sut.commit_handler(raft::request::Add("foo", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	sut.request(raft::request::Add("eris", "fnord", "bar"));

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(non_leader_ignore_committed_update, test_fixture)
{
	raft::Client sut("eris", handler_);
	sut.commit_handler(raft::request::Add("eris", "fnord", "bar"));
	BOOST_REQUIRE(sut.exists("fnord"));

	sut.request(raft::request::Update("eris", "fnord", "foo", "bar"));

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(non_leader_ignore_committed_rename, test_fixture)
{
	raft::Client sut("eris", handler_);
	sut.commit_handler(raft::request::Add("eris", "fnord", "bar"));

	BOOST_REQUIRE(sut.exists("fnord"));

	sut.request(raft::request::Rename("eris", "thud", "fnord", "bar"));

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}


BOOST_FIXTURE_TEST_CASE(leader_add_no_key_success, test_fixture)
{
	//Now a leader
	raft::Client sut("discordia", handler_);

	raft::request::Add request("eris", "fnord", "foo");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 1);

	raft::request::Add add(append_to_log_args_.front());
	BOOST_REQUIRE_EQUAL(add, request);
}

BOOST_FIXTURE_TEST_CASE(leader_add_key_same_version_ignored, test_fixture)
{
	//Now a leader
	raft::Client sut("discordia", handler_);

	raft::request::Add request("eris", "fnord", "foo");
	sut.request(request);

	//new instance so we can be sure it's not just comparing pointers
	sut.request(raft::request::Add("eris", "fnord", "foo"));

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 1);

	raft::request::Add add(append_to_log_args_.front());
	BOOST_REQUIRE_EQUAL(add, request);

}

BOOST_FIXTURE_TEST_CASE(leader_add_key_wrong_version_ignored, test_fixture)
{
	//Now a leader
	raft::Client sut("discordia", handler_);

	raft::request::Add request("eris", "fnord", "foo");
	sut.request(request);

	//new instance so we can be sure it's not just comparing pointers
	sut.request(raft::request::Add("eris", "fnord", "bar"));

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 1);

	raft::request::Add add(append_to_log_args_.front());
	BOOST_REQUIRE_EQUAL(add, request);
}


BOOST_FIXTURE_TEST_CASE(leader_delete_key_right_version_success, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));

	BOOST_REQUIRE(sut.exists("fnord"));

	raft::request::Delete request("eris", "fnord", "foo");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 1);

	raft::request::Delete del(append_to_log_args_.front());
	BOOST_REQUIRE_EQUAL(del, request);
}

BOOST_FIXTURE_TEST_CASE(leader_delete_key_wrong_version_ignored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));

	raft::request::Delete request("eris", "fnord", "bar");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(leader_delete_no_key_ingored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	raft::request::Delete request("eris", "fnord", "bar");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}


BOOST_FIXTURE_TEST_CASE(leader_update_key_right_version_success, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));

	raft::request::Update request("eris", "fnord", "foo", "bar");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 1);

	raft::request::Update update(append_to_log_args_.front());
	BOOST_REQUIRE_EQUAL(update, request);
}

BOOST_FIXTURE_TEST_CASE(leader_update_key_wrong_version_ignored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));

	raft::request::Update request("eris", "fnord", "bar", "baz");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(leader_update_no_key_ignored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	raft::request::Update request("eris", "fnord", "bar", "baz");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}


BOOST_FIXTURE_TEST_CASE(leader_rename_key_right_version_success, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));

	raft::request::Rename request("eris", "fnord", "thud", "foo");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 1);

	raft::request::Rename rename(append_to_log_args_.front());
	BOOST_REQUIRE_EQUAL(rename, request);
}

BOOST_FIXTURE_TEST_CASE(leader_rename_key_wrong_version_ignored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));

	raft::request::Rename request("eris", "fnord", "thud", "bar");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(leader_rename_no_key_ignored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	raft::request::Rename request("eris", "fnord", "thud", "bar");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(leader_rename_new_key_exists_same_version_ignored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));
	sut.commit_handler(raft::request::Add("eris", "thud", "bar"));

	raft::request::Rename request("eris", "fnord", "thud", "bar");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}

BOOST_FIXTURE_TEST_CASE(leader_rename_new_key_exists_different_version_ignored, test_fixture)
{
	raft::Client sut("discordia", handler_);

	sut.commit_handler(raft::request::Add("eris", "fnord", "foo"));
	sut.commit_handler(raft::request::Add("eris", "thud", "bar"));

	raft::request::Rename request("eris", "fnord", "thud", "baz");
	sut.request(request);

	BOOST_REQUIRE_EQUAL(send_request_args_.size(), 0);
	BOOST_REQUIRE_EQUAL(append_to_log_args_.size(), 0);
}
