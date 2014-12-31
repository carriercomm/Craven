#include <string>
#include <sstream>

#include <boost/filesystem.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

namespace fs = boost::filesystem;

#include <json/json.h>
#include <json_help.hpp>

#include "raftclient.hpp"

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

	change_transfer::scratch::scratch(const boost::filesystem::path& root_storage,
					const std::string& key, const std::string& version)
		:root_storage_(root_storage),
		key_(key),
		version_(version)
	{
		throw std::runtime_error("Not yet implemented");
	}

	fs::path change_transfer::scratch::operator()() const
	{
		throw std::runtime_error("Not yet implemented");
	}

	std::string change_transfer::scratch::key() const
	{
		return key_;
	}

	std::string change_transfer::scratch::version() const
	{
		return version_;
	}

	change_transfer::change_transfer(const boost::filesystem::path& root_storage,
				const std::function<void (const std::string&, const Json::Value&)> send_handler,
				raft::Client& raft_client)
		:root_storage_(root_storage),
		send_handler_(send_handler),
		raft_client_(raft_client)
	{
		throw std::runtime_error("Not yet implemented");
	}

	rpc::response change_transfer::request(const rpc::request& rpc)
	{
		throw std::runtime_error("Not yet implemented");
	}

	void change_transfer::response(const rpc::response& rpc)
	{
		throw std::runtime_error("Not yet implemented");
	}

	void change_transfer::commit_handler(const std::string& from, const std::string& key,
			const std::string& version)
	{
		throw std::runtime_error("Not yet implemented");
	}

	bool change_transfer::exists(const std::string& key) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	bool change_transfer::exists(const std::string& key, const std::string& version) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	std::vector<std::string> change_transfer::versions(const std::string& key) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	fs::path change_transfer::operator()(const std::string& key, const std::string& version) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	fs::path change_transfer::get(const std::string& key, const std::string& version) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	change_transfer::scratch change_transfer::open(const std::string& key, const std::string& version)
	{
		throw std::runtime_error("Not yet implemented");
	}

	std::string change_transfer::close(const scratch& scratch_info)
	{
		throw std::runtime_error("Not yet implemented");
	}

	void change_transfer::kill(const scratch& scratch_info)
	{
		throw std::runtime_error("Not yet implemented");
	}

	void change_transfer::rename(const std::string& new_key, const scratch& scratch_info)
	{
		throw std::runtime_error("Not yet implemented");
	}

}
