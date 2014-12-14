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
	:io_service_(io_service),
	output_stream_(output)
{
	unixsock::stream_protocol::endpoint ep(config.socket().string());

	connection_type::socket_type socket(*io_service_);
	socket.connect(ep);

	connection_ = connection_type::create(socket);

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

	connection_->queue_write(msg.str());

	connection_->connect_read(
			[this](const std::string& line)
			{
				output_stream_ << line << std::endl;
			});

	connection_->connect_close(
			[this]()
			{
				io_service_->stop();
			});

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
