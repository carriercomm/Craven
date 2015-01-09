#pragma once

#include <unordered_map>

//! Manages the connections to other nodes
class comms_man
{
public:
	//! Other node information type
	typedef std::unordered_map<std::string,
			std::tuple<std::string, std::string>> node_map_type;

	//! Construct the communications manager
	/*!
	 *  \param id The ID of this node
	 *  \param io The IO service for this thread
	 *  \param endpoint The TCP endpoint to bind to to listen for connections
	 *  \param nodes A map of connection IDs to their ip/port details
	 *	\param pool The connection pool to hand completed connections
	 *	to.
	 */
	comms_man(const std::string& id, boost::asio::io_service& io,
			const boost::asio::ip::tcp::endpoint& endpoint,
			const node_map_type& nodes, TCPConnectionPool& pool);

protected:
	void start_accept();
	void start_connect(
			const std::pair<std::string, std::tuple<std::string, std::string>>& info);
	void start_connect(const std::string& endpoint);
	void start_connect(const std::string& name, const std::string& host, const std::string& service);

	void handle_connection(TCPConnectionPool::connection_type::pointer conn,
			const std::string& endpoint, bool ours);


	void setup_connection(const std::string& name,
			std::shared_ptr<boost::asio::ip::tcp::socket> sock,
			const boost::system::error_code& ec,
			boost::asio::ip::tcp::resolver::iterator iter);

	void install_handlers(TCPConnectionPool::connection_type::pointer conn,
			const std::string& endpoint);

	std::string id_;
	boost::asio::io_service& io_;
	boost::asio::ip::tcp::acceptor acc_;
	boost::asio::ip::tcp::resolver res_;
	node_map_type nodes_;
	TCPConnectionPool& pool_;
};
