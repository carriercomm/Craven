#define BOOST_TEST_MODULE "Comms test"
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <functional>
#include <iterator>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

namespace fs = boost::filesystem;

#include "../configure.hpp"
#include "../comms.hpp"

//! Timeout in seconds for the socket-based tests.
const unsigned timeout = 3;

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
		arguments_ = {"dfsctl", "-s", temp_unix.string()};
	else
		arguments_ = {"dfsctl"};

	arguments_.reserve(arguments_.size() + arguments.size());
	std::copy(arguments.begin(), arguments.end(), std::back_insert_iterator(arguments_));
}

SetupConfig::~SetupConfig()
{
	fs::remove(temp_rc_);

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

void invert_bool(const boost::system::error_code&, bool* var)
{
	*var = !*var;
}

BOOST_AUTO_TEST_CASE(arguments_passthrough)
{
	//Set up the configuration
	SetupConfig sc({}, true);
	auto config = sc.build_config();
	std::ostringstream os;

	CommsManager sut(config, os);

	//set up Asio
	boost::asio::io_service io_service;

	//set up the unix socket
	boost::asio::local::stream_protocol::endpoint ep(sc.temp_unix().string());
	boost::asio::local::stream_protocol::acceptor acceptor(io_service,
			ep);
	boost::asio::local::stream_protocol::socket socket(io_service);

	acceptor.async_accept

	//set up the timeout
	boost::asio::deadline_timer t(io_service, boost::posix_time::seconds(timeout));

	bool timeout = false;

	t.async_wait(boost::bind(invert_bool, boost::asio::placeholders::error, &timeout));

	io_service.run();

	BOOST_REQUIRE(!timeout);

}

BOOST_AUTO_TEST_CASE(output_on_stream)
{
	BOOST_REQUIRE(true);
}
