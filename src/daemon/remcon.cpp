#include <iostream>
#include <tuple>


#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

#include "../common/connection.hpp"
#include "remcon.hpp"

CTLSession::CTLSession(connection_type::pointer connection)
	:connection_(connection)
{

}

void CTLSession::write(const std::string& msg)
{
	connection_->queue_write(msg);
}

RemoteControl::parse_error::parse_error(const std::string& what)
	:runtime_error(what)
{
}

RemoteControl::RemoteControl(boost::asio::io_service& io, const fs::path& socket)
	:io_(io),
	acceptor_(io_, boost::asio::local::stream_protocol::endpoint(socket.string()))
{
	BOOST_LOG_TRIVIAL(info) << "Control listening on " << socket.string();
	start_accept();
}

void RemoteControl::start_accept()
{
	auto sock = std::make_shared<boost::asio::local::stream_protocol::socket>(io_);
	acceptor_.async_accept(*sock,
			[this, sock](const boost::system::error_code& ec)
			{
				if(ec)
					throw boost::system::system_error(ec);

				CTLSession::connection_type::pointer connection = CTLSession::connection_type::create(sock);
				BOOST_LOG_TRIVIAL(info) << "Connection on control socket";


				connection->connect_read(
						[this, connection](const std::string& msg) mutable
						{
							BOOST_LOG_TRIVIAL(debug) << "Message on control socket: " << msg;

							if(connection)
							{
								try
								{
									std::tuple<std::string, std::vector<std::string>> message = parse_line(msg);

									if(registry_.count(std::get<0>(message)))
									{
										//Call the handlers
										CTLSession session(connection);
										registry_[std::get<0>(message)](std::get<1>(message), connection);
										connection.reset();
									}
									else
									{
										std::string error_msg = "Unknown verb: ";
										error_msg += std::get<0>(message) + ".\n";
										connection->queue_write(error_msg);
										connection.reset();
										BOOST_LOG_TRIVIAL(warning) << "Error on control socket: " << error_msg << " (ignored)";

									}
								}
								catch(const parse_error& ex)
								{
									connection->queue_write(ex.what());
									connection.reset();
									BOOST_LOG_TRIVIAL(warning) << "Error on control socket: " << ex.what() << "(ignored)";
								}
							}
							else
								BOOST_LOG_TRIVIAL(warning) << "Unexpected message on control socket";
						});
				start_accept();
			});
}

std::tuple<std::string, std::vector<std::string>> RemoteControl::parse_line(const std::string& msg)
{
	pt::ptree parsed;
	std::istringstream is(msg);

	pt::json_parser::read_json(is, parsed);

	std::string verb;
	std::vector<std::string> args;

	pt::ptree::iterator it = parsed.begin();

	if(it != parsed.end())
	{
		if(it->first != "")
			throw parse_error("Bad format -- needs to be an array of strings");

		verb = (it++)->second.data();
	}
	else
		throw parse_error("Too few tokens -- no verb supplied");

	for(; it != parsed.end(); ++it)
	{
		if(it->first != "")
			throw parse_error("Bad format -- needs to be an array of strings");

		args.push_back(it->second.data());
	}

	return std::make_tuple(verb, args);
}
