#include <string>

#include <boost/asio.hpp>

#include "connection_pool.hpp"
#include "comms_man.hpp"


comms_man::comms_man(const std::string& id, boost::asio::io_service& io,
			const boost::asio::ip::tcp::endpoint& endpoint,
			const node_map_type& nodes, TCPConnectionPool& pool)
	:id_(id),
	io_(io),
	acc_(io, endpoint),
	res_(io),
	nodes_(nodes),
	pool_(pool)
{
	//Start the accept
	start_accept();

	//Start the connect
	for(auto node : nodes_)
		start_connect(node);
}

void comms_man::start_accept()
{
	auto sock = std::make_shared<boost::asio::ip::tcp::socket>(io_);

	acc_.async_accept(*sock, [this, sock](const boost::system::error_code& ec)
			{
				if(!ec)
				{
					//make a signals2 connection; this will be used to disconnect
					//the read handkler
					auto read_connection =
						std::make_shared<boost::signals2::connection>();

					//wrap in our connection type
					auto conn = TCPConnectionPool::connection_type::create(sock);

					*read_connection =
						conn->connect_read([this, conn, read_connection]
								(const std::string& line)
								{
									handle_connection(conn, line, false);
									//clean up the handler
									read_connection->disconnect();
								});
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Error on accept: "
						<< ec.message();

				start_accept();
			});
}

void comms_man::start_connect(
		const std::pair<std::string, std::tuple<std::string, std::string>>& info)
{
		start_connect(std::get<0>(info), std::get<0>(std::get<1>(info)),
				std::get<1>(std::get<1>(info)));
}

void comms_man::start_connect(const std::string& endpoint)
{
	auto it = nodes_.find(endpoint);
	if(it != nodes_.end())
		start_connect(*it);
	else
		BOOST_LOG_TRIVIAL(warning) << "Connection requested for unknown node: "
			<< endpoint;
}

void comms_man::start_connect(const std::string& name, const std::string& host,
		const std::string& service)
{
	typedef boost::asio::ip::tcp::resolver resolver;
	resolver::query query(host, service);

	res_.async_resolve(query, [this, name](const boost::system::error_code& ec,
				resolver::iterator iter)
			{
				if(!ec)
				{
					auto sock = std::make_shared<boost::asio::ip::tcp::socket>(io_);

					boost::asio::async_connect(*sock, iter,
							std::bind(&comms_man::setup_connection, this,
								name, sock, std::placeholders::_1,
								std::placeholders::_2));
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Failed resolution of endpoint "
						<< name << ": " << ec.message();
			});
}

void comms_man::handle_connection(
		std::shared_ptr<TCPConnectionPool::connection_type> conn,
			const std::string& endpoint, bool ours)
{
	//check this is a valid connection
	if(nodes_.count(endpoint))
	{
		//determine if we already have a connection for this endpoint
		if(pool_.exists(endpoint))
		{
			if((endpoint > id_ && !ours)
					|| (endpoint < id_ && ours))
			{
				pool_.delete_connection(endpoint);
				pool_.add_connection(endpoint, conn);
				install_handlers(conn, endpoint);
			}

		}
		else
		{
			pool_.add_connection(endpoint, conn);
			install_handlers(conn, endpoint);
		}
	}
	else
		BOOST_LOG_TRIVIAL(warning) << "Unknown connection from: " << endpoint;
}

void comms_man::setup_connection(const std::string& name,
			std::shared_ptr<boost::asio::ip::tcp::socket> sock,
			const boost::system::error_code& ec,
			boost::asio::ip::tcp::resolver::iterator /*iter*/)
{
	if(!ec)
	{
		auto conn = TCPConnectionPool::connection_type::create(sock);
		conn->queue_write(id_ + '\n');
		handle_connection(conn, name, true);
	}
	else
		BOOST_LOG_TRIVIAL(warning)
			<< "Failed to connect to endpoint "
			<< name << ": " << ec.message();
}

void comms_man::install_handlers(TCPConnectionPool::connection_type::pointer conn,
		const std::string& endpoint)
{
	conn->connect_close(
			[this, endpoint](const std::string& uuid)
			{
				// If there is no connection for this endpoint or this connection
				// was the connection for this endpoint, replace it
				if(!pool_.exists(endpoint) ||
						pool_.responsible(uuid, endpoint))
				{
					//a lambda because bind can't cope with overloads
					io_.post([this, endpoint]
							{
								start_connect(endpoint);
							});
				}
			});
}
