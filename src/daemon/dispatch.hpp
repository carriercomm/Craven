#pragma once

#include <unordered_map>

#include "connection_pool.hpp"

//! This class handles the top-level dispatch for the RPC. The class is also
//! responsible for top-level marshalling.
<template T>
class TopLevelDispatch
{
public:
	typedef T connection_pool_type;

	//! Exception thrown when connect_dispatcher is called with an ID that's
	//already registered.
	struct dispatcher_exists : std::runtime_error
	{
		dispatcher_exists(const std::string& id)
			:std::runtime_error("Dispatcher with ID " + id + " is already registered.")
		{}
	};

	//! Wrapper around the connection pool's callback that handles json serialising.
	class Callback
	{
	public:
		Callback() = default;
		Callback(const std::string& module, const
				connection_pool_type::Callback& cb)
			:module_(module),
			wrapped_(cb)
		{
		}


		operator bool() const
		{
			return static_cast<bool>(wrapped_);
		}

		connection_pool_type::uid_type endpoint() const
		{
			return wrapped_.endpoint();
		}

		void operator()(const Json::Value& msg)
		{
			Json::Value root;
			root["module"] = module_;
			root["content"] = msg;

			Json::FastWriter writer;
			wrapped_(writer.write(root));
		}

	protected:
		std::string module_;
		connection_pool_type::Callback wrapped_;
	};


	//! Construct the dispatch
	TopLevelDispatch()
	{
		register_["dispatch"] = [this](const Json::Value& msg, const
				Callback& cb)
		{
			try
			{
				BOOST_LOG_TRIVIAL(warning) << "Error message from "
					<< cb.endpoint() << ": " << msg["error"].asString();
			}
			catch(std::runtime_error& ex)
			{
				BOOST_LOG_TRIVIAL(error) << "Error in top-level dispatch error handler.";
			}
		};
	}

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
	void operator()(const std::string& msg, const connection_pool_type::Callback& cb)
	{
		Json::Value root;
		Json::Reader reader;
		if(reader.parse(msg, root, false))
		{
			if(root.isMember("module") && root["module"].isString()
					&& root.isMember("content"))
			{
				auto module_id = root["module"].asString();

				if(connected(module_id))
				{
					Callback module_callback(module_id, cb);

					//Call the registered dispatch handler for the module
					register_[module_id](root["content"], module_callback);
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "RPC for unconnected module "
						<< root["module"];
			}
			else
				respond_with_error("Bad JSON format: need module and content members", cb);
		}
		else
		{
			std::ostringstream os;

			os << "JSON parse error in top level dispatch: "
				<< reader.getFormattedErrorMessages();

			respond_with_error(os.str(), cb);
		}
	}

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
	bool connected(const std::string& id) const
	{
		return register_.count(id);
	}

	//! Disconnects module id; does nothing if it's not connected.
	void disconnect(const std::string& id)
	{
		if(connected(id))
			register_.erase(id);
	}

protected:
	std::unordered_map<std::string, std::function<void (const Json::Value&,
			const Callback&)>> register_;

	void respond_with_error(const std::string& error, const
			connection_pool_type::Callback& cb) const
	{
		BOOST_LOG_TRIVIAL(warning) << error;

		Callback module_callback("dispatch", cb);
		Json::Value error_message;

		error_message["error"] = error;

		module_callback(error_message);
	}
};
