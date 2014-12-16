
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <json/json.h>

#include "dispatch.hpp"

TopLevelDispatch::dispatcher_exists::dispatcher_exists(const std::string& id)
	:std::runtime_error("Dispatcher with ID " + id + " is already registered.")
{}

TopLevelDispatch::Callback::Callback(const std::string& module, const
		connection_pool_type::Callback& cb)
	:module_(module),
	wrapped_(cb)
{
}

TopLevelDispatch::Callback::operator bool() const
{
	return static_cast<bool>(wrapped_);
}

TopLevelDispatch::connection_pool_type::uid_type TopLevelDispatch::Callback::endpoint() const
{
	return wrapped_.endpoint();
}


void TopLevelDispatch::Callback::operator()(const Json::Value& msg)
{
	Json::Value root;
	root["module"] = module_;
	root["content"] = msg;

	Json::FastWriter writer;
	wrapped_(writer.write(root));
}

TopLevelDispatch::TopLevelDispatch()
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

void TopLevelDispatch::operator()(const std::string& msg, const
		TopLevelDispatch::connection_pool_type::Callback& cb)
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

bool TopLevelDispatch::connected(const std::string& id) const
{
	return register_.count(id);
}

void TopLevelDispatch::disconnect(const std::string& id)
{
	 if(connected(id))
		 register_.erase(id);
}

void TopLevelDispatch::respond_with_error(const std::string& error, const
		TopLevelDispatch::connection_pool_type::Callback& cb) const
{
	BOOST_LOG_TRIVIAL(warning) << error;

	Callback module_callback("dispatch", cb);
	Json::Value error_message;

	error_message["error"] = error;

	module_callback(error_message);
}
