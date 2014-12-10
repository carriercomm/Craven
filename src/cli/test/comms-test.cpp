#define BOOST_TEST_MODULE "Comms test"
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <functional>
#include <iterator>
#include <array>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

namespace fs = boost::filesystem;

#include "../configure.hpp"
#include "../comms.hpp"

//! Timeout in seconds for the socket-based tests.
const unsigned timeout = 30;

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

void handle_accept(const boost::system::error_code& error, std::string*
		received, boost::asio::local::stream_protocol::socket* socket)
{
	if(error)
		throw boost::system::system_error(error);

	//read in the whole message
	std::ostringstream os;

	while(true)
	{
		std::array<char, 512> buf;
		boost::system::error_code error;

		size_t len = socket->read_some(boost::asio::buffer(buf), error);

		if(error == boost::asio::error::eof)
			break;
		else if(error)
			throw boost::system::system_error(error);

		os.write(buf.data(), len);
	}

	*received = os.str();
}

void invert_bool(const boost::system::error_code& error, boost::asio::io_service* io, bool* var)
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

	std::cout << "Connect to: " <<  sc.temp_unix().string() << "\n";

	CommsManager sut(config, os);

	//set up Asio
	boost::asio::io_service io_service;

	//set up the unix socket
	boost::asio::local::stream_protocol::endpoint ep(sc.temp_unix().string());
	boost::asio::local::stream_protocol::acceptor acceptor(io_service,
			ep);
	boost::asio::local::stream_protocol::socket socket(io_service);

	std::string received;

	acceptor.async_accept(socket, boost::bind(handle_accept,
				boost::asio::placeholders::error, &received, &socket));

	//set up the timeout
	boost::asio::deadline_timer t(io_service, boost::posix_time::seconds(timeout));

	bool timeout = false;

	t.async_wait(boost::bind(invert_bool, boost::asio::placeholders::error, &io_service, &timeout));

	io_service.run();

	BOOST_REQUIRE(!timeout);

	BOOST_REQUIRE_EQUAL(received, R"(["foo", "bar baz", "t", "thud"])");
}

void stream_to_accept(const boost::system::error_code& error, const
		std::string& stream, boost::asio::local::stream_protocol::socket* socket)
{
	if(error)
		throw boost::system::system_error(error);

	boost::system::error_code write_error;
	boost::asio::write(*socket, boost::asio::buffer(stream), write_error);

	if(write_error)
		throw boost::system::system_error(write_error);
}

BOOST_AUTO_TEST_CASE(output_on_stream)
{
	//Set up the configuration
	SetupConfig sc({"foo", "bar baz", "t", "thud"}, true);
	auto config = sc.build_config();
	std::ostringstream os;

	std::cout << "Connect to: " <<  sc.temp_unix().string() << "\n";

	CommsManager sut(config, os);

	//set up Asio
	boost::asio::io_service io_service;

	//set up the unix socket
	boost::asio::local::stream_protocol::endpoint ep(sc.temp_unix().string());
	boost::asio::local::stream_protocol::acceptor acceptor(io_service,
			ep);
	boost::asio::local::stream_protocol::socket socket(io_service);

	std::string to_stream("Foo bar baz fnord\nbaz bar foo");

	acceptor.async_accept(socket, boost::bind(stream_to_accept,
				boost::asio::placeholders::error, to_stream, &socket));

	//set up the timeout
	boost::asio::deadline_timer t(io_service, boost::posix_time::seconds(timeout));

	bool timeout = false;

	t.async_wait(boost::bind(invert_bool, boost::asio::placeholders::error, &io_service, &timeout));

	io_service.run();

	BOOST_REQUIRE(!timeout);

	BOOST_REQUIRE_EQUAL(os.str(), to_stream);
}
