#pragma once

//! Class to handle daemon--ctl comms from the ctl side.
class CommsManager
{
public:
	//! \param config The configuration for the ctl.
	//! \param output The stream to output daemon responses to.
	CommsManager(const CtlConfigure& config, std::ostream& output);

	CommsManager(const CtlConfigure& config, std::ostream& output,
			std::shared_ptr<boost::asio::io_service> io_service);

	~CommsManager();

private:
	//! Handler for extra data
	void handle_read(const boost::system::error_code& error, std::size_t bytes_tx);

	//! The message to be sent
	std::string message_;

	//! The read buffer
	boost::asio::streambuf buf_;

	std::shared_ptr<boost::asio::io_service> io_service_;

	boost::asio::local::stream_protocol::socket socket_;

	//! The stream used to output
	std::ostream& output_stream_;
};
