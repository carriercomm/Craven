#include <cstdint>

#include <exception>
#include <string>
#include <vector>
#include <json/json.h>
#include "../common/json_help.hpp"

#include "raftrpc.hpp"

template <typename T>
T append_entries_request::checked_from_json(const Json::Value& root, const std::string& key) const
{
	return json_help::checked_from_json<T>(root, key, "Bad json for append_entries RPC:");
}

append_entries_request::append_entries_request(uint32_t term, const std::string& candidate_id,
		uint32_t prev_log_index, uint32_t prev_log_term, const
		std::vector<Json::Value>& entries, uint32_t leader_commit)
	:term_(term),
	candidate_id_(candidate_id),
	prev_log_(std::make_tuple(prev_log_term, prev_log_index)),
	entries_(entries),
	leader_commit_(leader_commit)

{

}

append_entries_request::append_entries_request(const Json::Value& root)
{
	std::string type = checked_from_json<std::string>(root, "type");

	if(!(type == "append_entries"))
		throw std::runtime_error("RPC not append_entries");

	term_ = checked_from_json<uint32_t>(root, "term");

	candidate_id_ = checked_from_json<std::string>(root, "candidate_id");

	prev_log_ = std::make_tuple(
			checked_from_json<uint32_t>(root, "prev_log_term"),
			checked_from_json<uint32_t>(root, "prev_log_index"));

	entries_ = checked_from_json<std::vector<Json::Value>>(root, "entries");

	leader_commit_ = checked_from_json<uint32_t>(root, "leader_commit");
}

append_entries_request::operator Json::Value() const
{
	Json::Value root;
	root["type"] = "append_entries";
	root["term"] = term_;
	root["candidate_id"] = candidate_id_;
	root["prev_log_term"] = prev_log_term();
	root["prev_log_index"] = prev_log_index();

	Json::Value entries;
	entries.resize(entries_.size());
	for(unsigned int i = 0; i < entries_.size(); ++i)
		entries[i] = entries_[i];

	root["entries"] = entries;
	root["leader_commit"] = leader_commit_;

	return root;
}

uint32_t append_entries_request::term() const
{
	return term_;
}

std::string append_entries_request::candidate_id() const
{
	return candidate_id_;
}

std::tuple<uint32_t, uint32_t> append_entries_request::prev_log() const
{
	return prev_log_;
}

uint32_t append_entries_request::prev_log_index() const
{
	return std::get<1>(prev_log_);
}

uint32_t append_entries_request::prev_log_term() const
{
	return std::get<0>(prev_log_);
}

std::vector<Json::Value> append_entries_request::entries() const
{
	return entries_;
}

uint32_t append_entries_request::leader_commit() const
{
	return leader_commit_;
}

template <typename T>
T request_vote::checked_from_json(const Json::Value& root, const std::string& key) const
{
	return json_help::checked_from_json<T>(root, key, "Bad json for request_vote RPC:");
}


request_vote::request_vote(uint32_t term, const std::string& candidate_id, uint32_t last_log_term,
		uint32_t last_log_index)
	:term_(term),
	candidate_id_(candidate_id),
	last_log_(std::make_tuple(last_log_term, last_log_index))
{
}

request_vote::request_vote(const Json::Value& root)
{
	std::string type = checked_from_json<std::string>(root, "type");

	if(!(type == "request_vote"))
		throw std::runtime_error("RPC not request_vote");

	term_ = checked_from_json<uint32_t>(root, "term");

	candidate_id_ = checked_from_json<std::string>(root, "candidate_id");

	last_log_ = std::make_tuple(
			checked_from_json<uint32_t>(root, "last_log_term"),
			checked_from_json<uint32_t>(root, "last_log_index"));
}

request_vote::operator Json::Value() const
{
	Json::Value root;
	root["type"] = "request_vote";

	root["term"] = term_;
	root["candidate_id"] = candidate_id_;

	root["last_log_term"] = last_log_term();
	root["last_log_index"] = last_log_index();

	return root;
}

uint32_t request_vote::term() const
{
	return term_;
}

std::string request_vote::candidate_id() const
{
	return candidate_id_;
}

std::tuple<uint32_t, uint32_t> request_vote::last_log() const
{
	return last_log_;
}

uint32_t request_vote::last_log_index() const
{
	return std::get<1>(last_log_);
}

uint32_t request_vote::last_log_term() const
{
	return std::get<0>(last_log_);
}
