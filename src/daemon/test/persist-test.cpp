#define BOOST_TEST_MODULE "Connection pool test"
#include <boost/test/unit_test.hpp>

#include <string>
#include <fstream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;

#include "../persist.hpp"

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
}

test_fixture::~test_fixture()
{
	fs::remove_all(temp_root_);
}

BOOST_FIXTURE_TEST_CASE(functor_throws_with_empty_root, test_fixture)
{
	change::persistence sut(temp_root_);

	BOOST_REQUIRE_THROW(sut("foo", "bar"), std::logic_error);
}

BOOST_FIXTURE_TEST_CASE(add_does_not_write_to_new_file, test_fixture)
{
	change::persistence sut(temp_root_);

	auto new_version = sut.add("foo", "bar");

	BOOST_REQUIRE(sut.exists("foo"));
	BOOST_REQUIRE(sut.exists("foo", "bar"));
	BOOST_REQUIRE(!sut.exists("foo", "baz"));
	BOOST_REQUIRE(!sut.exists("fnord"));

	BOOST_REQUIRE(!fs::exists(new_version));
}

BOOST_FIXTURE_TEST_CASE(writing_to_file_from_add_is_recoverable, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");

		fs::ofstream new_file(new_version);

		new_file << "fnord\n";
	}
	change::persistence sut(temp_root_);

	BOOST_REQUIRE(sut.exists("foo"));
	BOOST_REQUIRE(sut.exists("foo", "bar"));

	auto recovered = sut("foo", "bar");

	fs::ifstream recovered_file(recovered);
	std::string line;
	BOOST_CHECK(std::getline(recovered_file, line));

	BOOST_CHECK_EQUAL(line, "fnord");
	BOOST_CHECK(!std::getline(recovered_file, line));
	BOOST_CHECK(recovered_file.eof());
	BOOST_CHECK_EQUAL(line, "");
}

BOOST_FIXTURE_TEST_CASE(existing_keys_and_versions_are_recovered, test_fixture)
{
	std::unordered_multimap<std::string, std::string> keys{
		{"foo", "fnord"},
		{"foo", "thud"},
		{"bar", "eris"},
		{"baz", "discord"}
	};

	for(const std::pair<std::string, std::string>& key_version : keys)
	{
		fs::fstream tmp(temp_root_ / std::get<0>(key_version) / std::get<0>(key_version));

		tmp << std::get<0>(key_version) << " " << std::get<1>(key_version) << "\n";
	}

	change::persistence sut(temp_root_);

	for(const std::pair<std::string, std::string>& key_version : keys)
	{
		BOOST_CHECK(sut.exists(std::get<0>(key_version)));
		BOOST_CHECK(sut.exists(std::get<0>(key_version), std::get<1>(key_version)));
	}
}

