#pragma once

namespace raft
{
	namespace request
	{
		class Request
		{
		public:
			Request(const std::string& from, const std::string& key);
			Request(const Json::Value& root);

			operator Json::Value() const;

			std::string from() const;
			std::string key() const;


		protected:
			std::string from_;
			std::string key_;

			template <typename T>
			T checked_from_json(const Json::Value& root, const std::string& key) const;
		};

		class Update : public Request
		{
		public:
			Update(const std::string& from, const std::string& key,
				   const std::string& old_version, const std::string& new_version);

			Update(const Json::Value& root);

			operator Json::Value() const;

			std::string old_version() const;
			std::string new_version() const;

		protected:
			std::string old_version_;
			std::string new_version_;
		};

		class Delete : public Request
		{
		public:
			Delete(const std::string& from, const std::string& key,
					const std::string& version);

			Delete(const Json::Value& root);

			operator Json::Value() const;

			std::string version() const;

		protected:
			std::string version_;
		};

		class Rename : public Request
		{
		public:
			Rename(const std::string& from, const std::string& key,
					const std::string& new_key, const std::string& version);

			Rename(const Json::Value& root);

			operator Json::Value() const;

			std::string new_key() const;
			std::string version() const;

		protected:
			std::string new_key_;
			std::string version_;
		};

		class Add : public Request
		{
		public:
			Add(const std::string& from, const std::string& key,
					const std::string& version);
			Add(const Json::Value& root);

			operator Json::Value() const;

			std::string version() const;

		protected:
			std::string version_;
		};
	}
}

class ClientHandlers
{
public:

	void send_request(const std::string& to, const Json::Value& request);

	void send_commit(const std::string& to, const Json::Value& commit);

};

class RaftClient
{
public:
	RaftClient(RaftState& state, ClientHandlers& handlers);

	void request(const raft::request::Request& request);


};
