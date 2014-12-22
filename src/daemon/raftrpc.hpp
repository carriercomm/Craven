#pragma once

#include "../common/json_help.hpp"

class append_entries_request
{
public:
	append_entries_request(uint32_t term, const std::string& candidate_id,
			uint32_t prev_log_index, uint32_t prev_log_term, const
			std::vector<Json::Value>& entries, uint32_t leader_commit);

	append_entries_request(const Json::Value& root);

	operator Json::Value() const;

	uint32_t term() const;
	std::string candidate_id() const;

	std::tuple<uint32_t, uint32_t> prev_log() const;
	uint32_t prev_log_index() const;
	uint32_t prev_log_term() const;

	std::vector<Json::Value> entries() const;

	uint32_t leader_commit() const;

private:
	//Private so its definition can be in the cpp
	template <typename T>
	T checked_from_json(const Json::Value& root, const std::string& key) const;

protected:
	uint32_t term_;
	std::string candidate_id_;

	//! Stores the term and index of prev log, in that order.
	std::tuple<uint32_t, uint32_t> prev_log_;
	std::vector<Json::Value> entries_;
	uint32_t leader_commit_;

};

class request_vote
{
public:
	request_vote(uint32_t term, const std::string& candidate_id, uint32_t last_log_term,
			uint32_t last_log_index);

	request_vote(const Json::Value& root);

	operator Json::Value() const;

	uint32_t term() const;
	std::string candidate_id() const;
	std::tuple<uint32_t, uint32_t> last_log() const;
	uint32_t last_log_index() const;
	uint32_t last_log_term() const;

private:
	//Private so its definition can be in the cpp
	template <typename T>
	T checked_from_json(const Json::Value& root, const std::string& key) const;

protected:
	uint32_t term_;
	std::string candidate_id_;

	//! Stores the term and index, in that order
	std::tuple<uint32_t, uint32_t> last_log_;
};
