#define BOOST_TEST_MODULE "Dispatch test"
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>
#include <tuple>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <json/json.h>
#include "../../common/json_help.hpp"

#include "../dispatch.hpp"

struct disable_logging
{
	disable_logging()
	{
		boost::log::core::get()->set_logging_enabled(false);
	}
};

BOOST_GLOBAL_FIXTURE(disable_logging)


struct callback_details
{
	callback_details()
		:valid_(false)
	{}

	bool valid_;
	std::string endpoint_;
	std::vector<std::string> msgs_;
};

struct connection_pool_mock
{
	typedef std::string uid_type;

	class Callback
	{
	public:
		Callback()
			:details_(nullptr)
		{}

		Callback(callback_details* details)
			:details_(details)
		{}

		Callback(callback_details& details)
			:details_(&details)
		{}

		struct invalid_callback : std::runtime_error
		{
			invalid_callback()
				:std::runtime_error("Callback not initialised")
			{}
		};

		operator bool() const
		{
			return details_ != nullptr && details_->valid_;
		}

		std::string endpoint() const
		{
			throw_if_invalid();

			return details_->endpoint_;
		}

		void operator()(const std::string& msg)
		{
			throw_if_invalid();
			details_->msgs_.push_back(msg);
		}


	private:
		inline void throw_if_invalid() const
		{
			if(details_ == nullptr)
				throw invalid_callback();
		}

		callback_details* details_;
	};

	void send_targeted(const std::string& to, const std::string& msg)
	{
		send_targeted_args_.push_back(std::make_tuple(to, msg));
	};

	bool exists(const std::string& node) const
	{
		return true;
	}

	std::vector<std::tuple<std::string, std::string>> send_targeted_args_;
};

typedef  TopLevelDispatch<connection_pool_mock> dispatch_type;

struct Module
{

	std::function<void (const Json::Value&, const dispatch_type::Callback&)> handler()
	{
		return [this](const Json::Value& msg, const dispatch_type::Callback& cb)
		{
			auto json_string = json_help::write(msg);
			json_string.pop_back();
			calls_.push_back(std::make_tuple(json_string, cb));
		};
	}

	std::vector<std::tuple<std::string, dispatch_type::Callback>> calls_;
};

BOOST_AUTO_TEST_CASE(dispatch_targets_pass_through)
{
	Module m1;
	Module m2;

	connection_pool_mock cpm;
	dispatch_type sut(cpm);

	sut.connect_dispatcher("m1", m1.handler());
	sut.connect_dispatcher("m2", m2.handler());

	callback_details cbd1;
	callback_details cbd2;

	connection_pool_mock::Callback cb1(cbd1);
	connection_pool_mock::Callback cb2(cbd2);

	sut(R"({"module":"m1","reply":"thud","content":"foobar"})", cb1);

	BOOST_REQUIRE_EQUAL(m1.calls_.size(), 1);
	BOOST_REQUIRE_EQUAL(m2.calls_.size(), 0);

	BOOST_REQUIRE_EQUAL(std::get<0>(m1.calls_[0]), "\"foobar\"");

	sut(R"({"module":"m2","reply":"fnord","content":{"Hail!":"Eris"}})", cb2);
	BOOST_REQUIRE_EQUAL(m1.calls_.size(), 1);
	BOOST_REQUIRE_EQUAL(m2.calls_.size(), 1);

	BOOST_REQUIRE_EQUAL(std::get<0>(m2.calls_[0]), R"({"Hail!":"Eris"})");
}

