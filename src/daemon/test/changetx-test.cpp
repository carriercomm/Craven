#define BOOST_TEST_MODULE "Connection pool test"
#include <boost/test/unit_test.hpp>

#include <string>
#include <functional>
#include <fstream>
#include <set>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <json/json.h>
#include <json_help.hpp>

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

BOOST_FIXTURE_TEST_CASE(add_creates_a_new_file_that_can_be_written_to_on_existing_key, test_fixture)
{
	BOOST_REQUIRE(false);
}

BOOST_FIXTURE_TEST_CASE(add_creates_a_new_file_that_can_be_written_to_on_new_key, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(double_add_for_same_key_retrieves_the_same_file, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(added_scratches_are_recovered, test_fixture)
{

}


BOOST_FIXTURE_TEST_CASE(kill_deletes_a_scratch, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(killed_scratches_are_not_recovered, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(killing_the_last_scratch_in_a_key_with_no_other_versions_removes_the_key, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(rename_creates_the_key_and_returns_new_version, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(rename_new_key_and_version_are_recovered, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(rename_to_existing_key_throws, test_fixture)
{

}


BOOST_FIXTURE_TEST_CASE(closing_a_scratch_generates_a_new_version_of_the_key, test_fixture)
{

}

BOOST_FIXTURE_TEST_CASE(close_new_version_is_recoverable, test_fixture)
{

}
