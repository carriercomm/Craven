#define BOOST_TEST_MODULE "Connection pool test"
#include <boost/test/unit_test.hpp>

#include <string>
#include <functional>
#include <fstream>
#include <set>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <json/json.h>
#include <json_help.hpp>

#include <b64/encode.h>
#include <b64/decode.h>

#include "../raftrequest.hpp"
#include "../raftclient.hpp"
#include "../changetx.hpp"

struct disable_logging
{
	disable_logging()
	{
		boost::log::core::get()->set_logging_enabled(false);
	}
};

BOOST_GLOBAL_FIXTURE(disable_logging)

struct test_fixture
{
	test_fixture();
	~test_fixture();

	fs::path temp_root_;
	void send_handler_(const std::string& to, const Json::Value& msg);
	std::vector<std::tuple<std::string, Json::Value>> send_handler_args_;

	std::function<void (const std::string&, const Json::Value&)> send_handler_bound();

	std::vector<std::string> nodes_ = {"eris", "discordia", "hung_mung"};
};

test_fixture::test_fixture()
	:temp_root_(fs::temp_directory_path() / fs::unique_path())
{
	fs::create_directory(temp_root_);
}

test_fixture::~test_fixture()
{
	fs::remove_all(temp_root_);
}

void test_fixture::send_handler_(const std::string& to, const Json::Value& msg)
{
	send_handler_args_.push_back(std::make_tuple(to, msg));
}

std::function<void (const std::string&, const Json::Value&)> test_fixture::send_handler_bound()
{
	return std::bind(&test_fixture::send_handler_, this, std::placeholders::_1, std::placeholders::_2);
}

typedef change::change_transfer<> change_tx_type;

/*
 * Scratch management tests
 */

BOOST_FIXTURE_TEST_CASE(add_creates_a_new_file_that_can_be_written_to_on_existing_key, test_fixture)
{
	//make the existing key
	fs::create_directory(temp_root_ / "foo");

	{
		//make an existing version too
		fs::ofstream eris(temp_root_ / "foo" / "eris");
		eris << "Consult your pineal gland\n";
	}

	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	typename change_tx_type::scratch sc = sut.add("foo");

	BOOST_CHECK_EQUAL(sc.key(), "foo");
	BOOST_CHECK(sc.version() != "eris");
	BOOST_CHECK(!fs::exists(sc()));

	fs::ofstream temp(sc());

	BOOST_CHECK(temp.good());

	temp << "Hung Mung\n";
}

BOOST_FIXTURE_TEST_CASE(add_creates_a_new_file_that_can_be_written_to_on_new_key, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	typename change_tx_type::scratch sc = sut.add("foo");

	BOOST_CHECK_EQUAL(sc.key(), "foo");
	BOOST_CHECK(!fs::exists(sc()));
	BOOST_CHECK(fs::exists(temp_root_ / "foo"));

	fs::ofstream temp(sc());

	BOOST_CHECK(temp.good());

	temp << "Hung Mung\n";
}

BOOST_FIXTURE_TEST_CASE(double_add_for_same_key_retrieves_the_same_file, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	typename change_tx_type::scratch sc = sut.add("foo");

	BOOST_CHECK_EQUAL(sc.key(), "foo");
	BOOST_CHECK(!fs::exists(sc()));
	BOOST_CHECK(fs::exists(temp_root_ / "foo"));

	auto sc2 = sut.add("foo");

	BOOST_CHECK_EQUAL(sc2.version(), sc.version());
	BOOST_CHECK_EQUAL(sc2(), sc());
}

BOOST_FIXTURE_TEST_CASE(added_scratches_are_recovered, test_fixture)
{
	{
		change_tx_type sut(nodes_, temp_root_, send_handler_bound());

		typename change_tx_type::scratch sc = sut.add("foo");

		fs::ofstream eris(sc());
		eris << "Dr Van Van Mojo\n";

		BOOST_REQUIRE(eris.good());
	}
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	auto scratches = sut.scratches("foo");

	BOOST_REQUIRE_EQUAL(scratches.size(), 1);

	fs::ifstream mojo(scratches[0]());
	std::string line;

	BOOST_CHECK(mojo.good());
	BOOST_CHECK(std::getline(mojo, line));
	BOOST_CHECK_EQUAL(line, "Dr Van Van Mojo");
}


