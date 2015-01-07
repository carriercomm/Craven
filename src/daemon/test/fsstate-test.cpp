#define BOOST_TEST_MODULE "Filesystem test"
#include <boost/test/unit_test.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>

#include <json/json.h>
#include <json_help.hpp>

struct disable_logging
{
	disable_logging()
	{
		//boost::log::core::get()->set_logging_enabled(false);
	}
};

BOOST_GLOBAL_FIXTURE(disable_logging)

#include "../raftrequest.hpp"
#include "../fsstate.hpp"

struct client_mock
{
	client_mock();

	client_mock(const client_mock&) = delete;

	std::vector<Json::Value> requests_;

	template <typename Derived>
	void request(const Derived& request)
	{
		requests_.push_back(request);
	}

	template <typename Callable>
	boost::signals2::connection connect_commit_update(Callable&& f)
	{
		++commit_update_connections_;
		return commit_update_.connect(std::forward<Callable>(f));
	}

	template <typename Callable>
	boost::signals2::connection connect_commit_rename(Callable&& f)
	{
		++commit_rename_connections_;
		return commit_rename_.connect(std::forward<Callable>(f));
	}

	template <typename Callable>
	boost::signals2::connection connect_commit_delete(Callable&& f)
	{
		++commit_delete_connections_;
		return commit_delete_.connect(std::forward<Callable>(f));
	}

	template <typename Callable>
	boost::signals2::connection connect_commit_add(Callable&& f)
	{
		++commit_add_connections_;
		return commit_add_.connect(std::forward<Callable>(f));
	}

	bool exists(const std::string& key) const noexcept;

	std::tuple<std::string, std::string> operator[] (const std::string& key)
		noexcept(false);

	std::unordered_map<std::string, std::tuple<std::string, std::string>>
		keys_;

	boost::signals2::signal <void (const raft::request::Update&)> commit_update_;
	uint32_t commit_update_connections_;
	boost::signals2::signal <void (const raft::request::Rename&)> commit_rename_;
	uint32_t commit_rename_connections_;
	boost::signals2::signal <void (const raft::request::Delete&)> commit_delete_;
	uint32_t commit_delete_connections_;
	boost::signals2::signal <void (const raft::request::Add&)> commit_add_;
	uint32_t commit_add_connections_;
};

struct changetx_mock
{
	struct scratch
	{
		scratch(const std::string& key, const std::string& version,
				const boost::filesystem::path& path)
			:key_(key),
			version_(version),
			path_(path)
		{
		}


		std::string key_;
		std::string version_;
		boost::filesystem::path path_;

		std::string version() const
		{
			return version_;
		}

		std::string key() const
		{
			return key_;
		}

		boost::filesystem::path operator()()
		{
			return path_;
		}
	};

	changetx_mock();
	changetx_mock(const changetx_mock&) = delete;

	template <typename Callable>
	boost::signals2::connection connect_arrival_notifications(Callable&& f)
	{
		++connections_;
		return notify_arrival_.connect(std::forward<Callable>(f));
	}

	bool exists(const std::string& key) const;
	bool exists(const std::string& key, const std::string& version) const;

	std::unordered_multimap<std::string, std::string> existing_entries_;

	void copy(const std::string& key, const std::string& version,
			const std::string& new_key)
	{
		copy_args_.push_back(std::make_tuple(key, version, new_key));
	}

	std::vector<std::tuple<std::string, std::string, std::string>>
		copy_args_;

	scratch move(const std::string& new_key, const scratch& scratch_info)
	{
		move_args_.emplace_back(new_key, scratch_info);

		scratch new_scratch = scratch_info;
		new_scratch.key_ = new_key;
		new_scratch.path_ = dfs::decode_path(new_key);

		return new_scratch;
	}

	std::vector<std::tuple<std::string, scratch>> move_args_;


	boost::signals2::signal<void (const std::string&, const std::string&)> notify_arrival_;
	uint32_t connections_;

};


// Exposes all of the internals because the fuse interface is meh.
// This is technically bad testing form, but again, meh.
struct State : dfs::basic_state<client_mock, changetx_mock>
{
	typedef dfs::basic_state<client_mock, changetx_mock> base_type;

	State(client_mock& cm, changetx_mock& ctxm)
		:dfs::basic_state<client_mock, changetx_mock>(cm, ctxm)
	{
	}

