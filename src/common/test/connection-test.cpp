#define BOOST_TEST_MODULE "Utility"
#include <boost/test/unit_test.hpp>

#include <string>
#include <deque>
#include <functional>

#include <boost/asio.hpp>

#include "../connection.hpp"

typedef boost::asio::local::stream_protocol::socket socket_type;
typedef util::connection<
	boost::asio::local::stream_protocol::socket, util::connection_single_handler_tag> single_connection;

typedef util::connection<
	boost::asio::local::stream_protocol::socket, util::connection_multiple_handler_tag> multiple_connection;

class check_array
{
public:
	check_array(boost::asio::io_service& io, const std::deque<std::string>& expected);

	void operator()(const std::string& line);

	bool success() const;

	std::size_t remaining() const;

protected:
	boost::asio::io_service& io_;
	std::deque<std::string> expected_;
	bool success_;
};

check_array::check_array(boost::asio::io_service& io, const std::deque<std::string>& expected)
	:io_(io),
	expected_(expected),
	success_(true)
{
}

void check_array::operator()(const std::string& line)
{
	if(expected_.empty())
	{
		success_ = false;
		io_.stop();
	}
	else
	{
		success_ = success_ && (line == expected_.front());
		expected_.pop_front();
		if(!success_ || expected_.empty())
			io_.stop();
	}
}

bool check_array::success() const
{
	return success_ && expected_.empty();
}

std::size_t check_array::remaining() const
{
	return expected_.size();
}

class timer
{
public:
	static const unsigned timeout = 30;

	timer(boost::asio::io_service& io);

	bool timed_out() const;

protected:
	bool timed_out_;
	boost::asio::deadline_timer t_;
};

timer::timer(boost::asio::io_service& io)
	:timed_out_(false),
	t_(io, boost::posix_time::seconds(timeout))
{
	t_.async_wait(
			[this, &io](const boost::system::error_code& ec)
			{
				if(ec)
					throw boost::system::system_error(ec);

				timed_out_ = true;
				io.stop();
			});
}

bool timer::timed_out() const
{
	return timed_out_;
}



BOOST_AUTO_TEST_CASE(single_handler_trait)
{
	boost::asio::io_service io;
	socket_type sock1(io);
	socket_type sock2(io);

	boost::asio::local::connect_pair(sock1, sock2);

	auto conn1 = single_connection::create(sock1);
	auto conn2 = single_connection::create(sock2);

	std::string msg1 = "Fnord\nfoo\n";
	std::string msg2 = "bar\n";

	check_array ca(io, std::deque<std::string>{"Fnord", "foo", "bar"});

	conn2->connect_read([&ca](const std::string& msg){ca(msg);});

	conn1->queue_write(msg1);
	conn1->queue_write(msg2);

	timer t(io);

	io.run();

	BOOST_REQUIRE_MESSAGE(!t.timed_out(), "Test took too long.");
	BOOST_REQUIRE_MESSAGE(ca.success(), "Remaining: " << ca.remaining());
}

BOOST_AUTO_TEST_CASE(multiple_handler_trait)
{
	boost::asio::io_service io;
	socket_type sock1(io);
	socket_type sock2(io);

	boost::asio::local::connect_pair(sock1, sock2);

	auto conn1 = multiple_connection::create(sock1);
	auto conn2 = multiple_connection::create(sock2);

	std::string msg1 = "Fnord\nfoo\n";
	std::string msg2 = "bar\n";

	check_array ca(io, std::deque<std::string>{"Fnord", "foo", "bar"});

	conn2->connect_read([&ca](const std::string& msg){ca(msg);});

	conn1->queue_write(msg1);
	conn1->queue_write(msg2);

	timer t(io);

	io.run();

	BOOST_REQUIRE_MESSAGE(!t.timed_out(), "Test took too long.");
	BOOST_REQUIRE_MESSAGE(ca.success(), "Remaining: " << ca.remaining());
}