BOOST_FIXTURE_TEST_CASE(kill_deletes_a_scratch, test_fixture)
{
	{
		change_tx_type sut(nodes_, temp_root_, send_handler_bound());

		typename change_tx_type::scratch sc = sut.add("foo");

		fs::ofstream eris(sc());
		eris << "Dr Van Van Mojo\n";
	}
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	auto scratches = sut.scratches("foo");

	BOOST_REQUIRE_EQUAL(scratches.size(), 1);

	sut.kill(scratches[0]);

	BOOST_REQUIRE_EQUAL(sut.scratches("foo").size(), 0);
}

BOOST_FIXTURE_TEST_CASE(killed_scratches_are_not_recovered, test_fixture)
{
	std::string check_exists;
	{
		change_tx_type sut(nodes_, temp_root_, send_handler_bound());

		auto sc = sut.add("foo");
		check_exists = sut.close(sc);

		auto sc2 = sut.add("foo");

		sut.kill(sc2);
	}

	change_tx_type sut(nodes_, temp_root_, send_handler_bound());
	BOOST_CHECK_EQUAL(sut.scratches("foo").size(), 0);
	BOOST_CHECK_EQUAL(sut.versions("foo").size(), 1);
	BOOST_CHECK_EQUAL(sut.versions("foo")[0], check_exists);
}

BOOST_FIXTURE_TEST_CASE(killing_the_last_scratch_in_a_key_with_no_other_versions_removes_the_key, test_fixture)
{
	{
		change_tx_type sut(nodes_, temp_root_, send_handler_bound());

		auto sc = sut.add("foo");

		sut.kill(sc);
	}

	change_tx_type sut(nodes_, temp_root_, send_handler_bound());
	BOOST_CHECK_EQUAL(sut.scratches("foo").size(), 0);
	BOOST_CHECK(!sut.exists("foo"));
}

BOOST_FIXTURE_TEST_CASE(rename_creates_the_key_and_returns_new_version, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());
	auto sc = sut.add("foo");

	{
		fs::ofstream syadasti(sc());
		syadasti << "Syadasti\n";
	}

	auto new_version = sut.rename("bar", sc);

	BOOST_CHECK(!sut.exists("foo"));
	BOOST_REQUIRE_EQUAL(sut.versions("bar").size(), 1);
	BOOST_REQUIRE_EQUAL(sut.versions("bar")[0], new_version);
}

BOOST_FIXTURE_TEST_CASE(rename_new_key_and_version_are_recovered, test_fixture)
{
	std::string new_version;
	{
		change_tx_type sut(nodes_, temp_root_, send_handler_bound());
		auto sc = sut.add("foo");

		{
			fs::ofstream syadasti(sc());
			syadasti << "Syadasti\n";
		}

		new_version = sut.rename("bar", sc);
	}
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());
	BOOST_CHECK(sut.exists("bar", new_version));
}

BOOST_FIXTURE_TEST_CASE(rename_to_existing_key_throws, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	auto sc = sut.add("foo");
	auto sc2 = sut.add("bar");

	BOOST_REQUIRE_THROW(sut.rename("bar", sc), std::logic_error);
}


BOOST_FIXTURE_TEST_CASE(closing_a_scratch_generates_a_new_version_of_the_key, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	auto sc = sut.add("foo");

	{
		fs::ofstream zarathud(sc());
		zarathud << "Zarathud\n";
	}

	std::string new_version = sut.close(sc);
	BOOST_CHECK(sut.exists("foo", new_version));
}

BOOST_FIXTURE_TEST_CASE(close_new_version_is_recoverable, test_fixture)
{
	std::string new_version;
	{
		change_tx_type sut(nodes_, temp_root_, send_handler_bound());

		auto sc = sut.add("foo");

		{
			fs::ofstream zarathud(sc());
			zarathud << "Zarathud\n";
		}

		new_version = sut.close(sc);
		BOOST_CHECK(sut.exists("foo", new_version));
	}
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	BOOST_CHECK(sut.exists("foo", new_version));
}

/*
 * RPC tests: Request
 */
BOOST_FIXTURE_TEST_CASE(request_for_missing_key_gets_no_key_response, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	auto response = sut.request(change::rpc::request("foo", "bar", "baz", 0));

	BOOST_CHECK_EQUAL(response.ec(), change::rpc::response::no_key);
	BOOST_CHECK_EQUAL(response.data(), "");
}

