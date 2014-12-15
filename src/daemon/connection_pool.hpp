#pragma once

#include <unordered_map>

#include <boost/uuid/uuid.hpp>

//! This class manages a group of connections with IDs, passing messages to a dispatch class.
/*! T is the connection manager type, R is the dispatch manager type. This class
 *  is templated to support its unit tests.
 */
template <class T, R>
class ConnectionPool
{
public:
	typedef T connection_type;
	typedef R dispatch_type;
	typedef boost::uuids::uuid uid_type;

	class remote_missing
	{

	};

	//! Callback handler provided to dispatch when an RPC comes in.
	/*!
	 *  Instances of this class act as a callback to write a response when an
	 *  RPC comes in. It's provided to dispatch with the message, which then
	 *  handles marshalling.
	 */
	class callback
	{
	public:
		//! Construct the handler
		/*!
		 *  \param parent -- a reference to the owning ConnectionPool that this
		 *  callback forwards to.
		 *
		 *  \param endpoint The endpoint the message came from.
		 */
		callback(ConnectionPool<& parent, const uid_type& endpoint)
			:parent_(parent),
			endpoint(endpoint_)
		{
		}

		//! Returns the endpoint the acompanying message came from.
		uid_type endpoint() const
		{
			return endpoint_;
		}

		//! The callback; sends a response to the RPC.
		/*!
		 *  This function handles sending a response to the node from whence
		 *  this RPC originated.
		 *
		 *  Can throw a remote_missing exception if the remote's disconnected
		 *  since.
		 *
		 *  \param msg The message to respond with.
		 */
		void operator()(const std:string& msg)
		{
			throw std::runtime_error("Not yet implemented");
		}

	protected:
		//! A reference to the parent class of this callback
		ConnectionPool& parent_;

		//! The originating node
		uid_type endpoint_;
	};

	//! Construct the ConnectionPool.
	/*!
	 *  \param dispatch The instance of dispatch_type handling RPC dispatch
	 *  \param connections The map of connections to initialise with.
	 */
	ConnectionPool(dispatch_type& dispatch, const std::unordered_map<uid_type, connection_type::pointer>& connections={})
		:dispatch_(dispatch),
		connections_(connections)
	{

	}

	//! Send a message to all known nodes.
	void broadcast(const std::string& msg)
	{
		throw std::runtime_error("");
	}

	//! Send a message to a specific node
	void send_targeted(const uid_type& endpoint, const std::string& msg)
	{
		throw std::runtime_error("Not yet implemented");
	}

	void add_connection(connection_type::pointer connection, const uid_type& uid)
	{
		throw std::runtime_error("Not yet implemented");
	}


protected:
	//! A reference to the instance of dispatch_type providing RPC dispatch serivces
	dispatch_type& dispatch_;

	//! The map of connections.
	std::unordered_map<uid_type, connection_type::pointer> connections_;

};
