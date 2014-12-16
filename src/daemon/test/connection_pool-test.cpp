#define BOOST_TEST_MODULE "Connection pool test"
#include <boost/test/unit_test.hpp>

#include <string>
#include <memory>
#include <map>

#include "../../common/connection.hpp"
#include "../connection_pool.hpp"

struct connection_mock
{
	typedef std::shared_ptr<connection_mock> pointer;
	connection_mock()
		:open_(true)
	{}

	void queue_write(const std::string& msg)
	{
		write_queue_.push_back(msg);
	}

	template <typename Callable>
	void connect_read(Callable&& f)
	{
		read_handler_ = f;
	}

	template <typename Callable>
	void connect_close(Callable&& f)
	{
		close_handler_ = f;
	}

	bool is_open()
	{
		return open_;
	}

	void close()
	{
		open_ = false;
		close_handler_();
	}


	std::function<void(const std::string& msg)> read_handler_;
	std::function<void(void)> close_handler_;
	std::vector<std::string> write_queue_;
	bool open_;
};

struct dispatch_mock
{
	void operator()(const std::string& msg, const ConnectionPool<connection_mock, dispatch_mock>::Callback& callback)
	{
		calls.push_back(std::make_tuple(msg, callback.endpoint()));
		last_callback = callback;
	}

	ConnectionPool<connection_mock, dispatch_mock>::Callback last_callback;

	std::vector<std::tuple<std::string, std::string>> calls;
};

typedef ConnectionPool<connection_mock, dispatch_mock> connection_pool_type;

BOOST_AUTO_TEST_CASE(read_handler_to_dispatch)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	connection_pool_type::uid_type cm_uid("1");

	connection_pool_type sut(dm, {{cm_uid, cm}});

	cm->read_handler_("Foo bar baz test message please ignore");

	BOOST_REQUIRE_EQUAL(dm.calls.size(), 1);
	BOOST_REQUIRE_EQUAL(std::get<0>(dm.calls[0]), "Foo bar baz test message please ignore");
	BOOST_REQUIRE_EQUAL(std::get<1>(dm.calls[0]), cm_uid);
}

BOOST_AUTO_TEST_CASE(dispatch_callback_to_read_handler)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	auto cm2 = std::make_shared<connection_mock>();
	connection_pool_type::uid_type cm_uid("1");
	connection_pool_type::uid_type cm_uid2("2");

	connection_pool_type sut(dm, {{cm_uid, cm}, {cm_uid2, cm2}});

	cm->read_handler_("Foo bar baz test message please ignore");

	dm.last_callback("Fnord foo bar");

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 1);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 0);

	BOOST_REQUIRE_EQUAL(cm->write_queue_[0], "Fnord foo bar");
}

BOOST_AUTO_TEST_CASE(broadcast_writes_to_all)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	auto cm2 = std::make_shared<connection_mock>();

	auto not_added = std::make_shared<connection_mock>();

	connection_pool_type::uid_type cm_uid("1");
	connection_pool_type::uid_type cm_uid2("2");

	connection_pool_type sut(dm, {{cm_uid, cm}, {cm_uid2, cm2}});

	sut.broadcast("Test message please ignore\n");

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 1);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 1);

	//Though it'd be a miracle if it managed it!
	BOOST_REQUIRE_EQUAL(not_added->write_queue_.size(), 0);

	BOOST_REQUIRE_EQUAL(cm->write_queue_[0], "Test message please ignore\n");
	BOOST_REQUIRE_EQUAL(cm2->write_queue_[0], "Test message please ignore\n");
}

BOOST_AUTO_TEST_CASE(targeted_does_not_broadcast)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	auto cm2 = std::make_shared<connection_mock>();

	connection_pool_type::uid_type cm_uid("1");
	connection_pool_type::uid_type cm_uid2("2");

	connection_pool_type sut(dm, {{cm_uid, cm}, {cm_uid2, cm2}});

	sut.send_targeted(cm_uid, "Hello from the system under test\n");

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 1);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 0);

	BOOST_REQUIRE_EQUAL(cm->write_queue_[0], "Hello from the system under test\n");
}

BOOST_AUTO_TEST_CASE(targeted_on_non_existant_throws)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	auto cm2 = std::make_shared<connection_mock>();

	connection_pool_type::uid_type cm_uid("1");
	connection_pool_type::uid_type cm_uid2("2");
	connection_pool_type::uid_type non_existant("3");

	connection_pool_type sut(dm, {{cm_uid, cm}, {cm_uid2, cm2}});

	BOOST_REQUIRE_THROW(sut.send_targeted(non_existant, "This should throw!\n"),
		connection_pool_type::endpoint_missing);

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 0);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 0);
}

BOOST_AUTO_TEST_CASE(can_add_connections)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	auto cm2 = std::make_shared<connection_mock>();

	connection_pool_type::uid_type cm_uid("1");
	connection_pool_type::uid_type cm_uid2("2");

	connection_pool_type sut(dm, {{cm_uid, cm}});

	BOOST_REQUIRE_THROW(sut.send_targeted(cm_uid2, "This should throw!\n"),
			connection_pool_type::endpoint_missing);

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 0);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 0);

	sut.add_connection(cm_uid2, cm2);

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 0);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 0);

	sut.send_targeted(cm_uid2, "This should pass :)\n");

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 0);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 1);

	BOOST_REQUIRE_EQUAL(cm2->write_queue_[0], "This should pass :)\n");
}

BOOST_AUTO_TEST_CASE(closed_connections_drop)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	auto cm2 = std::make_shared<connection_mock>();

	connection_pool_type::uid_type cm_uid("1");
	connection_pool_type::uid_type cm_uid2("2");

	connection_pool_type sut(dm, {{cm_uid, cm}, {cm_uid2, cm2}});

	sut.send_targeted(cm_uid2, "This should pass :)\n");

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 0);
	BOOST_REQUIRE_EQUAL(cm2->write_queue_.size(), 1);

	BOOST_REQUIRE_EQUAL(cm2->write_queue_[0], "This should pass :)\n");

	cm2->close();
	std::weak_ptr<connection_mock> weak_cm2(cm2);
	cm2.reset();

	BOOST_REQUIRE(weak_cm2.expired());

	BOOST_REQUIRE_THROW(sut.send_targeted(cm_uid2, "This should throw!\n"),
			connection_pool_type::endpoint_missing);

	sut.broadcast("This should not throw\n");

	BOOST_REQUIRE_EQUAL(cm->write_queue_.size(), 1);
	BOOST_REQUIRE_EQUAL(cm->write_queue_[0], "This should not throw\n");
}

BOOST_AUTO_TEST_CASE(connection_pool_last_out_cleans_connections)
{
	dispatch_mock dm;
	auto cm = std::make_shared<connection_mock>();
	auto cm2 = std::make_shared<connection_mock>();

	connection_pool_type::uid_type cm_uid("1");
	connection_pool_type::uid_type cm_uid2("2");

	std::weak_ptr<connection_mock> weak_cm(cm);
	std::weak_ptr<connection_mock> weak_cm2(cm2);
	{
		connection_pool_type sut(dm, {{cm_uid, cm}, {cm_uid2, cm2}});

		//remove our strong references while sut is still in scope
		cm.reset();
		cm2.reset();

		BOOST_REQUIRE(!weak_cm.expired());
		BOOST_REQUIRE(!weak_cm2.expired());
	}

	BOOST_REQUIRE(weak_cm.expired());
	BOOST_REQUIRE(weak_cm2.expired());
}
