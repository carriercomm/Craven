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

template <typename T>
T raft::request::Request::checked_from_json(const Json::Value& root, const std::string& key) const
{
	return json_help::checked_from_json<T>(root, key, "Bad json for raft request:");
}

namespace raft
{
	namespace request
	{
		Request::Request(const std::string& from, const std::string& key)
			:from_(from),
			key_(key)
		{
		}

		Request::Request(const Json::Value& root)
		{
			from_ = checked_from_json<std::string>(root, "from");
			key_ = checked_from_json<std::string>(root, "key");
		}

		Request::operator Json::Value() const
		{
			Json::Value root;
			root["from"] = from_;
			root["key"] = key_;

			return root;
		}

		std::string Request::from() const
		{
			return from_;
		}

		std::string Request::key() const
		{
			return key_;
		}

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
