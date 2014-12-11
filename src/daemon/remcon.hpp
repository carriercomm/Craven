#pragma once

class RemoteControl
{
public:
	RemoteControl(boost::asio::io_service& io, const boost::filesystem::path& socket);

protected:
	boost::asio::io_service& io_;
};
