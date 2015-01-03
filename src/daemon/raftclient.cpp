#include <string>
#include <functional>
#include <fstream>
#include <set>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include <json/json.h>
#include <json_help.hpp>

#include "raftrpc.hpp"
#include "raftstate.hpp"
#include "raftrequest.hpp"
#include "raftclient.hpp"

raft::Client::Handlers::Handlers(const send_request_type& send_request, const append_to_log_type& append_to_log,
		const leader_type& leader)
	:send_request_(send_request),
	append_to_log_(append_to_log),
	leader_(leader)
{
}

void raft::Client::Handlers::send_request(const std::string& to, const Json::Value& request) const
{
	send_request_(to, request);
}

void raft::Client::Handlers::append_to_log(const Json::Value& request) const
{
	append_to_log_(request);
}

boost::optional<std::string> raft::Client::Handlers::leader() const
{
	return leader_();
}

raft::Client::Client(const std::string& id, raft::Client::Handlers& handlers)
	:id_(id),
	handlers_(handlers)
{
}

void raft::Client::commit_handler(const Json::Value& root)
{
	const std::string type = json_help::checked_from_json<std::string>(root, "type", "Bad commit RPC:");
	if(type == "update")
	{
		raft::request::Update entry(root);

		BOOST_LOG_TRIVIAL(info) << "Committing update on key: " << entry.key()
			<< " version: " << entry.old_version() << " -> " << entry.new_version();

		commit_if_valid(entry);
	}
	else if(type == "delete")
	{
		raft::request::Delete entry(root);
		BOOST_LOG_TRIVIAL(info) << "Committing delete on key: " << entry.key()
			<< " version: " << entry.version();

		auto commit_valid = valid(entry, version_map_);
		if(commit_valid == request_invalid && version_map_.count(entry.key()) == 1)
			throw std::runtime_error("Bad commit: conflicts");

		apply_to(entry, version_map_);
		//Remove the entry from pending if it's in there
		if(pending_version_map_.count(entry.key()) == 1 &&
				std::get<0>(pending_version_map_[entry.key()]) == entry.version())
			pending_version_map_.erase(entry.key());

		//Notify the commit handlers
		commit_notify(entry);
	}
	else if(type == "rename")
	{
		raft::request::Rename entry(root);
		BOOST_LOG_TRIVIAL(info) << "Committing rename on key: " << entry.key()
			<< " -> " << entry.new_key() << " version: " << entry.version();

		commit_if_valid(entry);
	}
	else if(type == "add")
	{
		raft::request::Add entry(root);
		BOOST_LOG_TRIVIAL(info) << "Committing add on key: " << entry.key()
			<< " version: " << entry.version();

		commit_if_valid(entry);
	}
	else throw std::runtime_error("Bad commit RPC: unknown type " + type);
}

bool raft::Client::exists(const std::string& key) const noexcept
{
	return version_map_.count(key) == 1;
}

std::tuple<std::string, std::string> raft::Client::operator [](const std::string& key) noexcept(false)
{
	return version_map_.at(key);
}
void raft::Client::commit_notify(const request::Update& rpc)
{
	commit_update_(rpc);
}

void raft::Client::commit_notify(const request::Rename& rpc)
{
	commit_rename_(rpc);
}

void raft::Client::commit_notify(const request::Delete& rpc)
{
	commit_delete_(rpc);
}

void raft::Client::commit_notify(const request::Add& rpc)
{
	commit_add_(rpc);
}


void raft::Client::apply_to(const raft::request::Update& update, version_map_type& version_map)
{
	version_map[update.key()] = std::make_tuple(update.new_version(), update.from());
}

void raft::Client::apply_to(const raft::request::Delete& update, version_map_type& version_map)
{
	version_map.erase(update.key());
}

void raft::Client::apply_to(const raft::request::Rename& update, version_map_type& version_map)
{
	version_map[update.new_key()] = std::make_tuple(std::get<0>(version_map[update.key()]),
			update.from());
	version_map.erase(update.key());
}

void raft::Client::apply_to(const raft::request::Add& update, version_map_type& version_map)
{
	version_map[update.key()] = std::make_tuple(update.version(), update.from());
}

bool raft::Client::check_conflict(const raft::request::Update& update) const
{
	if(pending_version_map_.count(update.key()))
		return std::get<0>(pending_version_map_.at(update.key())) != update.old_version();
	else if(version_map_.count(update.key()))
		return std::get<0>(version_map_.at(update.key())) != update.old_version();
	else
		return true;
}

bool raft::Client::check_conflict(const raft::request::Delete& del) const
{
	if(pending_version_map_.count(del.key()))
		return std::get<0>(pending_version_map_.at(del.key())) != del.version();
	else if(version_map_.count(del.key()))
		return std::get<0>(version_map_.at(del.key())) != del.version();

	return true;
}

bool raft::Client::check_conflict(const raft::request::Rename& rename) const
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

bool raft::Client::check_conflict(const raft::request::Add& add) const
{
	return version_map_.count(add.key()) == 1 || pending_version_map_.count(add.key()) == 1;
}


bool raft::Client::done(raft::request::Update::const_reference request, const version_map_type& version_map) const noexcept
{
	try
	{
		if(version_map.count(request.key()))
			return std::get<0>(version_map.at(request.key())) == request.new_version();
	}
	catch(std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::done(Update...). Ignoring. Details: "
			<< ex.what();
	}
	catch(...)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::done(Update...). Ignoring.";
	}

	return false;
}

bool raft::Client::done(raft::request::Delete::const_reference, const version_map_type&) const noexcept
{
	//can't tell if the've been done
	return false;
}

bool raft::Client::done(raft::request::Rename::const_reference request, const version_map_type& version_map) const noexcept
{
	try
	{
		if(version_map.count(request.new_key()))
			return std::get<0>(version_map.at(request.new_key())) == request.version();
	}
	catch(std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::done(Rename...). Ignoring. Details: "
			<< ex.what();
	}
	catch(...)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::done(Rename...). Ignoring.";
	}

	return false;
}

bool raft::Client::done(raft::request::Add::const_reference request, const version_map_type& version_map) const noexcept
{
	try
	{
		if(version_map.count(request.key()))
			return std::get<0>(version_map.at(request.key())) == request.version();
	}
	catch(std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::done(Add...). Ignoring. Details: "
			<< ex.what();
	}
	catch(...)
	{
		BOOST_LOG_TRIVIAL(warning) << "Unexpected exception in raft::Client::done(Add...). Ignoring.";
	}

	return false;
}
