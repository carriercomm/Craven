#pragma once

#include <map>
#include <functional>

#include "../common/connection.hpp"

//! This class manages a group of connections with IDs, passing messages to a dispatch class.
/*! T is the connection manager type, R is the dispatch manager type. This class
 *  is templated to support its unit tests.
 */
template <typename T>
class ConnectionPool
{
public:
	typedef T connection_type;
	typedef ConnectionPool<T> type;
	typedef std::string uid_type;
	typedef std::map<uid_type, typename connection_type::pointer> connection_map_type;

	struct endpoint_missing : std::runtime_error
	{
		endpoint_missing(const std::string& endpoint)
			:std::runtime_error("Endpoint " + endpoint + " does not exist")
		{}
	};

	//! Callback handler provided to dispatch when an RPC comes in.
	/*!
	 *  Instances of this class act as a callback to write a response when an
	 *  RPC comes in. It's provided to dispatch with the message, which then
	 *  handles marshalling.
	 *
	 *  The callback must not outlive its parent or a segfault will occur on
	 *  several operations.
	 */
	class Callback
	{
	public:
		struct invalid_callback : std::runtime_error
		{
			invalid_callback()
				:std::runtime_error("Callback not initialised")
			{}
		};

		//! Construct an invalid handler.
		Callback()
			:parent_(nullptr)
		{}
		//! Construct the handler
		/*!
		 *  \param parent -- a reference to the owning ConnectionPool that this
		 *  callback forwards to.
		 *
		 *  \param endpoint The endpoint the message came from.
		 */
		Callback(type* parent, const uid_type& endpoint)
			:parent_(parent),
			endpoint_(endpoint)
		{
		}

		//! Checks that this class is a valid callback.
		operator bool() const
		{
			return parent_ != nullptr && parent_->exists(endpoint_);
		}

		//! Returns the endpoint the acompanying message came from.
		uid_type endpoint() const
		{
			this->throw_if_invalid();
			return endpoint_;
		}

		//! The callback; sends a response to the RPC.
		/*!
		 *  This function handles sending a response to the node from whence
		 *  this RPC originated.
		 *
		 *  Can throw a endpoint_missing exception if the endpoint's disconnected
		 *  since.
		 *
		 *  \param msg The message to respond with.
		 */
		void operator()(const std::string& msg)
		{
			this->throw_if_invalid();
			parent_->send_targeted(endpoint_, msg);
		}

	protected:
		inline void throw_if_invalid() const
		{
			if(parent_ == nullptr)
				throw invalid_callback();
		}


		//! A reference to the parent class of this callback
		type* parent_;

		//! The originating node
		uid_type endpoint_;
	};

	typedef std::function<void(const std::string&, const Callback&)> dispatch_type;

	//! Construct the ConnectionPool.
	/*!
	 *  \param dispatch A function object handling dispatch
	 *  \param connections The map of connections to initialise with.
	 */
	ConnectionPool(const dispatch_type& dispatch, const std::map<uid_type, typename connection_type::pointer>& connections={})
		:dispatch_(dispatch),
		connections_(connections)
	{
		for(auto connection : connections_)
			install_handlers(connection.first, connection.second);
	}

	//! Send a message to all known nodes.
	void broadcast(const std::string& msg)
	{
		for(auto connection : connections_)
			connection.second->queue_write(msg);
	}

	//! Send a message to a specific node
	void send_targeted(const uid_type& endpoint, const std::string& msg)
	{
		if(!connections_.count(endpoint))
			throw endpoint_missing(endpoint);

		connections_[endpoint]->queue_write(msg);
	}

	void add_connection(const uid_type& endpoint, typename connection_type::pointer connection)
	{
		if(!connections_.count(endpoint) || !connections_[endpoint]->is_open())
			install_handlers(endpoint, connection);
	}

	bool exists(const uid_type& endpoint) const
	{
		return connections_.count(endpoint);
	}

protected:
	//! A reference to the instance of dispatch_type providing RPC dispatch serivces
	dispatch_type dispatch_;

	//! The map of connections.
	connection_map_type connections_;

	void install_handlers(const uid_type& endpoint, typename connection_type::pointer connection)
	{
		connection->connect_read(
				[this, endpoint](const std::string& msg)
				{
					Callback cb(this, endpoint);

					dispatch_(msg, cb);
				});

		connection->connect_close(
				[this, endpoint]()
				{
					if(connections_.count(endpoint))
						connections_.erase(endpoint);
				});

	}
};

typedef ConnectionPool<util::connection<boost::asio::ip::tcp::socket, util::connection_multiple_handler_tag>> TCPConnectionPool;
