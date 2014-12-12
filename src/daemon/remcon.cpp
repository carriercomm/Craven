#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>

namespace fs = boost::filesystem;

#include "remcon.hpp"

detail::connection::connection(boost::asio::local::stream_protocol::socket socket)
	:socket_(std::move(socket))
{
	throw std::runtime_error("Not yet implemented");
}

detail::connection::pointer detail::connection::create(boost::asio::io_service& io_service)
{
	throw std::runtime_error("Not yet implemented");
	return nullptr;
}

boost::asio::local::stream_protocol::socket& detail::connection::socket()
{
	return socket_;
}

const boost::asio::local::stream_protocol::socket& detail::connection::socket() const
{
	return socket_;
}

void detail::connection::queue_write(const std::string& msg)
{
	throw std::runtime_error("Not yet implemented");
}


RemoteControl::RemoteControl(boost::asio::io_service& io, const fs::path& socket)
	:io_(io)
{

}
