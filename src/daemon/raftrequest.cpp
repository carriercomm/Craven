#include <string>

#include <json/json.h>
#include <json_help.hpp>

#include "raftrequest.hpp"

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

		std::string Update::version() const noexcept
		{
			return new_version_;
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
