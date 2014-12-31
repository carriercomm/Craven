#pragma once


namespace change
{
	namespace rpc
	{
		//! Base class for change transfer RPC
		class base
		{
		protected:
			//! Construct the base
			/*!
			 *  \param key The key whose value is being referenced
			 *  \param version The version of that value being referenced
			 *  \param old_version The last known version of that value
			 *  \param start The point to start in the file
			 */
			base(const std::string& key, const std::string& version, const std::string& old_version,
					uint32_t start);

			//! Construct the base from json
			base(const Json::Value& root);

			//! Convert the base to json
			operator Json::Value() const;

			//protected so its definition can be in the cpp
			template <typename T>
			T checked_from_json(const Json::Value& root, const std::string& key) const;

		public:

			std::string key() const;
			std::string version() const;
			std::string old_version() const;
			uint32_t start() const;

		protected:
			std::string key_, version_, old_version_;
			uint32_t start_;

		};

		//! Represents a request for the value of a key
		class request : public base
		{
		public:
			//! Constructs the request.
			/*!
			 *  \param key The key to retrieve the value for
			 *  \param version The desired version
			 *  \param old_version The currently-held version (for future delta
			 *  support)
			 *  \param start The byte to start this transfer at, for
			 *  multiple-packet transfers.
			 */
			request(const std::string& key, const std::string& version, const std::string& old_version,
					uint32_t start);

			//! Constructs the request from json
			request(const Json::Value& root);

			//! Constructs json from the request
			operator Json::Value() const;
		};

		//! Represents the response to a request
		class response : public base
		{
		public:
			//! Error code to report the status of the request
			enum error_code {
				ok,			//!< The request was fine
				eof,		//!< File transfer complete
				no_key,		//!< No such key on file
				no_version	//!< No such version on file
			};

			//! Construct the response
			/*!
			 *  \param key The key requested
			 *  \param version The version requested
			 *  \param old_version The old version the delta was derived from
			 *  (future).
			 *  \param start The position in the file this chunk starts at
			 *  \param data The base64-encoded data for this chunk
			 */
			response(const std::string& key, const std::string& version, const std::string& old_version,
					uint32_t start, const std::string& data, error_code ec);

			//! Construct the response from a request
			response(const request& respond_to, const std::string& data, error_code ec);

			//! Construct the response from json
			response(const Json::Value& root);

			//! Construct json from the response
			operator Json::Value() const;

			std::string data() const;
			error_code ec() const;

		protected:
			std::string data_;
			error_code ec_;
		};
	}

	//! The class handling change transfer
	class change_transfer
	{
	public:
		//! Represents a scratch file
		class scratch
		{
			friend change_transfer;
			scratch(const boost::filesystem::path& root_storage,
					const std::string& key, const std::string& version);
		public:

			boost::filesystem::path operator()() const;

			std::string key() const;
			std::string version() const;

		protected:
			boost::filesystem::path root_storage_;
			std::string key_, version_;
		};

		//! Construct the class
		/*!
		 *  \param root_storage The path to the root storage directory
		 *  \param send_handler The send handler, expected to wrap the provided
		 *  json in the required RPC labels.
		 */
		change_transfer(const boost::filesystem::path& root_storage,
				const std::function<void (const std::string&, const Json::Value&)> send_handler,
				raft::Client& raft_client);

		//! Handler for request rpcs
		rpc::response request(const rpc::request& rpc);

		//! Handler for response rpcs
		void response(const rpc::response& rpc);

		//! Handler for commit notificiations
		void commit_handler(const std::string& from, const std::string& key,
				const std::string& version);

		//! Returns true if the key provided is known
		bool exists(const std::string& key) const;

		//! Returns true if the version exists for the provided key
		bool exists(const std::string& key, const std::string& version) const;

		//! Returns the versions available for the specified key
		std::vector<std::string> versions(const std::string& key) const;

		//! Functor overload to retrieve the file containing the specified version
		//! of the specified key.
		boost::filesystem::path operator()(const std::string& key, const std::string& version) const;

		//! Convenience for pointer access to operator()()
		boost::filesystem::path get(const std::string& key, const std::string& version) const;

		//! Creates a scratch file for the specified key, starting from version.
		/*!
		 *  This function creates a scratch file for the specified key, where
		 *  changes can be stored until they're ready to be added to the system.
		 *
		 *  The scratch starts out with the version's content, so is suitable for
		 *  read operations too. If a previous call to open() has been made
		 *  without an intervening call to close(), this function will return the
		 *  existing scratch, ignoring the version.
		 *
		 *  Call close(key) to finalise this scratch into a referenceable version.
		 */
		scratch open(const std::string& key, const std::string& version);

		//! Generates a version that can be added to raft in an update RPC.
		std::string close(const scratch& scratch_info);

		//! Deletes a scratch
		void kill(const scratch& scratch_info);

		//! Produces a new key from a scratch. Fails if that key exists.
		void rename(const std::string& new_key, const scratch& scratch_info);

	protected:
		boost::filesystem::path root_storage_;
		std::function<void (const std::string&, const Json::Value&)> send_handler_;
		raft::Client& raft_client_;

	};
}