	State(const State&) = delete;

	using base_type::node_info;
	using base_type::dcache_;
	using base_type::rcache_;
	using base_type::fusetl_;
	using base_type::sync_cache_;

};

struct test_fixture
{
	test_fixture();
	client_mock client_;
	changetx_mock changetx_;
};

test_fixture::test_fixture()
	:client_(),
	changetx_()
{
}

client_mock::client_mock()
	:commit_update_connections_(0),
	commit_rename_connections_(0),
	commit_delete_connections_(0),
	commit_add_connections_(0)
{
}

bool client_mock::exists(const std::string& key) const noexcept
{
	return keys_.count(key) == 1;
}

std::tuple<std::string, std::string> client_mock::operator[] (
		const std::string& key) noexcept(false)
{
	return keys_.at(key);
}

changetx_mock::changetx_mock()
	:connections_(0)
{
}

bool changetx_mock::exists(const std::string& key) const
{
	return existing_entries_.count(key) > 0;
}

bool changetx_mock::exists(const std::string& key,
		const std::string& version) const
{
	auto key_range = existing_entries_.equal_range(key);
	auto it = boost::range::find_if(key_range,
			[=](const std::pair<std::string, std::string>& value)
			{
				return value.second == version;
			});

	return it != key_range.second;
}

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

/*
 * Init tests
 */

BOOST_FIXTURE_TEST_CASE(handlers_installed, test_fixture)
{
	State sut(client_, changetx_);

	BOOST_CHECK_EQUAL(client_.commit_update_connections_, 1);
	BOOST_CHECK_EQUAL(client_.commit_rename_connections_, 1);
	BOOST_CHECK_EQUAL(client_.commit_delete_connections_, 1);
	BOOST_CHECK_EQUAL(client_.commit_add_connections_, 1);

	BOOST_CHECK_EQUAL(changetx_.connections_, 1);
}

/*
 * Commit tests
 */

BOOST_FIXTURE_TEST_CASE(add_keys_are_decoded, test_fixture)
{
	changetx_.existing_entries_ = {{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}};
	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo/bar"].size(), 2);
	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());
}

BOOST_FIXTURE_TEST_CASE(add_entries_for_every_parent_dir, test_fixture)
{
	changetx_.existing_entries_ = {{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 1);
}

BOOST_FIXTURE_TEST_CASE(add_node_data_for_added_key_correct, test_fixture)
{
	changetx_.existing_entries_ = {{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo/bar"].size(), 2);
	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->type, State::node_info::file);
	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);

	//Not a directory, so the inode number doesn't matter

	BOOST_CHECK(!node_it->rename_info);
	BOOST_CHECK(!node_it->scratch_info);
	BOOST_CHECK(!node_it->previous_version);
	BOOST_CHECK_EQUAL(node_it->name, "baz");
	BOOST_CHECK_EQUAL(node_it->version,
			"a4e3e1394621ec2301076e39c6e5585bb1d665dc");
}

BOOST_FIXTURE_TEST_CASE(add_for_key_that_clashes_leaves_state_correct, test_fixture)
{
	State sut(client_, changetx_);

	//set up the existing key
	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo/bar"].size(), 2);
	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	node_it->state = State::node_info::novel;
	sut.sync_cache_["/foo/bar/baz"].push_back(*node_it);
	sut.sync_cache_["/foo/bar/baz"].back().name = "/foo/bar/baz";

	//Commit a clashing entry
	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	//Not active; no translation
	BOOST_REQUIRE_EQUAL(sut.fusetl_.size(), 0);

	auto redir_it = boost::range::find_if(
			sut.dcache_["/foo/bar"],
			[=](const State::node_info& ni) -> bool
			{
				return ni.name != "." && ni.name != "baz";
			});

	BOOST_REQUIRE(redir_it != sut.dcache_["/foo/bar"].end());
	BOOST_CHECK_EQUAL(redir_it->type, State::node_info::file);
	BOOST_CHECK_EQUAL(redir_it->state, State::node_info::novel);

	std::string expected_name = "/foo/bar/" + redir_it->name;

	BOOST_REQUIRE_EQUAL(sut.sync_cache_.size(), 1);
	BOOST_REQUIRE_EQUAL(sut.sync_cache_[expected_name].size(), 1);
	BOOST_REQUIRE_EQUAL(sut.sync_cache_[expected_name].front().name, expected_name);
}

BOOST_FIXTURE_TEST_CASE(add_for_pending_version_marked, test_fixture)
{
	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo/bar"].size(), 2);
	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::pending);
	BOOST_REQUIRE(!node_it->previous_version);

	//and now we notify of an arrival
	changetx_.notify_arrival_("%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc");

	node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);

}