BOOST_FIXTURE_TEST_CASE(request_for_missing_version_gets_no_version_response, test_fixture)
{
	fs::create_directory(temp_root_ / "foo");
	{
		fs::ofstream thud(temp_root_ / "foo" / "thud");
	}
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	BOOST_CHECK(sut.exists("foo"));

	auto response = sut.request(change::rpc::request("foo", "bar", "baz", 0));

	BOOST_CHECK_EQUAL(response.ec(), change::rpc::response::no_version);
	BOOST_CHECK_EQUAL(response.data(), "");
}

BOOST_FIXTURE_TEST_CASE(request_for_key_and_version_gets_ok, test_fixture)
{
	fs::create_directory(temp_root_ / "foo");
	{
		fs::ofstream malaclypse(temp_root_ / "foo" / "malaclypse");
		malaclypse << "Malaclypse the Elder\n";
	}
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	auto response = sut.request(change::rpc::request("foo", "malaclypse", "", 0));

	BOOST_CHECK_EQUAL(response.ec(), change::rpc::response::eof);
	std::istringstream encoded(response.data());
	std::ostringstream decoded;
	base64::decoder de;
	de.decode(encoded, decoded);

	BOOST_CHECK_EQUAL(decoded.str(), "Malaclypse the Elder\n");
}

BOOST_FIXTURE_TEST_CASE(request_for_key_and_large_version_gets_mutliple_chunks, test_fixture)
{
	fs::create_directory(temp_root_ / "foo");
	{
		fs::ofstream malaclypse(temp_root_ / "foo" / "malaclypse");
		malaclypse << "Malaclypse the Elder\n";
	}
	change::change_transfer<21> sut(nodes_, temp_root_, send_handler_bound());

	auto response = sut.request(change::rpc::request("foo", "malaclypse", "", 0));
	auto response2 = sut.request(change::rpc::request("foo", "malaclypse", "", 21));

	BOOST_CHECK_EQUAL(response2.ec(), change::rpc::response::eof);
	std::stringstream encoded;
	encoded << response.data() << response2.data();

	std::ostringstream decoded;
	base64::decoder de;
	de.decode(encoded, decoded);

	BOOST_CHECK_EQUAL(decoded.str(), "Malaclypse the Elder\n");
}


/*
 * RPC tests: Response
 */

BOOST_FIXTURE_TEST_CASE(response_no_key_creates_key_and_version, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());
	std::istringstream data("The Curse of Greyface\n");
	std::ostringstream encoded;
	base64::encoder enc;
	enc.encode(data, encoded);

	sut.response("eris", change::rpc::response("foo", "bar", "", 0, encoded.str(),
				change::rpc::response::eof));

	BOOST_CHECK(sut.exists("foo", "bar"));

	auto foo = sut("foo", "bar");

	fs::ifstream grey(foo);
	std::string line;
	BOOST_CHECK(std::getline(grey, line));
	BOOST_CHECK_EQUAL(line, "The Curse of Greyface");
	BOOST_CHECK(!std::getline(grey, line));
	BOOST_CHECK_EQUAL(line, "");
}

BOOST_FIXTURE_TEST_CASE(response_no_version_creates_version, test_fixture)
{
	fs::create_directory(temp_root_ / "foo");
	{
		fs::ofstream baz(temp_root_ / "foo" / "baz");
		baz << "There is no Goddess but Goddess and She is Your Goddess.\n";
	}

	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	BOOST_CHECK(sut.exists("foo"));

	std::istringstream data("The Curse of Greyface\n");
	std::ostringstream encoded;
	base64::encoder enc;
	enc.encode(data, encoded);

	sut.response("eris", change::rpc::response("foo", "bar", "", 0, encoded.str(),
				change::rpc::response::eof));

	BOOST_CHECK(sut.exists("foo", "bar"));
	BOOST_CHECK(sut.exists("foo", "baz"));

	auto foo = sut("foo", "bar");

	fs::ifstream grey(foo);
	std::string line;
	BOOST_CHECK(std::getline(grey, line));
	BOOST_CHECK_EQUAL(line, "The Curse of Greyface");
	BOOST_CHECK(!std::getline(grey, line));
	BOOST_CHECK_EQUAL(line, "");
}

