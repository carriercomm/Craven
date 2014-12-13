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
		if(!success_)
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



BOOST_AUTO_TEST_CASE(single_handler_trait)
{
	boost::asio::io_service io;
	socket_type sock1(io);
	socket_type sock2(io);

	auto conn1 = single_connection::create(sock1);
	auto conn2 = single_connection::create(sock2);

	boost::asio::local::connect_pair(sock1, sock2);

	std::string msg1 = "Fnord\nfoo\n";
	std::string msg2 = "bar\n";

	check_array ca(io, std::deque<std::string>{"Fnord", "foo", "bar"});

	conn2->connect_read(ca);

	conn1->queue_write(msg1);
	conn1->queue_write(msg2);

	BOOST_REQUIRE(ca.success());
}

BOOST_AUTO_TEST_CASE(multiple_handler_trait)
{
	boost::asio::io_service io;
	socket_type sock1(io);
	socket_type sock2(io);

	auto conn1 = multiple_connection::create(sock1);
	auto conn2 = multiple_connection::create(sock2);

	boost::asio::local::connect_pair(sock1, sock2);

	std::string msg1 = "Fnord\nfoo\n";
	std::string msg2 = "bar\n";

	check_array ca(io, std::deque<std::string>{"Fnord", "foo", "bar"});

	conn2->connect_read(ca);

	conn1->queue_write(msg1);
	conn1->queue_write(msg2);

	BOOST_REQUIRE(ca.success());
}
