#include <cstdint>

#include <string>
#include <vector>
#include <iterator>

#include <json/json.h>

#include "json_help.hpp"

Json::Value json_help::parse(const std::string& line)
{
	Json::Reader r;
	Json::Value value;
	r.parse(line, value);
	return value;
}

bool json_help::parse(const std::string& line, Json::Value& root)
{
	Json::Reader r;
	return r.parse(line, root);
}

std::string json_help::write(const Json::Value& root)
{
	Json::FastWriter w;
	return w.write(root);
}


void membership_check(const Json::Value& root, const std::string& key, const std::string& msg)
{
	if(!root.isMember(key))
		throw std::runtime_error(msg + " no such member \"" + key + "\"");
}

template<>
uint32_t json_help::checked_from_json<uint32_t>(const Json::Value& root, const std::string& key, const std::string& msg)
{
	membership_check(root, key, msg);

	if(!root[key].isInt() || !root[key].asInt() >= 0)
		throw std::runtime_error(msg + " " + key + " is not a uint");

	return static_cast<uint32_t>(root[key].asInt());
}

template<>
std::string json_help::checked_from_json<std::string>(const  Json::Value& root, const std::string& key, const std::string& msg)
{
	membership_check(root, key, msg);

	if(!root[key].isString())
		throw std::runtime_error(msg + " " + key + " is not a string");

	return root[key].asString();
}

template<>
std::vector<std::string> json_help::checked_from_json<std::vector<std::string>>(const
		Json::Value& root, const std::string& key, const std::string& msg)
{
	membership_check(root, key, msg);

	if(!root[key].isArray())
		throw std::runtime_error(msg + " " + key + " is not an array");

	std::vector<std::string> value;
	value.reserve(root[key].size());

	for(const Json::Value& val : root[key])
	{
		if(!val.isString())
			throw std::runtime_error(msg + " " + key + " element is not a string");

		value.push_back(val.asString());
	}

	return value;
}


template<>
std::vector<Json::Value> json_help::checked_from_json<std::vector<Json::Value>>(const
		Json::Value& root, const std::string& key, const std::string& msg)
{
	membership_check(root, key, msg);

	if(!root[key].isArray())
		throw std::runtime_error(msg + " " + key + " is not an array");

	std::vector<Json::Value> value;
	value.reserve(root[key].size());

	for(const Json::Value& val : root[key])
		value.push_back(val);

	return value;
}

template<>
std::vector<std::tuple<uint32_t, Json::Value>> json_help::checked_from_json
	<std::vector<std::tuple<uint32_t, Json::Value>>>(
			const Json::Value& root, const std::string& key, const std::string& msg)
{
	membership_check(root, key, msg);

	if(!root[key].isArray())
		throw std::runtime_error(msg + " " + key + " is not an array");

	std::vector<std::tuple<uint32_t, Json::Value>> value;
	value.reserve(root[key].size());

	for(const Json::Value& val : root[key])
	{
		if(!val.isArray() || val.size() != 2)
			throw std::runtime_error(msg + " " + key + " is not an array of two-tuples.");

		if(!val[0u].isInt() || val[0u].asInt() < 0)
			throw std::runtime_error(msg + " " + key + " contains an invalid pair.");

		value.push_back(std::make_tuple(static_cast<uint32_t>(val[0u].asInt()), val[1u]));

	}

	return value;

}