BOOST_FIXTURE_TEST_CASE(rename_key_exists_and_synced_executes_rename, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fthud",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	sut.commit_rename(raft::request::Rename("eris",
				"%2ffoo%2fbar%2fbaz", "%2ffoo%2fthud",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/thud"), 0);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 0);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar/baz"), 0);

	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo"].size(), 2);
	auto node_it = boost::range::find_if(sut.dcache_["/foo"],
			[](const State::node_info& info)
			{
				return info.name == "thud";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo"].end());
	BOOST_CHECK_EQUAL(node_it->version,
			"a4e3e1394621ec2301076e39c6e5585bb1d665dc");
	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);
}

BOOST_FIXTURE_TEST_CASE(rename_key_exists_and_is_dirty_moves_and_handles_state, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fthud",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fthud",
				"5898511673f223c4adb65ddce23981a2d87dec5c"}
		};

	//for the dirty node
	client_.keys_["%2ffoo%2fthud"] = std::make_tuple("a4e3e1394621ec2301076e39c6e5585bb1d665dc", "eris");

	State sut(client_, changetx_);

	//set up the existing key
	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	//Set up the clashing key
	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fthud",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo"].size(), 3);
	auto node_it = boost::range::find_if(sut.dcache_["/foo"],
			[](const State::node_info& info)
			{
				return info.name == "thud";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo"].end());

	node_it->state = State::node_info::dirty;
	sut.sync_cache_["/foo/thud"].push_back(*node_it);
	sut.sync_cache_["/foo/thud"].back().name = "/foo/thud";

	//Commit test
	sut.commit_rename(raft::request::Rename("eris",
				"%2ffoo%2fbar%2fbaz", "%2ffoo%2fthud",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));
	//
	//Not active; no translation
	BOOST_REQUIRE_EQUAL(sut.fusetl_.size(), 0);

	auto redir_it = boost::range::find_if(
			sut.dcache_["/foo"],
			[=](const State::node_info& ni) -> bool
			{
				return ni.name != "." && ni.name != "thud";
			});

	BOOST_REQUIRE(redir_it != sut.dcache_["/foo"].end());
	BOOST_CHECK_EQUAL(redir_it->type, State::node_info::file);
	BOOST_CHECK_EQUAL(redir_it->state, State::node_info::novel);
	BOOST_CHECK_EQUAL(redir_it->version,
			"5898511673f223c4adb65ddce23981a2d87dec5c");

	std::string expected_name = "/foo/" + redir_it->name;

	BOOST_REQUIRE_EQUAL(sut.sync_cache_.size(), 1);
	BOOST_REQUIRE_EQUAL(sut.sync_cache_[expected_name].size(), 1);
	BOOST_REQUIRE_EQUAL(sut.sync_cache_[expected_name].front().name, expected_name);

}

BOOST_FIXTURE_TEST_CASE(rename_key_exists_and_is_active_moves_and_handles_state, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fthud",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}
		};

	State sut(client_, changetx_);

	//set up the existing key
	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	//Set up the clashing key
	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fthud",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo"].size(), 3);
	auto node_it = boost::range::find_if(sut.dcache_["/foo"],
			[](const State::node_info& info)
			{
				return info.name == "thud";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo"].end());

	node_it->state = State::node_info::active_write;
	node_it->scratch_info = changetx_mock::scratch(
			dfs::encode_path("/foo/thud"), ".scratch",
			"/does/not/exist");

	//Commit test
	sut.commit_rename(raft::request::Rename("eris",
				"%2ffoo%2fbar%2fbaz", "%2ffoo%2fthud",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	//Check the moved node
	BOOST_REQUIRE_EQUAL(sut.fusetl_.size(), 1);
	BOOST_CHECK_EQUAL(std::get<0>(sut.fusetl_["/foo/thud"]), State::dcache);
	boost::filesystem::path redirect = std::get<1>(sut.fusetl_["/foo/thud"]);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count(redirect.parent_path().string()), 1);

	auto redir_it = boost::range::find_if(
			sut.dcache_[redirect.parent_path().string()],
			[=](const State::node_info& ni) -> bool
			{
				return ni.name == redirect.filename();
			});

	BOOST_REQUIRE(redir_it != sut.dcache_[redirect.parent_path().string()].end());
	BOOST_CHECK_EQUAL(redir_it->type, State::node_info::file);
	BOOST_CHECK_EQUAL(redir_it->state, State::node_info::active_write);

}

BOOST_FIXTURE_TEST_CASE(rename_key_does_not_exist_acts_like_add, test_fixture)
{
	State sut(client_, changetx_);

	//Commit test
	sut.commit_rename(raft::request::Rename("eris",
				"%2ffoo%2fbar%2fbaz", "%2ffoo%2fthud",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo"],
			[](const State::node_info& info)
			{
				return info.name == "thud";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::pending);
	BOOST_CHECK_EQUAL(node_it->version,
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc");
}


BOOST_FIXTURE_TEST_CASE(update_normal_executes, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	sut.commit_update(raft::request::Update("eris",
				"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);
	BOOST_CHECK_EQUAL(node_it->version,
				"5898511673f223c4adb65ddce23981a2d87dec5c");
}

BOOST_FIXTURE_TEST_CASE(update_dirty_conflict_manages, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"},
		{"%2ffoo%2fbar%2fbaz",
				"cafe504e8aaf2f1d1f4207be9fbc37edc5c042b1"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"cafe504e8aaf2f1d1f4207be9fbc37edc5c042b1"));

	auto added = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});
	BOOST_REQUIRE(added != sut.dcache_["/foo/bar"].end());
	added->state = State::node_info::dirty;
	sut.sync_cache_["/foo/bar/baz"].push_back(*added);
	sut.sync_cache_["/foo/bar/baz"].back().name = "/foo/bar/baz";

	// Exercise
	sut.commit_update(raft::request::Update("eris",
				"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	// Check the update applied
	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 3);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);
	BOOST_CHECK_EQUAL(node_it->version,
				"5898511673f223c4adb65ddce23981a2d87dec5c");

	// Check the conflict management worked
	node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name != "." && info.name != "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::novel);
	BOOST_CHECK_EQUAL(node_it->version,
				"cafe504e8aaf2f1d1f4207be9fbc37edc5c042b1");

	BOOST_CHECK_EQUAL(sut.sync_cache_.size(), 1);
	BOOST_CHECK_EQUAL(sut.sync_cache_.begin()->second.size(), 1);
	BOOST_CHECK_EQUAL(sut.sync_cache_.begin()->second.front().version,
				"cafe504e8aaf2f1d1f4207be9fbc37edc5c042b1");
	BOOST_CHECK_EQUAL(sut.sync_cache_.begin()->second.front().state,
			State::node_info::novel);
}

BOOST_FIXTURE_TEST_CASE(update_active_write_conflict_and_translate, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	auto added = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});
	BOOST_REQUIRE(added != sut.dcache_["/foo/bar"].end());
	added->state = State::node_info::active_read;

	// Exercise
	sut.commit_update(raft::request::Update("eris",
				"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	// Check the update applied
	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);
	BOOST_CHECK_EQUAL(node_it->version,
				"5898511673f223c4adb65ddce23981a2d87dec5c");

	// Check the conflict management worked
	BOOST_REQUIRE_EQUAL(sut.fusetl_.size(), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(sut.fusetl_["/foo/bar/baz"]), State::rcache);
	BOOST_REQUIRE_EQUAL(sut.rcache_.size(), 1);

	BOOST_REQUIRE_EQUAL(sut.rcache_.count("/foo/bar/baz"), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(sut.rcache_["/foo/bar/baz"]),
			"%2ffoo%2fbar%2fbaz");
	BOOST_REQUIRE_EQUAL(std::get<1>(sut.rcache_["/foo/bar/baz"]),
			"a4e3e1394621ec2301076e39c6e5585bb1d665dc");
}

BOOST_FIXTURE_TEST_CASE(update_no_key_acts_like_add, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"}
		};

	State sut(client_, changetx_);

	// Exercise
	sut.commit_update(raft::request::Update("eris",
				"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	// Check the update applied
	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);
	BOOST_CHECK_EQUAL(node_it->version,
				"5898511673f223c4adb65ddce23981a2d87dec5c");
}

BOOST_FIXTURE_TEST_CASE(update_for_node_synced_but_wrong_version_overwrites, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"},
		{"%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"},
		{"%2ffoo%2fbar%2fbaz",
				"cafe504e8aaf2f1d1f4207be9fbc37edc5c042b1"}
		};

	State sut(client_, changetx_);

	//Add wrong version
	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"cafe504e8aaf2f1d1f4207be9fbc37edc5c042b1"));

	// Exercise
	sut.commit_update(raft::request::Update("eris",
				"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	// Check the update applied
	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);
	BOOST_CHECK_EQUAL(node_it->version,
				"5898511673f223c4adb65ddce23981a2d87dec5c");
}

BOOST_FIXTURE_TEST_CASE(update_for_pending_version_marked, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	sut.commit_update(raft::request::Update("eris",
				"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::pending);
	BOOST_CHECK_EQUAL(node_it->version,
				"5898511673f223c4adb65ddce23981a2d87dec5c");
	BOOST_REQUIRE(node_it->previous_version);
	BOOST_REQUIRE_EQUAL(*node_it->previous_version,
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc");


	changetx_.existing_entries_.insert(std::make_pair("%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	changetx_.notify_arrival_("%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c");

	node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->state, State::node_info::clean);
	BOOST_CHECK_EQUAL(node_it->version,
				"5898511673f223c4adb65ddce23981a2d87dec5c");
	BOOST_REQUIRE(!node_it->previous_version);
}

BOOST_FIXTURE_TEST_CASE(delete_key_and_version_correct_executes, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	sut.commit_delete(raft::request::Delete("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo"), 0);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 0);
}

BOOST_FIXTURE_TEST_CASE(delete_wrong_version_conflict_and_recover, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	node_it->state = State::node_info::novel;
	sut.sync_cache_["/foo/bar/baz"].push_back(*node_it);
	sut.sync_cache_["/foo/bar/baz"].back().name = "/foo/bar/baz";


	sut.commit_delete(raft::request::Delete("eris", "%2ffoo%2fbar%2fbaz",
				"5898511673f223c4adb65ddce23981a2d87dec5c"));

	//check that the delete wasn't applied.
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_["/foo/bar"].size(), 2);
	node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name != ".";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	BOOST_CHECK_EQUAL(node_it->type, State::node_info::file);
	BOOST_CHECK_EQUAL(node_it->state, State::node_info::novel);

	//Not a directory, so the inode number doesn't matter

	BOOST_CHECK(!node_it->rename_info);
	BOOST_CHECK(!node_it->scratch_info);
	BOOST_CHECK(!node_it->previous_version);
	BOOST_CHECK_EQUAL(node_it->version,
			"a4e3e1394621ec2301076e39c6e5585bb1d665dc");

}

BOOST_FIXTURE_TEST_CASE(delete_tombstone_clean_and_remove_from_sync_cache, test_fixture)
{
	changetx_.existing_entries_ = {
		{"%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"}
		};

	State sut(client_, changetx_);

	sut.commit_add(raft::request::Add("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_CHECK_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_.count("/foo/bar"), 1);
	BOOST_CHECK_EQUAL(sut.dcache_["/foo/bar"].size(), 2);

	auto node_it = boost::range::find_if(sut.dcache_["/foo/bar"],
			[](const State::node_info& info)
			{
				return info.name == "baz";
			});

	BOOST_REQUIRE(node_it != sut.dcache_["/foo/bar"].end());

	node_it->state = State::node_info::dead;
	sut.sync_cache_["/foo/bar/baz"].push_back(*node_it);
	sut.sync_cache_["/foo/bar/baz"].back().name = "/foo/bar/baz";

	sut.commit_delete(raft::request::Delete("eris", "%2ffoo%2fbar%2fbaz",
				"a4e3e1394621ec2301076e39c6e5585bb1d665dc"));

	BOOST_REQUIRE_EQUAL(sut.sync_cache_.size(), 0);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/"), 1);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo"), 0);
	BOOST_REQUIRE_EQUAL(sut.dcache_.count("/foo/bar"), 0);
}

