#define BOOST_TEST_MODULE "Comms test"
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iterator>
#include <array>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

namespace fs = boost::filesystem;

#include "../configure.hpp"
#include "../comms.hpp"

//! Timeout in seconds for the socket-based tests.
const unsigned timeout = 300;

//! Fixture for testing the cli-side of the comms module.
class SetupConfig
{
public:
	SetupConfig(const std::vector<std::string>& arguments = {},
			bool insert_socket=false);
	~SetupConfig();

	CtlConfigure build_config() const;

	fs::path temp_rc() const;
	fs::path temp_unix() const;

protected:
	fs::path temp_rc_;
	fs::path temp_unix_;

	std::vector<std::string> arguments_;

	bool insert_socket_;
	bool timeout_marker_;
};

SetupConfig::SetupConfig(const std::vector<std::string>& arguments,
		bool insert_socket)
	:temp_rc_(fs::temp_directory_path() / fs::unique_path()),
	temp_unix_(fs::temp_directory_path() / fs::unique_path()),
	insert_socket_(insert_socket)
{
	if(insert_socket)
		arguments_ = {"dfsctl", "-s", temp_unix_.string()};
	else
		arguments_ = {"dfsctl"};

	arguments_.reserve(arguments_.size() + arguments.size());
	std::copy(arguments.begin(), arguments.end(), std::back_inserter(arguments_));
}

SetupConfig::~SetupConfig()
{
	fs::remove(temp_rc_);
	fs::remove(temp_unix_);
}

CtlConfigure SetupConfig::build_config() const
{
	return CtlConfigure(arguments_);
}

fs::path SetupConfig::temp_rc() const
{
	return temp_rc_;
}

fs::path SetupConfig::temp_unix() const
{
	return temp_unix_;
}


class UnixServer : std::enable_shared_from_this<UnixServer>
{
public:
	typedef std::shared_ptr<boost::asio::io_service> io_ptr;
	typedef boost::system::error_code error_code;

	UnixServer(io_ptr io_service, const fs::path& socket)
		:io_(io_service),
		acc_(*io_, boost::asio::local::stream_protocol::endpoint(socket.string())),
		sock_(*io_)

	{
		acc_.async_accept(sock_, boost::bind(&UnixServer::handle_accept, this,
					boost::asio::placeholders::error));
	}

	std::string data() const
	{
		return data_;
	}

	void handle_read(const error_code& error, std::size_t bytes_tx)
	{
		if(error && error != boost::asio::error::eof)
			throw boost::system::system_error(error);

		buf_.commit(bytes_tx);
		std::istream buffer_stream(&buf_);
		std::getline(buffer_stream, data_);

		auto t = std::make_shared<boost::asio::deadline_timer>(*io_, boost::posix_time::seconds(1));

		t->async_wait([this](const boost::system::error_code& error)
					{
						sock_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_both);
						sock_.close();

						acc_.close();
					});
	}

	void handle_accept(const boost::system::error_code& error)
	{
		if(error)
			throw boost::system::system_error(error);

		boost::asio::async_read_until(sock_, buf_, '\n',
				boost::bind(&UnixServer::handle_read, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
	}

private:
	io_ptr io_;
	boost::asio::local::stream_protocol::acceptor acc_;
	boost::asio::local::stream_protocol::socket sock_;

	boost::asio::streambuf buf_;
	std::string data_;
};


void invert_bool(const boost::system::error_code& error, std::shared_ptr<boost::asio::io_service> io, bool* var)
{
	if(error)
		throw boost::system::system_error(error);

	*var = !*var;

	// Stop the wait.
	io->stop();
}

BOOST_AUTO_TEST_CASE(arguments_passthrough)
{
	//Set up the configuration
	SetupConfig sc({"foo", "bar baz", "t", "thud"}, true);
	auto config = sc.build_config();
	std::ostringstream os;

	//set up Asio
	auto io_service = std::make_shared<boost::asio::io_service>();

	//set up the unix socket
	UnixServer ux(io_service, sc.temp_unix());

	//set up the timeout
	boost::asio::deadline_timer t(*io_service, boost::posix_time::seconds(timeout));

	bool timeout = false;

	t.async_wait(boost::bind(invert_bool, boost::asio::placeholders::error, io_service, &timeout));

	//Run the system under test.
	CommsManager sut(config, os, io_service);

	BOOST_REQUIRE(!timeout);

	// TODO #12: work out why dfsctl is coming through
	// probably related to ticket 11
	BOOST_REQUIRE_EQUAL(ux.data(), R"(["dfsctl", "foo", "bar baz", "t", "thud"])");
}

class UnixStream : std::enable_shared_from_this<UnixStream>
{
public:
	typedef std::shared_ptr<boost::asio::io_service> io_ptr;
	typedef boost::system::error_code error_code;

	UnixStream(io_ptr io_service, const fs::path& socket, const std::string& data)
		:io_(io_service),
		acc_(*io_, boost::asio::local::stream_protocol::endpoint(socket.string())),
		sock_(*io_),
		data_(data)

	{
		acc_.async_accept(sock_, boost::bind(&UnixStream::handle_accept, this,
					boost::asio::placeholders::error));
	}

	void handle_accept(const boost::system::error_code& error)
	{
		if(error)
			throw boost::system::system_error(error);

		//Setup a bogus read handler
		boost::asio::async_read_until(sock_, buf_, '\n',
				[this](const error_code& error, std::size_t bytes_tx)
				{
					if(error)
						throw boost::system::system_error(error);
					buf_.commit(bytes_tx);
					buf_.consume(bytes_tx);
				});

		boost::system::error_code write_error;
		boost::asio::write(sock_, boost::asio::buffer(data_), write_error);

		if(write_error)
			throw boost::system::system_error(write_error);

		auto t = std::make_shared<boost::asio::deadline_timer>(*io_, boost::posix_time::seconds(1));

		t->async_wait([this](const boost::system::error_code& error)
					{
						sock_.shutdown(boost::asio::local::stream_protocol::socket::shutdown_both);
						sock_.close();

						acc_.close();
					});
	}

private:
	io_ptr io_;
	boost::asio::local::stream_protocol::acceptor acc_;
	boost::asio::local::stream_protocol::socket sock_;

	boost::asio::streambuf buf_;
	std::string data_;
};

BOOST_AUTO_TEST_CASE(output_on_stream)
{
	//Set up the configuration
	SetupConfig sc({"foo", "bar baz", "t", "thud"}, true);
	auto config = sc.build_config();
	std::ostringstream os;

	//set up Asio
	auto io_service = std::make_shared<boost::asio::io_service>();

	std::string to_stream("Foo bar baz fnord\nbaz bar foo");

	//set up the unix socket
	UnixStream ux(io_service, sc.temp_unix().string(), to_stream);

	//set up the timeout
	boost::asio::deadline_timer t(*io_service, boost::posix_time::seconds(timeout));

	bool timeout = false;

	t.async_wait(boost::bind(invert_bool, boost::asio::placeholders::error, io_service, &timeout));

	CommsManager sut(config, os, io_service);

	BOOST_REQUIRE(!timeout);

	std::string result(os.str());

	BOOST_REQUIRE_EQUAL(result, to_stream);
}
