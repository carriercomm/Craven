#define BOOST_TEST_MODULE "RemoteControl test"
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <functional>
#include <deque>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>

namespace fs = boost::filesystem;

#include "../remcon.hpp"

//! Timeout in seconds for the socket-based tests.
const unsigned timeout = 300;

class UnixServer
{
public:
	UnixServer(const std::deque<std::string>& expected={});

	boost::asio::io_service& io();
	const boost::asio::io_service& io() const;

	detail::connection::pointer connection();

	bool success() const;

	void write_string(const std::string& msg);

protected:
	void start_read();

	boost::asio::io_service io_;
	boost::asio::local::stream_protocol::socket sock_;

	std::deque<std::string> expected_;
	bool success_;

	detail::connection::pointer connection_;
};

UnixServer::UnixServer(const std::deque<std::string>& expected)
	:io_(),
	sock_(io_),
	expected_(expected),
	success_(true),
	connection_(detail::connection::create(io_))
{
	boost::asio::local::connect_pair(sock_, connection_->socket());
	start_read();
}

boost::asio::io_service& UnixServer::io()
{
	return io_;
}

const boost::asio::io_service& UnixServer::io() const
{
	return io_;
}

detail::connection::pointer UnixServer::connection()
{
	return connection_;
}

void UnixServer::start_read()
{
	if(success_ && expected_.empty())
	{
		auto buf = std::make_shared<boost::asio::streambuf>();

		boost::asio::async_read(sock_, *buf,
				[this, buf](const boost::system::error_code& ec, std::size_t bytes_tx)
				{
					if(ec)
						throw boost::system::system_error(ec);
					buf->commit(bytes_tx);
					std::istream is(buf.get());

					//don't skip whitespace
					is.unsetf(std::ios::skipws);
					std::string s;
					is >> s;
					success_ = (s == expected_.front());
					expected_.pop_front();

					start_read();
				});
	}
	else if(success_)
		io_.stop();
}

bool UnixServer::success() const
{
	return success_;
}

void UnixServer::write_string(const std::string& msg)
{
	auto shared_string = std::make_shared<std::string>(msg);

	boost::asio::async_write(sock_, boost::asio::buffer(*shared_string),
			[this, shared_string](const boost::system::error_code& ec, std::size_t)
			{
				if(ec)
					throw boost::system::system_error(ec);
			});
}

BOOST_AUTO_TEST_CASE(write_to_connection)
{
	UnixServer ux({"foo bar baz fnord\nthud"});

	ux.connection()->queue_write("foo bar baz fnord\nthud");

	//set up the timeout
	boost::asio::deadline_timer t(ux.io(), boost::posix_time::seconds(timeout));

	bool timeout = false;

	t.async_wait([&timeout, &ux](const boost::system::error_code& ec)
			{
				if(ec)
					throw boost::system::system_error(ec);

				timeout = !timeout;

				ux.io().stop();
			});

	ux.io().run();

	BOOST_REQUIRE(!timeout);
	BOOST_REQUIRE(ux.success());
}

BOOST_AUTO_TEST_CASE(read_from_connection)
{
	UnixServer ux;

	bool success = false;
	ux.connection()->connect_read(
			[&ux, &success](const std::string& msg)
			{
				success = success && (msg == "foo bar baz fnord\n");
			});

	ux.write_string("foo bar baz fnord\nthud");

	//set up the timeout
	boost::asio::deadline_timer t(ux.io(), boost::posix_time::seconds(timeout));

	bool timeout = false;

	t.async_wait([&timeout, &ux](const boost::system::error_code& ec)
			{
				if(ec)
					throw boost::system::system_error(ec);

				timeout = !timeout;

				ux.io().stop();
			});

	ux.io().run();

	BOOST_REQUIRE(!timeout);
	BOOST_REQUIRE(success);

}
