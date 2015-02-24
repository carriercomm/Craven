#pragma once

#include <algorithm>
#include <array>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/uuid/sha1.hpp>
#include <boost/signals2.hpp>

#include <b64_help.hpp>

#include "raftrequest.hpp"

#include "persist.hpp"


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
	template <std::size_t block_limit = 450>
	class change_transfer
	{
	public:
		//! Represents a scratch file
		class scratch
		{
			friend change_transfer;
			scratch(const boost::filesystem::path& resolved,
					const std::string& key, const std::string& version)
				:resolved_(resolved),
				key_(key),
				version_(version)
			{
			}
		public:

			boost::filesystem::path operator()() const
			{
				return resolved_;
			}

			std::string key() const
			{
				return key_;
			}

			std::string version() const
			{
				return version_;
			}

		protected:
			boost::filesystem::path resolved_;
			std::string key_, version_;
		};

		//! Construct the class
		/*!
		 *  \param root_storage The path to the root storage directory
		 *  \param send_handler The send handler, expected to wrap the provided
		 *  json in the required RPC labels.
		 */
		change_transfer(const std::vector<std::string>& nodes,
				const boost::filesystem::path& root_storage,
				const std::function<void (const std::string&, const Json::Value&)> send_handler)
			:root_(root_storage),
			nodes_(nodes),
			send_handler_(send_handler)
		{
			//Reset all pending
			for(const std::pair<std::string, std::string>& key_value : root_.versions())
			{
				if(boost::ends_with(std::get<1>(key_value), ".pending"))
					root_.kill(std::get<0>(key_value), std::get<1>(key_value));
			}

			//Continue transfers
			tick();
		}


		//! Continue any in-progress transfers.
		void tick()
		{
			//rate-limit
			unsigned transfer_remaining = 20;
			for(std::pair<const std::tuple<std::string, std::string>,
					pending_info>& pending : pending_)
			{
				if(transfer_remaining > 0)
				{
					continue_transfer(pending);
					--transfer_remaining;
				}
			}
		}

		//! Handler for request rpcs
		rpc::response request(const rpc::request& rpc)
		{
			if(!exists(rpc.key()))
				return rpc::response(rpc, "", rpc::response::no_key);

			if(!exists(rpc.key(), rpc.version()))
				return rpc::response(rpc, "", rpc::response::no_version);


			boost::filesystem::ifstream file(root_(rpc.key(), rpc.version()));

			file.seekg(rpc.start());

			if(file.fail())
				return rpc::response(rpc, "", rpc::response::eof);

			std::array<char, block_limit> buf;
			file.read(buf.data(), buf.size());
			std::string read_data(buf.begin(), buf.begin() + file.gcount());

			std::istringstream datastream(read_data);

			return rpc::response(rpc, b64_help::encode(datastream),
					file.eof() ? rpc::response::eof : rpc::response::ok);
		}

		//! Handler for response rpcs
		void response(const std::string& from, const rpc::response& rpc)
		{
			if(rpc.ec() == rpc::response::no_key ||
				rpc.ec() == rpc::response::no_version)
			{
				//remove the pending info
				if(pending_.count(std::make_tuple(rpc.key(),
								rpc.version())) == 1)
					pending_.erase(std::make_tuple(rpc.key(),
								rpc.version()));

				if(root_.exists(rpc.key(), rpc.version() + ".pending"))
					root_.kill(rpc.key(), rpc.version() + ".pending");

				BOOST_LOG_TRIVIAL(warning) << "Failure to retrieve ("
					<< rpc.key() << ", " << rpc.version()
					<< ") from " << from
					<< (rpc.ec() == rpc::response::no_key
							? ": no such key"
							: ": no such version"
					   );
			}
			// If the key & version exist, we have all of it
			else if(!exists(rpc.key(), rpc.version()))
			{
				std::string pending_vers = rpc.version() + ".pending";
				//check for the pending data
				if(pending_.count(std::make_tuple(rpc.key(), rpc.version())) == 0)
					pending_.insert(std::make_pair(std::make_tuple(rpc.key(), rpc.version()),
							pending_info{from, rpc.version()}));

				//Add the file if it doesn't exist
				boost::filesystem::path pending_path;

				if(!root_.exists(rpc.key(), pending_vers))
					pending_path = root_.add(rpc.key(), pending_vers);
				else
					pending_path = root_(rpc.key(), pending_vers);

				//create the file if it does not exist
				if(!boost::filesystem::exists(pending_path))
					boost::filesystem::ofstream of(pending_path);

				//Both output & input to avoid truncating the stream
				boost::filesystem::fstream of(pending_path, std::ios::binary | std::ios::out | std::ios::in);

				pending_info& info = pending_.at(std::make_tuple(rpc.key(), rpc.version()));

				//handle retry data
				if(info.retry_counter == 0)
				{
					info.reset_counter();
					info.from = from;
				}


				//check this is in the right place trivially
				bool valid = info.length <= rpc.start();
				if(!valid)
				{
					//check it aligns to the start of a gap
					for(std::tuple<uint32_t, uint32_t> gap : info.gaps)
						valid = valid || rpc.start() == std::get<0>(gap);
				}
				//special case: eof & data == "" & start = 0 -- empty file

				if(valid)
				{
					if(rpc.data() != "")
					{
						//detect if our spool would create a gap
						if(info.length < rpc.start())
						{
							info.gaps.push_back(std::make_tuple(info.length,
										rpc.start() - info.length));
						}


						//spool to the position in file
						//This can go past the end, in which case it fills
						//with 0 bytes -- what we want.
						of.seekp(rpc.start());

						if(of.fail())
							throw std::runtime_error("Failed file transfer");

						b64_help::decode(rpc.data(), of);

						uint32_t length = static_cast<uint32_t>(of.tellp()) - rpc.start();
						BOOST_LOG_TRIVIAL(trace) << "Transferred " << length << " bytes of data"
							<< " for (" << rpc.key() << ", " << rpc.version() << ") from "
							<< from;

						info.length = std::max(static_cast<uint32_t>(of.tellp()), info.length);

						//Delete any gaps whose start positions are the same
						//as the rpc's
						boost::range::remove_erase_if(info.gaps,
								[=](const std::tuple<uint32_t, uint32_t>& value)
								{
									return std::get<0>(value) == rpc.start();
								});

						if(rpc.ec() == rpc::response::eof)
							info.eof_seen = true;

						//No longer pending!
						if(info.eof_seen && info.gaps.empty())
							finish_transfer(from, rpc);
						else
							continue_transfer(rpc.key(), rpc.version());

					}
					//special case: empty file
					else if(rpc.start() == 0
							&& rpc.ec() == rpc::response::eof)
					{
						finish_transfer(from, rpc);
					}
					else //invalid
						BOOST_LOG_TRIVIAL(info) << "Response invalid.";
				}
				else //invalid
					BOOST_LOG_TRIVIAL(info) << "Response invalid.";
			}
			else
				BOOST_LOG_TRIVIAL(info) << "Ignoring update for (" << rpc.key() << ", " << rpc.version()
					<< ") from " << from << ": not marked as pending.";
		}

		//! Handler for update & add notifications
		void handle_new_version(const std::string& from, const std::string& key,
				const std::string& new_version, const std::string& old_version) noexcept
		{
			try
			{
				//check we don't already have it
				if(!root_.exists(key, new_version))
				{
					BOOST_LOG_TRIVIAL(info) << "Starting transfer of (" << key
						<< ", " << new_version << ") from " << from;

					std::string pending_vers = new_version + ".pending";

					//add the pending version
					if(!exists(key, pending_vers))
						root_.add(key, pending_vers);

					//setup the pending info
					if(pending_.count(std::make_tuple(key, new_version)) == 0)
						pending_.insert(std::make_pair(std::make_tuple(key, new_version),
								pending_info{from, new_version}));

					//fire request
					send_handler_(from, rpc::request(key, new_version, old_version, 0));
				}
			}
			catch(std::logic_error& ex)
			{
				BOOST_LOG_TRIVIAL(error) << "Failed to register commit of ("
					<< key << ", " << new_version
					<< ") with change transfer & persistence: "
					<< ex.what();
			}
			catch(...)
			{
				BOOST_LOG_TRIVIAL(error) << "Failed to register commit of ("
					<< key << ", " << new_version
					<< ") with change transfer & persistence";
			}
		}

		//! Handle the commit of an update RPC
		void commit_update(const raft::request::Update& rpc)
		{
			handle_new_version(rpc.from(), rpc.key(), rpc.new_version(), rpc.old_version());
		}

		//! Handle the commit of a rename RPC
		void commit_rename(const raft::request::Rename& rpc)
		{
			if(root_.exists(rpc.key(), rpc.version()))
			{
				if(!root_.exists(rpc.new_key(), rpc.version()))
				{
					root_.rename(rpc.key(), rpc.version(), rpc.new_key());
					notify_arrival_(rpc.new_key(), rpc.version());
				}
			}
			else //fetch it
				handle_new_version(rpc.from(), rpc.new_key(),
						rpc.version(), "");
		}

		//! Handle the commit of a delete rpc
		void commit_delete(const raft::request::Delete& rpc)
		{
			if(root_.exists(rpc.key(), rpc.version()))
				root_.kill(rpc.key(), rpc.version());
			else
				BOOST_LOG_TRIVIAL(error) << "Failed to persist delete ("
					<< rpc.key() << ", " << rpc.version() << ") from "
					<< rpc.from();
		}

		//! Handle the commit of an add rpc
		void commit_add(const raft::request::Add& rpc)
		{
			handle_new_version(rpc.from(), rpc.key(), rpc.version(), "");
		}

		//! Returns true if the key provided is known
		bool exists(const std::string& key) const
		{
			return root_.exists(key);
		}

		//! Returns true if the version exists for the provided key
		bool exists(const std::string& key, const std::string& version) const
		{
			//check the version's not pending as well
			return pending_.count(std::make_tuple(key, version)) == 0 &&
				root_.exists(key, version);
		}

		//! Returns the versions available for the specified key, not including
		//! scratches.
		std::vector<std::string> versions(const std::string& key) const
		{
			std::vector<std::string> retval;

			auto version_set = root_.versions();
			auto versions = version_set.equal_range(key);

			retval.reserve(version_set.count(key));

			using namespace boost::adaptors;

			boost::range::push_back(retval,
					versions | filtered([](const std::pair<std::string, std::string>& value)
						{
							return !(boost::algorithm::ends_with(value.second, "scratch")
									|| boost::algorithm::ends_with(value.second, "pending")
									);
						})
					| transformed([this](const std::pair<std::string, std::string>& value)
						{
							return value.second;
						})
					);

			//Premature optimisation?
			retval.shrink_to_fit();
			return retval;
		}

		//! Returns all available scratches for the specified key
		std::vector<scratch> scratches(const std::string& key) const
		{
			std::vector<scratch> retval;

			auto version_set = root_.versions();
			auto versions = version_set.equal_range(key);

			retval.reserve(version_set.count(key));

			using namespace boost::adaptors;

			boost::range::push_back(retval,
					versions | filtered([](const std::pair<std::string, std::string>& value)
						{
							return boost::algorithm::ends_with(value.second, "scratch");
						})
					| transformed([this](const std::pair<std::string, std::string>& value) -> scratch
						{
							return scratch(root_(value.first, value.second), value.first, value.second);
						})
					);

			//Premature optimisation?
			retval.shrink_to_fit();
			return retval;
		}

		//! Functor overload to retrieve the file containing the specified version
		//! of the specified key.
		boost::filesystem::path operator()(const std::string& key, const std::string& version) const
		{
			//check it's not pending
			if(pending_.count(std::make_tuple(key, version)) == 1)
				throw std::logic_error("Version transfer not complete");

			//and root_ will do the checks too
			return root_(key, version);
		}

		//! Convenience for pointer access to operator()()
		boost::filesystem::path get(const std::string& key, const std::string& version) const
		{
			return (*this)(key, version);
		}

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
		scratch open(const std::string& key, const std::string& version)
		{
			auto scratch_id = version + ".scratch";

			if(root_.exists(key, scratch_id)) //reset the scratch
				root_.kill(key, scratch_id);

			auto path = root_.add(key, scratch_id);

			//Copy the initial info over
			boost::filesystem::copy(root_(key, version), path);

			return scratch(path, key, scratch_id);
		}

		//! Generates a version that can be added to raft in an update RPC.
		std::string close(const scratch& scratch_info)
		{
			//check the file exists on-disk and if not, create it
			if(!boost::filesystem::exists(scratch_info()))
			{
				boost::filesystem::ofstream sc(scratch_info());
			}

			//Calculate version information
			auto new_version = sha1_hash(scratch_info());
			if(!root_.exists(scratch_info.key(), new_version))
				root_.rename(scratch_info.key(), scratch_info.version(),
						scratch_info.key(), new_version);

			return new_version;
		}

		//! Creates a new scratch for a key
		scratch add(const std::string& key)
		{
			boost::filesystem::path path;
			if(!root_.exists(key, ".scratch"))
				path = root_.add(key, ".scratch");
			else
				path = root_(key, ".scratch");

			return scratch(path, key, ".scratch");
		}

		//! Deletes a scratch
		void kill(const scratch& scratch_info)
		{
			root_.kill(scratch_info.key(), scratch_info.version());
		}

		//! Produces a new key from a scratch. Fails if that key exists.
		/*!
		 *  \returns The version of the new key that was created.
		 */
		std::string rename(const std::string& new_key, const scratch& scratch_info)
		{
			if(root_.exists(new_key))
				throw std::logic_error("Can't create a new key if it already exists");

			//Calculate version information
			auto new_version = sha1_hash(scratch_info());

			//Perform the rename
			root_.rename(scratch_info.key(), scratch_info.version(), new_key, new_version);

			return new_version;
		}

		//! Copies a key
		void copy(const std::string& key, const std::string& version, const std::string& new_key)
		{
			root_.copy(key, version, new_key);
		}

		//! Moves a scratch
		scratch move(const std::string& new_key, const scratch& scratch_info)
		{
			//Perform the rename
			root_.rename(scratch_info.key(), scratch_info.version(), new_key);

			return scratch(root_(new_key, scratch_info.version()), new_key,
						scratch_info.version());
		}

		template <typename Callable>
		boost::signals2::connection connect_arrival_notifications(Callable&& f)
		{
			return notify_arrival_.connect(std::forward<Callable>(f));
		}

		struct transfer_info
		{
			std::string key;
			std::string version;
			std::string from;
		};

		//! Retrieve in-progress transfers
		std::vector<transfer_info> transfers() const
		{
			std::vector<transfer_info> result;
			result.reserve(pending_.size());

			for(const auto& tx : pending_)
			{
				transfer_info info;
				info.key = std::get<0>(tx.first);
				info.version = std::get<1>(tx.first);
				info.from = tx.second.from;

				result.push_back(info);
			}

			return result;
		}


	protected:
		persistence root_;

		struct pending_info
		{
			pending_info(const std::string& from, const std::string& version)
				:eof_seen(false),
				length(0),
				from(from),
				version(version)
			{
				reset_counter();
			}

			void reset_counter()
			{
				retry_counter = 2;
			}

			//! True if we've seen a message tagged with eof
			bool eof_seen;

			//! If eof_seen is false, this is the last known byte. Otherwise it's
			//! the length of the file
			uint32_t length;

			//! Number of retries left before attempting all nodes
			uint32_t retry_counter;

			//! The node with the full version
			std::string from;

			//! True version, to save some string processing
			std::string version;

			//! The first element in each gap is the position, the second is the
			//! length.
			std::list<std::tuple<uint32_t, uint32_t>> gaps;
		};

		std::vector<std::string> nodes_;

		//! (key, version) -> pending info
		std::map<std::tuple<std::string, std::string>, pending_info> pending_;

		std::function<void (const std::string&, const Json::Value&)> send_handler_;
		boost::signals2::signal<void (const std::string&, const std::string&)> notify_arrival_;

		//! Generate the SHA1 hash of a file
		std::string sha1_hash(const boost::filesystem::path& file) const
		{
			uintmax_t size = boost::filesystem::file_size(file);
			//Max size for this sha1 impl is 2^32 bytes
			if(size > std::numeric_limits<uint32_t>::max())
				BOOST_LOG_TRIVIAL(warning) << "Maximum file size is 4GiB.";

			boost::uuids::detail::sha1 sha1;

			boost::filesystem::ifstream data(file, std::ios::in | std::ios::binary);
			std::array<char, 512> buf;

			while(!data.eof())
			{
				data.read(buf.data(), buf.size());
				sha1.process_bytes(buf.data(), data.gcount());
			}

			unsigned int hash[5];
			sha1.get_digest(hash);
			std::ostringstream os;
			//SHA1 hashes are 40 hex digits long, so 8 digits per block
			os << std::hex << std::setfill('0') << std::setw(8);

			for(std::size_t i = 0; i < 5; ++i)
				os << hash[i];

			return os.str();
		}

		void continue_transfer(const std::string& key, const std::string& version)
		{
			auto it = pending_.find(std::make_tuple(key, version));
			if(it == pending_.end())
				throw std::logic_error("No such key, version pair in pending: " + key + ", " + version);

			continue_transfer(*it);
		}

		void continue_transfer(std::pair<const std::tuple<std::string, std::string>,
				pending_info>& pending)
		{
				uint32_t start_from = 0;
				if(pending.second.gaps.empty())
					start_from = pending.second.length;
				else
					start_from = std::get<0>(pending.second.gaps.front());
				if(pending.second.retry_counter == 0)
				{
					for(const auto& target : nodes_)
					{
						send_handler_(pending.second.from,
								rpc::request(std::get<0>(pending.first),
									pending.second.version, "",
									start_from));
					}
				}
				else
				{
					--pending.second.retry_counter;
					send_handler_(pending.second.from,
							rpc::request(std::get<0>(pending.first),
								pending.second.version, "",
								start_from));
				}
		}

		//! Finish a transfer
		void finish_transfer(const std::string& from, const rpc::response& rpc)
		{
			//check the version doesn't already exist
			if(!root_.exists(rpc.key(), rpc.version()))
			{
				//rename the pending key
				root_.rename(rpc.key(), rpc.version() + ".pending",
						rpc.key(), rpc.version());
			}

			//Remove the pending data
			pending_.erase(std::make_tuple(rpc.key(),
						rpc.version()));

			BOOST_LOG_TRIVIAL(info) << "Transfer of (" << rpc.key() << ", " << rpc.version()
				<< ") from " << from << " complete.";

			notify_arrival_(rpc.key(), rpc.version());

		}
	};

}
