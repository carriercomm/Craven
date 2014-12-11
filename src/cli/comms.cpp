#include <iostream>
#include <sstream>
#include <ostream>
#include <string>
#include <array>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

namespace unixsock = boost::asio::local;

#include "configure.hpp"
#include "comms.hpp"

CommsManager::CommsManager(const CtlConfigure& config, std::ostream& output,
		std::shared_ptr<boost::asio::io_service> io_service)
	:output_stream_(output),
	io_service_(io_service),
	socket_(*io_service_)
{
	unixsock::stream_protocol::endpoint ep(config.socket().string());
	socket_.connect(ep);

	std::vector<std::string> options = config.daemon_arguments();
	std::ostringstream msg;
	msg << "[";

	//Construct the message
	for(auto it = options.begin(); it != options.end(); ++it)
	{
		//The first argument doesn't need to finish the last.
		if(it != options.begin())
			msg << "\", ";
		msg << "\"" << *it;
	}
	msg << "\"]\n";
	message_ = msg.str();

	//Set up the write
	boost::asio::async_write(socket_, boost::asio::buffer(message_),
			//Write handler -- just throws an error
			[this](boost::system::error_code error, std::size_t)
			{
				if(error && error != boost::asio::error::eof)
				{
					std::cerr << "Throwing from write handler\n";
					throw boost::system::system_error(error);
				}
			});

	//Set up the read
	boost::asio::async_read(socket_, buf_,
			boost::bind(&CommsManager::handle_read, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));

	try
	{
		io_service_->run();
	}
	catch(const std::runtime_error& ex)
	{
		std::cerr << "Fatal error: " << ex.what() << "\n";
	}
}

CommsManager::CommsManager(const CtlConfigure& config, std::ostream& output)
	:CommsManager(config, output, std::make_shared<boost::asio::io_service>())
{
}

CommsManager::~CommsManager()
{
}

void CommsManager::handle_read(const boost::system::error_code& error, std::size_t bytes_tx)
{
	if(error)
	{
		if(error == boost::asio::error::eof)
			//Clean exit
			io_service_->stop();
		else
		{
			std::cerr << "Throwing from read handler\n";
			throw boost::system::system_error(error);
		}
	}

	buf_.commit(bytes_tx);
	const char* msg = boost::asio::buffer_cast<const char*>(buf_.data());

	output_stream_ << msg;


	//Set up the read
	boost::asio::async_read(socket_, buf_,
			boost::bind(&CommsManager::handle_read, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
}
