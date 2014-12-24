#pragma once

//! Helper functions for Json deserialisation
namespace json_help
{
	Json::Value parse(const std::string& line);
	bool parse(const std::string& line, Json::Value& root);

	std::string write(const Json::Value& root);

	template <typename T>
	T checked_from_json(const Json::Value& root, const std::string& key, const std::string& msg);

	template<>
	uint32_t checked_from_json<uint32_t>(const Json::Value& root, const std::string& key, const std::string& msg);

	template<>
	bool checked_from_json<bool>(const Json::Value& root, const std::string& key, const std::string& msg);

	template<>
	std::string checked_from_json<std::string>(const Json::Value& root, const std::string& key, const std::string& msg);

	template<>
	std::vector<std::string> checked_from_json<std::vector<std::string>>(const Json::Value& root, const std::string& key, const std::string& msg);

	template<>
	std::vector<Json::Value> checked_from_json<std::vector<Json::Value>>(const Json::Value& root, const std::string& key, const std::string& msg);

	template<>
	std::vector<std::tuple<uint32_t, Json::Value>> checked_from_json<std::vector<std::tuple<uint32_t, Json::Value>>>(const Json::Value& root, const std::string& key, const std::string& msg);
}
