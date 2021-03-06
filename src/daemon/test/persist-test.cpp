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
	fs::create_directory(temp_root_);
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
		fs::path p(temp_root_ / std::get<0>(key_version) / std::get<1>(key_version));
		fs::create_directory(p.parent_path());
		fs::ofstream tmp(p);

		tmp << std::get<0>(key_version) << " " << std::get<1>(key_version) << "\n";
		BOOST_CHECK(!tmp.fail());
	}

	change::persistence sut(temp_root_);

	for(const std::pair<std::string, std::string>& key_version : keys)
	{
		BOOST_CHECK_MESSAGE(sut.exists(std::get<0>(key_version)), "Check exists(" + std::get<0>(key_version) + ") failed");
		BOOST_CHECK_MESSAGE(sut.exists(std::get<0>(key_version), std::get<1>(key_version)),
				"Check exists(" + std::get<0>(key_version) + ", "
				+ std::get<1>(key_version) + ") failed");
	}
}

BOOST_FIXTURE_TEST_CASE(rename_version_to_new_version_success, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");

		fs::ofstream new_file(new_version);

		new_file << "fnord\n";
	}
	change::persistence sut(temp_root_);

	sut.rename("foo", "bar", "baz");

	BOOST_CHECK(sut.exists("baz", "bar"));
	BOOST_CHECK(!sut.exists("foo", "bar"));

	BOOST_CHECK(fs::exists(sut.root() / "baz" / "bar"));
}

BOOST_FIXTURE_TEST_CASE(rename_last_version_in_key_deletes_key_too, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");

		fs::ofstream new_file(new_version);

		new_file << "fnord\n";
	}
	change::persistence sut(temp_root_);

	sut.rename("foo", "bar", "baz");

	BOOST_CHECK(!sut.exists("foo"));
	BOOST_CHECK(!fs::exists(sut.root() / "foo"));
}

BOOST_FIXTURE_TEST_CASE(rename_version_in_key_moves_version, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");

		fs::ofstream new_file(new_version);

		new_file << "fnord\n";
	}
	change::persistence sut(temp_root_);

	sut.rename("foo", "bar", "baz");

	auto bazbar = sut("baz", "bar");

	fs::ifstream fnord(bazbar);
	std::string line;

	BOOST_CHECK(std::getline(fnord, line));
	BOOST_CHECK_EQUAL(line, "fnord");

	BOOST_CHECK(!std::getline(fnord, line));
	BOOST_CHECK_EQUAL(line, "");
}

BOOST_FIXTURE_TEST_CASE(rename_missing_version_throws, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");

		fs::ofstream new_file(new_version);

		new_file << "fnord\n";
	}
	change::persistence sut(temp_root_);

	BOOST_REQUIRE_THROW(sut.rename("foo", "baz", "fnord"), std::logic_error);
}

BOOST_FIXTURE_TEST_CASE(rename_missing_key_throws, test_fixture)
{
	change::persistence sut(temp_root_);

	BOOST_REQUIRE_THROW(sut.rename("foo", "bar", "fnord"), std::logic_error);
}

BOOST_FIXTURE_TEST_CASE(rename_existing_key_new_version_fine, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");

		fs::ofstream new_file(new_version);

		new_file << "fnord\n";

		auto existing = sut.add("fnord", "eris");

		fs::ofstream eris(existing);

		eris << "eris\n";
	}
	change::persistence sut(temp_root_);

	sut.rename("foo", "bar", "fnord");

	BOOST_CHECK(sut.exists("fnord", "bar"));
}

BOOST_FIXTURE_TEST_CASE(rename_existing_key_existing_version_throws, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");

		fs::ofstream new_file(new_version);

		new_file << "fnord\n";

		auto existing = sut.add("fnord", "bar");

		fs::ofstream eris(existing);

		eris << "eris\n";
	}
	change::persistence sut(temp_root_);

	BOOST_REQUIRE_THROW(sut.rename("foo", "bar", "fnord"), std::logic_error);
}

BOOST_FIXTURE_TEST_CASE(kill_version_success, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		{
			auto new_version = sut.add("foo", "bar");
			fs::ofstream new_file(new_version);

			new_file << "fnord\n";
		}

		{
			auto existing = sut.add("foo", "eris");

			fs::ofstream eris(existing);

			eris << "eris\n";
		}

		{
			auto existing = sut.add("fnord", "eris");

			fs::ofstream eris(existing);

			eris << "eris\n";
		}
	}
	change::persistence sut(temp_root_);

	sut.kill("foo", "eris");

	BOOST_CHECK(sut.exists("foo"));
	BOOST_CHECK(sut.exists("fnord"));
	BOOST_CHECK(!sut.exists("foo", "eris"));
	BOOST_CHECK(sut.exists("foo", "bar"));
	BOOST_CHECK(sut.exists("fnord", "eris"));
}

BOOST_FIXTURE_TEST_CASE(kill_last_version_key_deleted, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");
		fs::ofstream new_file(new_version);

		new_file << "fnord\n";
	}

	change::persistence sut(temp_root_);
	sut.kill("foo", "bar");

	BOOST_CHECK(!sut.exists("foo", "bar"));
	BOOST_CHECK(!sut.exists("foo"));

	BOOST_CHECK(!fs::exists(sut.root() / "foo"));
}

BOOST_FIXTURE_TEST_CASE(kill_no_version_throws, test_fixture)
{
	{
		change::persistence sut(temp_root_);

		auto new_version = sut.add("foo", "bar");
		fs::ofstream new_file(new_version);

		new_file << "fnord\n";
	}

	change::persistence sut(temp_root_);

	BOOST_REQUIRE_THROW(sut.kill("foo", "baz"), std::logic_error);
}

BOOST_FIXTURE_TEST_CASE(kill_no_key_throws, test_fixture)
{
	change::persistence sut(temp_root_);

	BOOST_REQUIRE_THROW(sut.kill("foo", "baz"), std::logic_error);
}

