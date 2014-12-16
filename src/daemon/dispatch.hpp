#pragma once

#include <unordered_map>

#include "connection_pool.hpp"

//! This class handles the top-level dispatch for the RPC. The class is also
//! responsible for top-level marshalling.
class TopLevelDispatch
{
public:
	typedef TCPConnectionPool connection_pool_type;

	//! Exception thrown when connect_dispatcher is called with an ID that's
	//already registered.
	struct dispatcher_exists : std::runtime_error
	{
		dispatcher_exists(const std::string& id);
	};

	//! Wrapper around the connection pool's callback that handles json serialising.
	class Callback
	{
	public:
		Callback() = default;
		Callback(const std::string& module, const
				connection_pool_type::Callback& cb);

		operator bool() const;

		connection_pool_type::uid_type endpoint() const;

		void operator()(const Json::Value& msg);

	protected:
		std::string module_;
		connection_pool_type::Callback wrapped_;
	};


	//! Construct the dispatch
	TopLevelDispatch();

	//! Dispatch the RPC encoded in msg.
	/*!
	 *  This function deserialises an RPC and dispatches it to the
	 *  module-specific dispatch requested.
	 *
	 *  \param msg A string containing the JSON to decode -- the dispatch
	 *  direction is computed from this.
	 *
	 *  \param cb The callback object for replies.
	 */
	void operator()(const std::string& msg, const connection_pool_type::Callback& cb);

	//! Connect a module-level dispatcher.
	/*!
	 *  This function connects a module-level dispatcher's dispatch handler. It
	 *  will be called when its ID comes in.
	 *
	 *  \param id The name of the module in RPC.
	 *
	 *  \param f The module-level dispatcher's dispatch handler.
	 */
	template <typename Callable>
	void connect_dispatcher(const std::string& id, Callable&& f)
	{
		if(register_.count(id))
			throw dispatcher_exists(id);

		register_[id] = f;
	}

	//! Checks to see if the module id has a handler registered
	bool connected(const std::string& id) const;

	//! Disconnects module id; does nothing if it's not connected.
	void disconnect(const std::string& id);

protected:
	std::unordered_map<std::string, std::function<void (const Json::Value&,
			const Callback&)>> register_;

	void respond_with_error(const std::string& error, const
			connection_pool_type::Callback& cb) const;
};