BOOST_AUTO_TEST_CASE(callbacks_pass_through)
{
	Module m1;

	connection_pool_mock cpm;
	dispatch_type sut(cpm);

	sut.connect_dispatcher("m1", m1.handler());

	callback_details cbd1;

	connection_pool_mock::Callback cb1(cbd1);

	sut(R"({"module":"m1","reply":"fnord","content":"foobar"})", cb1);

	BOOST_REQUIRE_EQUAL(m1.calls_.size(), 1);

	Json::Value root;
	Json::Value thing;
	thing["Hail!"] = "Eris";
	root["fnord"] = thing;

	std::get<1>(m1.calls_[0])(root);

	BOOST_REQUIRE_EQUAL(cbd1.msgs_.size(), 1);

	Json::Value expected_root;
	expected_root["module"] = "fnord";
	expected_root["reply"] = "m1";
	expected_root["content"]["fnord"]["Hail!"] = "Eris";

	Json::Value actual_root;
	Json::Reader r;
	r.parse(cbd1.msgs_[0], actual_root);

	BOOST_REQUIRE_EQUAL(actual_root, expected_root);
}

BOOST_AUTO_TEST_CASE(dispatch_to_unknown_silent)
{
	Module m1;

	connection_pool_mock cpm;
	dispatch_type sut(cpm);

	sut.connect_dispatcher("m1", m1.handler());

	callback_details cbd1;

	connection_pool_mock::Callback cb1(cbd1);

	sut(R"({"module":"notamodule","reply":"fnord","content":"foobar"})", cb1);


	BOOST_REQUIRE_EQUAL(m1.calls_.size(), 0);
	BOOST_REQUIRE_EQUAL(cbd1.msgs_.size(), 0);
}

BOOST_AUTO_TEST_CASE(connecting_existing_handler_throws)
{
	Module m1;
	Module m2;

	connection_pool_mock cpm;
	dispatch_type sut(cpm);

	sut.connect_dispatcher("m1", m1.handler());
	BOOST_REQUIRE_THROW(sut.connect_dispatcher("m1", m2.handler()), dispatch_type::dispatcher_exists);

}

BOOST_AUTO_TEST_CASE(disconnecting_non_existant_hander_silent)
{
	connection_pool_mock cpm;
	dispatch_type sut(cpm);
	sut.disconnect("fnoooord");
}

BOOST_AUTO_TEST_CASE(disconnecting_dispatch_throws)
{
	connection_pool_mock cpm;
	dispatch_type sut(cpm);
	BOOST_REQUIRE_THROW(sut.disconnect("dispatch"), dispatch_type::invalid_name);
}

BOOST_AUTO_TEST_CASE(bad_json_parse_error_response)
{
	Module m1;

	connection_pool_mock cpm;
	dispatch_type sut(cpm);

	sut.connect_dispatcher("m1", m1.handler());

	callback_details cbd1;

	connection_pool_mock::Callback cb1(cbd1);

	sut(R"({"module":"notamodule")", cb1);

	BOOST_REQUIRE_EQUAL(cbd1.msgs_.size(), 1);

	Json::Value root;
	Json::Reader r;
	r.parse(cbd1.msgs_[0], root);

	BOOST_REQUIRE_EQUAL(root["module"].asString(), "dispatch");
	BOOST_REQUIRE_EQUAL(root["reply"].asString(), "dispatch");
	BOOST_REQUIRE(root["content"].isMember("error"));
}

BOOST_AUTO_TEST_CASE(bad_json_bad_format)
{
	Module m1;

	connection_pool_mock cpm;
	dispatch_type sut(cpm);

	sut.connect_dispatcher("m1", m1.handler());

	callback_details cbd1;

	connection_pool_mock::Callback cb1(cbd1);

	sut(R"({"foo":"bar"})", cb1);

	BOOST_REQUIRE_EQUAL(cbd1.msgs_.size(), 1);

	Json::Value root;
	Json::Reader r;
	r.parse(cbd1.msgs_[0], root);

	BOOST_REQUIRE_EQUAL(root["module"].asString(), "dispatch");
	BOOST_REQUIRE_EQUAL(root["reply"].asString(), "dispatch");
	BOOST_REQUIRE(root["content"].isMember("error"));

}
