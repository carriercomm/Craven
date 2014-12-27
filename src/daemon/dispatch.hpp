#pragma once

#include <unordered_map>

#include "connection_pool.hpp"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

//! This class handles the top-level dispatch for the RPC. The class is also
//! responsible for top-level marshalling.
template <typename T>
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

	struct invalid_name : std::runtime_error
	{
		invalid_name()
			:std::runtime_error("Cannot register a handler with that name.")
		{}

	};

	//! Wrapper around the connection pool's callback that handles json serialising.
	class Callback
	{
	public:
		Callback() = default;
		Callback(const std::string& module, const std::string& reply,
				const typename connection_pool_type::Callback& cb)
			:module_(module),
			reply_(reply),
			wrapped_(cb)
		{
		}


		operator bool() const
		{
			return static_cast<bool>(wrapped_);
		}

		typename connection_pool_type::uid_type endpoint() const
		{
			return wrapped_.endpoint();
		}

		void operator()(const Json::Value& msg)
		{
			Json::Value root;
			root["module"] = module_;
			root["reply"] = reply_;
			root["content"] = msg;

			wrapped_(json_help::write(root));
		}

	protected:
		std::string module_, reply_;
		typename connection_pool_type::Callback wrapped_;
	};


	//! Construct the dispatch
	TopLevelDispatch(connection_pool_type& pool)
		:pool_(pool)
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
	void operator()(const std::string& msg, const typename connection_pool_type::Callback& cb)
	{
		Json::Value root;
		Json::Reader reader;
		if(reader.parse(msg, root, false))
		{
			if(check_message_valid(root))
			{
				auto module_id = root["module"].asString();

				if(connected(module_id))
				{
					Callback module_callback(root["reply"].asString(), module_id, cb);

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
	 *
	 *  \returns A function that can be used to send RPCs from your module. The
	 *  first argument of the function is the target node, the second the target
	 *  module. The final argument is the message to send.
	 */
	template <typename Callable>
	std::function<void(const std::string&, const std::string&, const Json::Value&)>
		connect_dispatcher(const std::string& id, Callable&& f)
	{
		if(register_.count(id))
			throw dispatcher_exists(id);

		register_[id] = f;

		return [this, id](const std::string& node, const std::string& module, const Json::Value& msg)
		{
			Json::Value root;
			root["module"] = module;
			root["reply"] = id;
			root["content"] = msg;

			try
			{
				pool_.send_targeted(node, json_help::write(root));
			}
			catch(std::exception& ex)
			{
				BOOST_LOG_TRIVIAL(error) << "Error sending marshalled RPC from " << id
					<< " to " << module << " on " << node << ": " << ex.what();
			}
			catch(...)
			{
				BOOST_LOG_TRIVIAL(error) << "Unknown error sending marshalled RPC";
			}

		};
	}

	//! Checks to see if the module id has a handler registered
	bool connected(const std::string& id) const
	{
		return register_.count(id);
	}

	//! Disconnects module id; does nothing if it's not connected.
	void disconnect(const std::string& id)
	{
		if(id == "dispatch")
			throw invalid_name();

		if(connected(id))
			register_.erase(id);
	}

protected:
	connection_pool_type& pool_;

	std::unordered_map<std::string, std::function<void (const Json::Value&,
			const Callback&)>> register_;

	bool check_message_valid(const Json::Value& msg)
	{
		return msg.isMember("module") && msg["module"].isString()
			&& msg.isMember("reply") && msg["reply"].isString()
			&& msg.isMember("content");
	}

	void respond_with_error(const std::string& error, const
			typename connection_pool_type::Callback& cb) const
	{
		BOOST_LOG_TRIVIAL(warning) << error;

		Callback module_callback("dispatch", "dispatch", cb);
		Json::Value error_message;

		error_message["error"] = error;

		module_callback(error_message);
	}
};
