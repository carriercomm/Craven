#define BOOST_TEST_MODULE "Filesystem test"
#include <boost/test/unit_test.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

struct disable_logging
{
	disable_logging()
	{
		boost::log::core::get()->set_logging_enabled(false);
	}
};

BOOST_GLOBAL_FIXTURE(disable_logging)

#include "../raftrequest.hpp"
#include "../fsstate.hpp"

struct client_mock
{

};

struct changetx_mock
{
	struct scratch
	{

	};

};


typedef dfs::basic_state<client_mock, changetx_mock> State;

struct test_fixture
{

};

BOOST_AUTO_TEST_CASE(urlencode_exercise)
{
	BOOST_CHECK_EQUAL(dfs::encode_path("foo bar"), "foo%20bar");
	BOOST_CHECK_EQUAL(dfs::encode_path("foo/bar"), "foo%2fbar");
	BOOST_CHECK_EQUAL(dfs::encode_path("Hail Eris!"), "Hail%20Eris%21");
}

BOOST_AUTO_TEST_CASE(urldecode_exercise)
{
	BOOST_CHECK_EQUAL(dfs::decode_path("foo%20bar"), "foo bar");
	BOOST_CHECK_EQUAL(dfs::decode_path("foo%2fbar"), "foo/bar");
	BOOST_CHECK_EQUAL(dfs::decode_path("Hail%20Eris%21"), "Hail Eris!");
}

BOOST_AUTO_TEST_CASE(urlencode_decode_matches)
{
	BOOST_CHECK_EQUAL(dfs::decode_path(dfs::encode_path(
					"foo bar baz/fnord!hello:hi_how~are,.//you?")),
					"foo bar baz/fnord!hello:hi_how~are,.//you?");
}
