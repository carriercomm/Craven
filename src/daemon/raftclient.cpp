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

		std::string Update::old_version() const noexcept
		{
			return old_version_;
		}

		std::string Update::new_version() const noexcept
		{
			return new_version_;
		}


		Delete::Delete(const std::string& from, const std::string& key,
				const std::string& version)
			:VersionRequest(from, key, version)
		{
		}

		Delete::Delete(const Json::Value& root)
			:VersionRequest(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "delete")
				throw std::runtime_error("Raft request not delete.");
		}

		Delete::operator Json::Value() const
		{
			Json::Value root = VersionRequest::operator Json::Value();

			root["type"] = "delete";

			return root;
		}


		Rename::Rename(const std::string& from, const std::string& key,
				const std::string& new_key, const std::string& version)
			:VersionRequest(from, key, version),
			new_key_(new_key)
		{
		}

		Rename::Rename(const Json::Value& root)
			:VersionRequest(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "rename")
				throw std::runtime_error("Raft request not rename.");

			new_key_ = checked_from_json<std::string>(root, "new_key");
		}

		Rename::operator Json::Value() const
		{
			Json::Value root = VersionRequest::operator Json::Value();

			root["type"] = "rename";
			root["new_key"] = new_key_;

			return root;
		}

		std::string Rename::new_key() const noexcept
		{
			return new_key_;
		}


		Add::Add(const std::string& from, const std::string& key,
				const std::string& version)
			:VersionRequest(from, key, version)
		{
		}

		Add::Add(const Json::Value& root)
			:VersionRequest(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "add")
				throw std::runtime_error("Raft request not add.");

		}

		Add::operator Json::Value() const
		{
			Json::Value root = VersionRequest::operator Json::Value();

			root["type"] = "add";
			root["version"] = version_;

			return root;
		}

	}
}


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
}

void RaftClient::commit_handler(const Json::Value& root)
{
	const std::string type = json_help::checked_from_json<std::string>(root, "type", "Bad commit RPC:");
	if(type == "update")
	{
		raft::request::Update entry(root);
		commit_if_valid(entry);

	}
	else if(type == "delete")
	{
		raft::request::Delete entry(root);
		auto commit_valid = valid(entry, version_map_);
		if(commit_valid == request_invalid && version_map_.count(entry.key()) == 1)
			throw std::runtime_error("Bad commit: conflicts");

		apply_to(entry, version_map_);
	}
	else if(type == "rename")
	{
		raft::request::Rename entry(root);
		commit_if_valid(entry);
	}
	else if(type == "add")
	{
		raft::request::Add entry(root);
		commit_if_valid(entry);
	}
	else throw std::runtime_error("Bad commit RPC: unknown type " + type);
}

bool RaftClient::exists(const std::string& key) const noexcept
{
	return version_map_.count(key) == 1;
}

std::tuple<std::string, std::string> RaftClient::operator [](const std::string& key) noexcept(false)
{
	return version_map_.at(key);
}

void RaftClient::apply_to(const raft::request::Update& update, version_map_type& version_map)
{
	version_map[update.key()] = std::make_tuple(update.new_version(), update.from());
}

void RaftClient::apply_to(const raft::request::Delete& update, version_map_type& version_map)
{
	version_map.erase(update.key());
}

void RaftClient::apply_to(const raft::request::Rename& update, version_map_type& version_map)
{
	version_map[update.new_key()] = std::make_tuple(std::get<0>(version_map[update.key()]),
			update.from());
	version_map.erase(update.key());
}

void RaftClient::apply_to(const raft::request::Add& update, version_map_type& version_map)
{
	version_map[update.key()] = std::make_tuple(update.version(), update.from());
}

bool RaftClient::check_conflict(const raft::request::Update& update) const
{
	if(pending_version_map_.count(update.key()))
		return std::get<0>(pending_version_map_.at(update.key())) != update.old_version();
	else if(version_map_.count(update.key()))
		return std::get<0>(version_map_.at(update.key())) != update.old_version();
	else
		return true;
}

bool RaftClient::check_conflict(const raft::request::Delete& del) const
{
	if(pending_version_map_.count(del.key()))
		return std::get<0>(pending_version_map_.at(del.key())) != del.version();
	else if(version_map_.count(del.key()))
		return std::get<0>(version_map_.at(del.key())) != del.version();

	return true;
}

bool RaftClient::check_conflict(const raft::request::Rename& rename) const
{
	if(!(pending_version_map_.count(rename.new_key()) == 1
		|| version_map_.count(rename.new_key()) == 1))
	{
		if(pending_version_map_.count(rename.key()))
			return std::get<0>(pending_version_map_.at(rename.key())) != rename.version();
		else if(version_map_.count(rename.key()))
			return std::get<0>(version_map_.at(rename.key())) != rename.version();
	}

	return true;
}

bool RaftClient::check_conflict(const raft::request::Add& add) const
{
	return version_map_.count(add.key()) == 1 || pending_version_map_.count(add.key()) == 1;
}


bool RaftClient::done(raft::request::Update::const_reference request, const version_map_type& version_map) const noexcept
{
	try
	{
		if(version_map.count(request.key()))
			return std::get<0>(version_map.at(request.key())) == request.new_version();
	}
	catch(std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::done(Update...). Ignoring. Details: "
			<< ex.what();
	}
	catch(...)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::done(Update...). Ignoring.";
	}

	return false;
}

bool RaftClient::done(raft::request::Delete::const_reference request, const version_map_type& version_map) const noexcept
{
	//can't tell if the've been done
	return false;
}

bool RaftClient::done(raft::request::Rename::const_reference request, const version_map_type& version_map) const noexcept
{
	try
	{
		if(version_map.count(request.new_key()))
			return std::get<0>(version_map.at(request.new_key())) == request.version();
	}
	catch(std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::done(Rename...). Ignoring. Details: "
			<< ex.what();
	}
	catch(...)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::done(Rename...). Ignoring.";
	}

	return false;
}

bool RaftClient::done(raft::request::Add::const_reference request, const version_map_type& version_map) const noexcept
{
	try
	{
		if(version_map.count(request.key()))
			return std::get<0>(version_map.at(request.key())) == request.version();
	}
	catch(std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::done(Add...). Ignoring. Details: "
			<< ex.what();
	}
	catch(...)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in RaftClient::done(Add...). Ignoring.";
	}

	return false;
}
