#define BOOST_TEST_MODULE "Expansion"
#include <boost/test/unit_test.hpp>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <cstdlib>
#include <string>
#include <fstream>

#include "../configure.hpp"

//define a class to expose expansion
struct expose_expansion : Configure
{
	expose_expansion()
	:Configure(0, nullptr)
	{}

	fs::path expansion_passthrough(fs::path const& path)
	{
		return expand(path);
	}

};

// Test cases defined on page 32 of logbook.

BOOST_AUTO_TEST_CASE(tilde_expansion)
{
	//get the home directory from the environment
	fs::path home(getenv("HOME"));

	expose_expansion sut;

	BOOST_REQUIRE_EQUAL(sut.expansion_passthrough("~"), home);
}

BOOST_AUTO_TEST_CASE(env_variable_expansion)
{
	//get the home directory from the environment
	fs::path home(getenv("HOME"));

	expose_expansion sut;

	BOOST_REQUIRE_EQUAL(sut.expansion_passthrough("$HOME"), home);
}

BOOST_AUTO_TEST_CASE(env_variable_brace_expansion)
{
	//get the home directory from the environment
	fs::path home(getenv("HOME"));

	expose_expansion sut;

	BOOST_REQUIRE_EQUAL(sut.expansion_passthrough("${HOME}"), home);
}

BOOST_AUTO_TEST_CASE(command_expansion)
{
	expose_expansion sut;

	BOOST_REQUIRE_EQUAL(sut.expansion_passthrough("$(echo foo)"), "foo");
}

BOOST_AUTO_TEST_CASE(syntax_throws)
{
	expose_expansion sut;

	BOOST_REQUIRE_THROW(sut.expansion_passthrough("$("), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(glob_produces_first)
{
	fs::path tmp = fs::temp_directory_path() / fs::unique_path();
	fs::create_directory(tmp);

	{

		std::ofstream foo((tmp / "foo").string());
		foo << "Fnord\n";
		std::ofstream bar((tmp / "bar").string());
		bar << "Thud\n";
	}
	BOOST_REQUIRE(fs::exists(tmp / "foo"));
	BOOST_REQUIRE(fs::exists(tmp / "bar"));

	expose_expansion sut;

	BOOST_REQUIRE_EQUAL(sut.expansion_passthrough(tmp / "/*"), (tmp / "bar"));

	fs::remove_all(tmp);
}
