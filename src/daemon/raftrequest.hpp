#pragma once

#include <json/json.h>
#include <json_help.hpp>

namespace raft
{
	namespace request
	{
		template <typename Derived>
		class Request
		{
		public:
			typedef Derived derived_type;
			typedef Derived& reference;
			typedef const Derived& const_reference;


		protected:
			Request(const std::string& from, const std::string& key)
				:from_(from),
				key_(key)
			{
			}

			Request(const Json::Value& root)
				:from_(checked_from_json<std::string>(root, "from")),
				key_(checked_from_json<std::string>(root, "key"))
			{
			}

			operator Json::Value() const
			{
				Json::Value root;
				root["from"] = from_;
				root["key"] = key_;

				return root;
			}

		public:

			bool operator==(const Request<Derived>& ot) const noexcept
			{
				return from_ == ot.from_ && key_ == ot.key_;
			}

			std::string from() const noexcept
			{
				return from_;
			}

			std::string key() const noexcept
			{
				return key_;
			}

			friend std::ostream& operator<<(std::ostream& os, const Derived& derived)
			{
				return os << json_help::write(derived);
			}

		protected:
			std::string from_;
			std::string key_;

			template <typename T>
			static T checked_from_json(const Json::Value& root, const std::string& key)
			{
				return json_help::checked_from_json<T>(root, key, "Bad json for raft request:");
			}
		};

		class Update : public Request<Update>
		{
		public:
			Update(const std::string& from, const std::string& key,
				   const std::string& old_version, const std::string& new_version);

			Update(const Json::Value& root);

			operator Json::Value() const;

			std::string old_version() const noexcept;
			//! Same as new_version; for compatibility
			std::string version() const noexcept;

			std::string new_version() const noexcept;

		protected:
			std::string old_version_;
			std::string new_version_;
		};

		template <typename Derived>
		class VersionRequest : public Request<Derived>
		{
		protected:
			VersionRequest(const std::string& from, const std::string& key,
					const std::string& version)
				:Request<Derived>::Request(from, key),
				version_(version)
			{
			}

			VersionRequest(const Json::Value& root)
				:Request<Derived>::Request(root),
				version_(Request<Derived>::template checked_from_json<std::string>(root, "version"))
			{
			}

			operator Json::Value() const
			{
				auto root = Request<Derived>::operator Json::Value();

				root["version"] = version_;

				return root;
			}

		public:
			std::string version() const noexcept
			{
				return version_;
			}

		protected:
			std::string version_;
		};

		class Delete : public VersionRequest<Delete>
		{
		public:
			Delete(const std::string& from, const std::string& key,
					const std::string& version);

			Delete(const Json::Value& root);

			operator Json::Value() const;

		};

		class Rename : public VersionRequest<Rename>
		{
		public:
			Rename(const std::string& from, const std::string& key,
					const std::string& new_key, const std::string& version);

			Rename(const Json::Value& root);

			operator Json::Value() const;

			std::string new_key() const noexcept;

		protected:
			std::string new_key_;
		};

		class Add : public VersionRequest<Add>
		{
		public:
			Add(const std::string& from, const std::string& key,
					const std::string& version);
			Add(const Json::Value& root);

			operator Json::Value() const;

		};

	}
}
