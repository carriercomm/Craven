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

		commit_if_valid(entry);
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
	else
		throw std::runtime_error("Bad commit RPC: unknown type " + type);
}

bool raft::Client::exists(const std::string& key) const noexcept
{
	exists_map(key, version_map_);
}

std::tuple<std::string, std::string> raft::Client::operator [](const std::string& key) noexcept(false)
{
	return *version_map_.at(key);
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

raft::Client::version_map_type& raft::Client::fetch_map(apply_target target)
{
	if(target == apply_main)
		return version_map_;
	else
		return pending_version_map_;
}

bool raft::Client::exists_map(const std::string& key, const version_map_type& map) noexcept
{
	return map.count(key) == 1 && map.at(key);
}

void raft::Client::apply_to(const raft::request::Update& update, apply_target target)
{
	fetch_map(target)[update.key()] = std::make_tuple(update.new_version(), update.from());
}

void raft::Client::apply_to(const raft::request::Delete& update, apply_target target)
{
	if(apply_pending == target)
		pending_version_map_[update.key()] = boost::none;
	else
	{
		//if there's a tombstone in pending, clean it up
		if(pending_version_map_.count(update.key())
				&& pending_version_map_[update.key()])
			pending_version_map_.erase(update.key());
		fetch_map(target).erase(update.key());
	}
}

void raft::Client::apply_to(const raft::request::Rename& update, apply_target target)
{
	//fetch the
	if(target == apply_pending)
	{ //if the from isn't in pending, it needs to be in main
		std::string version;
		if(exists_map(update.key(), pending_version_map_))
		{
			version = std::get<0>(*pending_version_map_.at(update.key()));
			//erase it
			pending_version_map_[update.key()] = boost::none;
		}
		else //let it be bounds checked
			version = std::get<0>(*version_map_.at(update.key()));

		//Add the rename
		pending_version_map_[update.new_key()] = std::make_tuple(version,
				update.from());
	}
	else
	{ //only read the main
		version_map_[update.new_key()] = std::make_tuple(std::get<0>(*version_map_[update.key()]),
				update.from());
		version_map_.erase(update.key());
	}
}

void raft::Client::apply_to(const raft::request::Add& update, apply_target target)
{
	fetch_map(target)[update.key()] = std::make_tuple(update.version(), update.from());
}

raft::Client::validity raft::Client::request_traits<raft::request::Update>::valid(
		const rpc_type& rpc, const version_map_type& version_map,
		const version_map_type& pending_map)
{
	if(!exists_map(rpc.key(), pending_map))
		return valid_impl(rpc, version_map);
	else
		return valid_impl(rpc, pending_map);
}

raft::Client::validity raft::Client::request_traits<raft::request::Update>::valid(
		const rpc_type& rpc, const version_map_type& version_map)
{
	return valid_impl(rpc, version_map);
}

raft::Client::validity raft::Client::request_traits<raft::request::Update>
	::valid_impl(const rpc_type& rpc, const version_map_type& version_map)
{
	if(exists_map(rpc.key(), version_map))
	{
		if(std::get<0>(*version_map.at(rpc.key())) == rpc.new_version())
			return request_done;
		else
			return std::get<0>(*version_map.at(rpc.key())) == rpc.old_version()
				? request_valid : request_invalid;
	}
	else
		return request_invalid;
}

raft::Client::validity raft::Client::request_traits<raft::request::Delete>::valid(
		const rpc_type& rpc, const version_map_type& version_map,
		const version_map_type& pending_map)
{
	//not using exists_map because if there're tombstones in pending we don't want
	//to mark valid.
	if(pending_map.count(rpc.key()) == 0)
		return valid_impl(rpc, version_map);
	else
		return valid_impl(rpc, pending_map);
}

raft::Client::validity raft::Client::request_traits<raft::request::Delete>::valid(
		const rpc_type& rpc, const version_map_type& version_map)
{
	return valid_impl(rpc, version_map);
}

raft::Client::validity raft::Client::request_traits<raft::request::Delete>
	::valid_impl(const rpc_type& rpc, const version_map_type& version_map)
{
	if(version_map.count(rpc.key()) == 1
			&& version_map.at(rpc.key()))
		return std::get<0>(*version_map.at(rpc.key())) == rpc.version()
			? request_valid : request_invalid;
	else
		return version_map.count(rpc.key()) == 1 ? request_invalid : request_done;
}

raft::Client::validity raft::Client::request_traits<raft::request::Rename>::valid(
		const rpc_type& rpc, const version_map_type& version_map,
		const version_map_type& pending_map)
{
	auto from_version = most_recent(rpc.key(), version_map, pending_map);
	auto to_version = most_recent(rpc.new_key(), version_map, pending_map);

	if(!from_version && to_version && *to_version == rpc.version())
		return request_done;
	if(from_version && !to_version && *from_version == rpc.version())
		return request_valid;

	return request_invalid;
}

raft::Client::validity raft::Client::request_traits<raft::request::Rename>::valid(
		const rpc_type& rpc, const version_map_type& version_map)
{
	if(exists_map(rpc.key(), version_map)
			&& std::get<0>(*version_map.at(rpc.key())) == rpc.version())
		return request_valid;
	else if(exists_map(rpc.new_key(), version_map)
			&& std::get<0>(*version_map.at(rpc.new_key())) == rpc.version())
		return request_done;
	else
		return request_invalid;
}

boost::optional<std::string> raft::Client
	::request_traits<raft::request::Rename>::most_recent(
		const std::string& key, const version_map_type& version_map,
		const version_map_type& pending_map)
{
	if(exists_map(key, pending_map))
		return std::get<0>(*pending_map.at(key));
	else
	{
		if(exists_map(key, version_map))
			return std::get<0>(*version_map.at(key));
		else
			return boost::none;
	}
}

raft::Client::validity raft::Client::request_traits<raft::request::Add>::valid(
		const rpc_type& rpc, const version_map_type& version_map,
		const version_map_type& pending_map)
{
	if(!exists_map(rpc.key(), pending_map))
	{
		if(exists_map(rpc.key(), version_map))
		{
			return std::get<0>(*version_map.at(rpc.key())) == rpc.version()
				? request_done : request_invalid;
		}
		else
			return request_valid;
	}
	else
	{
		return std::get<0>(*pending_map.at(rpc.key())) == rpc.version()
			? request_done : request_invalid;
	}
}

raft::Client::validity raft::Client::request_traits<raft::request::Add>::valid(
		const rpc_type& rpc, const version_map_type& version_map)
{
	if(!exists_map(rpc.key(), version_map))
		return request_valid;
	else if(std::get<0>(*version_map.at(rpc.key())) == rpc.version())
		return request_done;
	else
		return request_invalid;
}
