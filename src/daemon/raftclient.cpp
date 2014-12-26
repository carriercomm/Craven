#include <string>
#include <functional>
#include <fstream>
#include <set>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include <json/json.h>
#include "../common/json_help.hpp"

#include "raftrpc.hpp"
#include "raftstate.hpp"
#include "raftclient.hpp"

namespace raft
{
	namespace request
	{
		Update::Update(const std::string& from, const std::string& key,
			   const std::string& old_version, const std::string& new_version)
			:Request(from, key),
			old_version_(old_version),
			new_version_(new_version)
		{
		}

		Update::Update(const Json::Value& root)
			:Request(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "update")
				throw std::runtime_error("Raft request not update.");

			old_version_ = checked_from_json<std::string>(root, "old_version");
			new_version_ = checked_from_json<std::string>(root, "new_version");
		}

		Update::operator Json::Value() const
		{
			Json::Value root = Request::operator Json::Value();

			root["type"] = "update";
			root["old_version"] = old_version_;
			root["new_version"] = new_version_;

			return root;
		}

		std::string Update::old_version() const
		{
			return old_version_;
		}

		std::string Update::new_version() const
		{
			return new_version_;
		}


		Delete::Delete(const std::string& from, const std::string& key,
				const std::string& version)
			:Request(from, key),
			version_(version)
		{
		}

		Delete::Delete(const Json::Value& root)
			:Request(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "delete")
				throw std::runtime_error("Raft request not delete.");

			version_ = checked_from_json<std::string>(root, "version");
		}

		Delete::operator Json::Value() const
		{
			Json::Value root = Request::operator Json::Value();

			root["type"] = "delete";
			root["version"] = version_;

			return root;
		}

		std::string Delete::version() const
		{
			return version_;
		}


		Rename::Rename(const std::string& from, const std::string& key,
				const std::string& new_key, const std::string& version)
			:Request(from, key),
			new_key_(new_key),
			version_(version)
		{
		}

		Rename::Rename(const Json::Value& root)
			:Request(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "rename")
				throw std::runtime_error("Raft request not rename.");

			new_key_ = checked_from_json<std::string>(root, "new_key");
			version_ = checked_from_json<std::string>(root, "version");
		}

		Rename::operator Json::Value() const
		{
			Json::Value root = Request::operator Json::Value();

			root["type"] = "rename";
			root["new_key"] = new_key_;
			root["version"] = version_;

			return root;
		}

		std::string Rename::new_key() const
		{
			return new_key_;
		}

		std::string Rename::version() const
		{
			return version_;
		}


		Add::Add(const std::string& from, const std::string& key,
				const std::string& version)
			:Request(from, key),
			version_(version)
		{
		}

		Add::Add(const Json::Value& root)
			:Request(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "add")
				throw std::runtime_error("Raft request not add.");

			version_ = checked_from_json<std::string>(root, "version");
		}

		Add::operator Json::Value() const
		{
			Json::Value root = Request::operator Json::Value();

			root["type"] = "add";
			root["version"] = version_;

			return root;
		}

		std::string Add::version() const
		{
			return version_;
		}
	}
}

std::ostream& operator <<(std::ostream& os, const raft::request::Update& update)
{
	return os << json_help::write(update);
}

std::ostream& operator <<(std::ostream& os, const raft::request::Delete& del);
std::ostream& operator <<(std::ostream& os, const raft::request::Rename& rename);
std::ostream& operator <<(std::ostream& os, const raft::request::Add& add);

ClientHandlers::ClientHandlers(const send_request_type& send_request, const append_to_log_type& append_to_log,
		const leader_type& leader)
	:send_request_(send_request),
	append_to_log_(append_to_log),
	leader_(leader)
{
}

void ClientHandlers::send_request(const std::string& to, const Json::Value& request) const
{
	send_request_(to, request);
}

void ClientHandlers::append_to_log(const Json::Value& request) const
{
	append_to_log_(request);
}

boost::optional<std::string> ClientHandlers::leader() const
{
	return leader_();
}

RaftClient::RaftClient(const std::string& id, ClientHandlers& handlers)
	:id_(id),
	handlers_(handlers)
{
	throw std::runtime_error("Not yet implemented");
}

void RaftClient::commit_handler(const Json::Value& root)
{
	throw std::runtime_error("Not yet implemented");
}

bool RaftClient::exists(const std::string& key) const noexcept
{
	throw std::runtime_error("Not yet implemented");
}

std::tuple<std::string, std::string> RaftClient::operator [](const std::string& key) noexcept(false)
{
	throw std::runtime_error("Not yet implemented");
}
