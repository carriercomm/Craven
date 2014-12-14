#pragma once

#include <memory>

#include <boost/asio.hpp>

#include "../common/connection.hpp"


//! Class to handle daemon--ctl comms from the ctl side.
class CommsManager
{
public:
	typedef util::connection<boost::asio::local::stream_protocol::socket,
			util::connection_single_handler_tag> connection_type;


	//! \param config The configuration for the ctl.
	//! \param output The stream to output daemon responses to.
	CommsManager(const CtlConfigure& config, std::ostream& output);

	CommsManager(const CtlConfigure& config, std::ostream& output,
			std::shared_ptr<boost::asio::io_service> io_service);


private:
	std::shared_ptr<boost::asio::io_service> io_service_;

	connection_type::pointer connection_;

	//! The stream used to output
	std::ostream& output_stream_;
};