BOOST_FIXTURE_TEST_CASE(response_key_version_non_pending_ignored, test_fixture)
{
	fs::create_directory(temp_root_ / "foo");
	{
		fs::ofstream baz(temp_root_ / "foo" / "baz");
		baz << "There is no Goddess but Goddess and She is Your Goddess.\n";
	}

	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	BOOST_CHECK(sut.exists("foo"));

	std::istringstream data("The Curse of Greyface\n");
	std::ostringstream encoded;
	base64::encoder enc;
	enc.encode(data, encoded);

	sut.response("eris", change::rpc::response("foo", "baz", "", 0, encoded.str(),
				change::rpc::response::eof));

	BOOST_CHECK(sut.exists("foo", "baz"));
	fs::ifstream baz(sut("foo", "baz"));
	std::string line;

	BOOST_CHECK(std::getline(baz, line));
	BOOST_CHECK_EQUAL(line,  "There is no Goddess but Goddess and She is Your Goddess.");

	BOOST_CHECK(!std::getline(baz, line));
	BOOST_CHECK_EQUAL(line, "");
}

BOOST_FIXTURE_TEST_CASE(response_with_key_and_version_eof_closes_file_if_all_complete, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	{
		std::istringstream pentabarf("A Discordian Shall Always use the Official Discordian Document Numbering System\n");
		std::ostringstream encoded;
		base64::encoder enc;
		enc.encode(pentabarf, encoded);

		sut.response("eris", change::rpc::response("foo", "pentabarf", "", 46, encoded.str(), change::rpc::response::eof));
	}

	BOOST_CHECK(sut.exists("foo"));
	BOOST_REQUIRE(!sut.exists("foo", "pentabarf")); // for it is not complete

	std::istringstream pentabarf("A Discordian shall Partake of No Hot Dog Buns\n");
	std::ostringstream encoded;
	base64::encoder enc;
	enc.encode(pentabarf, encoded);

	sut.response("eris", change::rpc::response("foo", "pentabarf", "", 0, encoded.str(), change::rpc::response::ok));
	BOOST_CHECK(sut.exists("foo", "pentabarf")); // for it is now complete

	fs::fstream foo(sut("foo", "pentabarf"));
	std::string line;
	BOOST_CHECK(std::getline(foo, line));
	BOOST_CHECK_EQUAL(line, "A Discordian shall Partake of No Hot Dog Buns");

	BOOST_CHECK(std::getline(foo, line));
	BOOST_CHECK_EQUAL(line, "A Discordian Shall Always use the Official Discordian Document Numbering System");

	BOOST_CHECK(!std::getline(foo, line));
	BOOST_CHECK_EQUAL(line, "");
}


/*
 * Poll tests
 */

BOOST_FIXTURE_TEST_CASE(tick_with_pending_version_makes_request, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	//set up the pending request
	//That base64 encodes "foo\n".
	sut.response("eris", change::rpc::response("foo", "thud", "", 0, "Zm9vCg==", change::rpc::response::ok));

	sut.tick();

	//2 because the response handler also fires one
	BOOST_REQUIRE_EQUAL(send_handler_args_.size(), 2);
	BOOST_REQUIRE_EQUAL(std::get<0>(send_handler_args_[1]), "eris");
	change::rpc::request rpc(std::get<1>(send_handler_args_[1]));
	BOOST_REQUIRE_EQUAL(rpc.key(), "foo");
	BOOST_REQUIRE_EQUAL(rpc.version(), "thud");
	BOOST_REQUIRE_EQUAL(rpc.old_version(), "");
	BOOST_REQUIRE_EQUAL(rpc.start(), 4);
}

BOOST_FIXTURE_TEST_CASE(tick_with_nothing_to_do_does_nothing, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());
	sut.tick();

	BOOST_CHECK(send_handler_args_.empty());
}


/*
 * Commit handler
 */
BOOST_FIXTURE_TEST_CASE(commit_a_new_pending_file_and_requests_fired, test_fixture)
{
	change_tx_type sut(nodes_, temp_root_, send_handler_bound());

	sut.handle_new_version("eris", "foo", "bar", "");

	BOOST_CHECK(sut.exists("foo"));

	BOOST_CHECK_EQUAL(send_handler_args_.size(), 1);
	BOOST_CHECK_EQUAL(std::get<0>(send_handler_args_[0]), "eris");

	change::rpc::request rpc(std::get<1>(send_handler_args_[0]));
	BOOST_CHECK_EQUAL(rpc.key(), "foo");
	BOOST_CHECK_EQUAL(rpc.version(), "bar");
	BOOST_CHECK_EQUAL(rpc.old_version(), "");
	BOOST_CHECK_EQUAL(rpc.start(), 0);
}

