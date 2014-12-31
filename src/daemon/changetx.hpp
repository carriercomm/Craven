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
}
