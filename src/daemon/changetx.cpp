#include <string>
#include <sstream>

#include <json/json.h>
#include <json_help.hpp>

#include "changetx.hpp"

namespace change
{
	namespace rpc
	{
		template <typename T>
		T base::checked_from_json(const Json::Value& root, const std::string& key) const
		{
			return json_help::checked_from_json<T>(root, key, "Bad json for change transfer RPC:");
		}

		base::base(const std::string& key, const std::string& version, const std::string& old_version,
				uint32_t start)
			:key_(key),
			version_(version),
			old_version_(old_version),
			start_(start)
		{
		}

		base::base(const Json::Value& root)
		{
			key_ = checked_from_json<std::string>(root, "key");
			version_ = checked_from_json<std::string>(root, "version");
			old_version_ = checked_from_json<std::string>(root, "old_version");
			start_ = checked_from_json<uint32_t>(root, "start");
		}

		base::operator Json::Value() const
		{
			Json::Value root;
			root["key"] = key_;
			root["version"] = version_;
			root["old_version"] = old_version_;
			root["start"] = start_;

			return root;
		}

		std::string base::key() const
		{
			return key_;
		}

		std::string base::version() const
		{
			return version_;
		}

		std::string base::old_version() const
		{
			return old_version_;
		}

		uint32_t base::start() const
		{
			return start_;
		}

		request::request(const std::string& key, const std::string& version, const std::string& old_version,
				uint32_t start)
			:base(key, version, old_version, start)
		{
		}

		request::request(const Json::Value& root)
			:base(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "request")
				throw std::runtime_error("Bad json for change transfer RPC: rpc is not a request");
		}

		request::operator Json::Value() const
		{
			auto root = base::operator Json::Value();

			root["type"] = "request";

			return root;
		}

		response::response(const std::string& key, const std::string& version, const std::string& old_version,
					uint32_t start, const std::string& data, error_code ec)
			:base(key, version, old_version, start),
			data_(data),
			ec_(ec)
		{

		}
		response::response(const request& respond_to, const std::string& data, error_code ec)
			:base(respond_to.key(), respond_to.version(), respond_to.old_version(),
					respond_to.start()),
			data_(data),
			ec_(ec)
		{

		}

		response::response(const Json::Value& root)
			:base(root)
		{
			std::string type = checked_from_json<std::string>(root, "type");

			if(type != "response")
				throw std::runtime_error("Bad json for change transfer RPC: rpc is not a response");

			data_ = checked_from_json<std::string>(root, "data");
			ec_ = static_cast<error_code>(checked_from_json<uint32_t>(root, "error_code"));

		}

		response::operator Json::Value() const
		{
			auto root = base::operator Json::Value();

			root["type"] = "response";

			root["data"] = data_;
			root["error_code"] = static_cast<uint32_t>(ec_);

			return root;
		}

		std::string response::data() const
		{
			return data_;
		}

		response::error_code response::ec() const
		{
			return ec_;
		}
	}
}
