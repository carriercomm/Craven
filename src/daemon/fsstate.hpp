#pragma once

#include <unordered_map>
#include <list>
#include <deque>
#include <type_traits>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/optional.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm_ext/erase.hpp>


#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "raftclient.hpp"
#include "changetx.hpp"


namespace dfs
{
	std::string encode_path(const std::string& path);
	std::string decode_path(const std::string& path);

	struct ordinary_prepare_tag
	{
	};
	struct rename_prepare_tag
	{
	};

	template <typename Client, typename ChangeTx>
	class basic_state
	{
	public:
		//! Node information stored in the dcache
		struct node_info
		{
			node_info()
				:inode(0),
				fds(0)
			{
			}

			//! Make a directory
			node_info(const std::string& name, uint64_t inode)
				:type(dir),
				state(clean),
				inode(inode),
				fds(0),
				name(name)
			{
			}

			node_info(const std::string& name, const std::string& version, bool arrived)
				:type(file),
				state(arrived ? clean : pending),
				inode(0), //in the interests of consistency
				fds(0),
				name(name),
				version(version)
			{
			}

			//! Retrieve the active version (if there is one). In the case of an
			//! active write, this will return boost::none
			boost::optional<std::string> active_version() const
			{
				if(state == clean || state == dirty || state == active_read
						|| state == novel)
					return version;
				else if(state == pending)
					return previous_version;

				return boost::none;
			}


			//! The type of the node, to save an extra lookup
			enum node_type {
				dir,	//!< A directory
				file	//!< A file
			};

			//! The state of the node
			/*!
			 *  If the node is active, its version is the version it was derived
			 *  from and the scratch information holds where it's being written
			 *  to. Once FUSE releases it, this should be committed and marked
			 *  dirty.
			 */
			enum state_type {
				clean,	//!< The node is clean -- the same as the sync'd version
				pending,//!< The node is pending -- the current version of this file hasn't arrived
				dirty,	//!< The node is dirty -- it requires syncing, but isn't active.
				active_write,	//!< The node is actively being written to.
				active_read,	//!< The node is actively being read from (i.e. read-only access).
				novel,	//!< The node is new.
				dead	//!< The node has been deleted but the delete is not yet synced
			};

			friend std::ostream& operator<<(std::ostream& os, state_type st)
			{
				return os << state_type_tl_.at(st);
			}

			node_type type;
			state_type state;

			//! Inode number (must be unique, only meaningful on directories).
			uint64_t inode;

			//! Number of open file
			uint64_t fds;

			//! Rename information.
			/*!
			 *  If state == dead && rename_info is valid, this is a `from' marker
			 *  that can be cleaned up on commit. If state == novel && rename_info
			 *  is valid, this is a `to' marker and can be set to clean with its
			 *  rename_info reset on commit.
			 */
			boost::optional<std::string> rename_info;

			//! Scratch info, for if we're active
			boost::optional<typename ChangeTx::scratch> scratch_info;

			//! Previous version, if we're pending
			boost::optional<std::string> previous_version;

			//! The name of the node
			/*!
			 *  In the dcache, this is the filename and the full path is implied.
			 *  Elsewhere, this is the full name.
			 */
			std::string name;

			//! The version of the node. Empty for novel files & directories.
			std::string version;

		private:
			static const std::map<state_type, std::string> state_type_tl_;
		};

		basic_state(Client& client, ChangeTx& changetx, const std::string& id, uid_t uid, gid_t gid)
			:client_(client),
			changetx_(changetx),
			id_(id),
			next_inode_(0),
			uid_(uid),
			gid_(gid)
		{
			//install handlers
			client_.connect_commit_update(std::bind(
						&basic_state<Client, ChangeTx>::commit_update,
						this, std::placeholders::_1));
			client_.connect_commit_delete(std::bind(
						&basic_state<Client, ChangeTx>::commit_delete,
						this, std::placeholders::_1));
			client_.connect_commit_rename(std::bind(
						&basic_state<Client, ChangeTx>::commit_rename,
						this, std::placeholders::_1));
			client_.connect_commit_add(std::bind(
						&basic_state<Client, ChangeTx>::commit_add,
						this, std::placeholders::_1));

			changetx_.connect_arrival_notifications(std::bind(
						&basic_state<Client, ChangeTx>::notify_arrival, this,
						std::placeholders::_1, std::placeholders::_2));

			//set up root
			make_directories("/");
		}


		//! Fires off all change requests. Call periodically
		void tick();

		//! Handler for commit notification from the raft client.
		/*!
		 *  This function handles an incoming committed change to the filesystem.
		 *  It first checks to see if this change is one we've requested (if it
		 *  is, it marks those changes as synced), then checks to see if the
		 *  change conflicts before caching the information it provides & handling
		 *  any conflicts it causes.
		 */
		void commit_update(const raft::request::Update& rpc);

		//! \overload
		void commit_delete(const raft::request::Delete& rpc);

		//! \overload
		void commit_rename(const raft::request::Rename& rpc);

		//! \overload
		void commit_add(const raft::request::Add& rpc);

		//! Handler for arrival notifications from the change_transfer instance
		void notify_arrival(const std::string& key, const std::string& version);

		//! Get the attributes of a path
		int getattr(const boost::filesystem::path& path, struct stat* stat_info);

		//! Make a directory at path.
		//! \returns 0 on success, a system-like error number otherwise.
		int mkdir(const boost::filesystem::path& path, mode_t mode);

		//! Remove a directory at path
		int rmdir(const boost::filesystem::path& path);

		//! Remove a regular file
		int unlink(const boost::filesystem::path& path);

		//! Create and open a file node (mode is ignored)
		int create(const boost::filesystem::path& path, mode_t mode, struct fuse_file_info *fi);

		//! Rename a file or directory
		int rename(const boost::filesystem::path& from, const boost::filesystem::path& to);

		//! Resize a file
		int truncate(const boost::filesystem::path& path, off_t newsize);

		//! Open a file
		int open(const boost::filesystem::path& path, fuse_file_info* fi);

		//! Read from an open file
		int read(const boost::filesystem::path& path, char* buf, std::size_t size, off_t offset, fuse_file_info* fi);

		//! Write to an open file
		int write(const boost::filesystem::path& path, const char* buf, std::size_t size, off_t offset, fuse_file_info* fi);

		//! Release -- close & commit an open file
		int release(const boost::filesystem::path& path, fuse_file_info* fi);

		int readdir(const boost::filesystem::path& path, void *buf, fuse_fill_dir_t filler, off_t offset,
				struct fuse_file_info *fi);

		int flush(const boost::filesystem::path& path, struct fuse_file_info* fi);


		//! Enum signifying where the translation table is pointing (public for
		//! test reasons)
		enum redirect_to {
			dcache,	//!< Look in the dcache
			rcache	//!< Look in the readcache
		};

	protected:
		//! Helper to ensure all parent directories exist for a path.
		void make_directories(const boost::filesystem::path& path)
		{
			boost::filesystem::path progress;
			for(auto piece : path)
			{
				progress /= piece;
				if(dcache_.count(progress.string()) == 0)
				{
					//add it, it/. and ../it
					node_info piece_info{piece.string(), next_inode_++};

					//../it
					if(progress.string() != "/")
						dcache_[progress.parent_path().string()].push_back(piece_info);

					piece_info.name = ".";

					//it & it/.
					dcache_[progress.string()] = {piece_info};
				}
			}
		}

		//! Helper to clean up empty directories along a path
		void clean_directories(boost::filesystem::path path)
		{
			for(; path != "/"; path = path.parent_path())
			{
				if(dcache_.count(path.string()) == 1)
				{
					//clean up tombstones
					boost::range::remove_erase_if(
							dcache_[path.string()],
							[](const node_info& value)
							{
								return value.state == node_info::dead;
							});

					if(dcache_[path.string()].size() == 1)
					{
						//Remove the directory marker
						dcache_.erase(path.string());

						//Remove the '../dir' marker
						if(dcache_.count(path.parent_path().string()))
						{
							boost::range::remove_erase_if(
									dcache_[path.parent_path().string()],
									check_name(path.filename().string()));
						}

					}
					else //none above
						break;
				}
			}
		}

		//! Alter the given path for conflict management
		boost::filesystem::path recover_path(boost::filesystem::path path) const
		{
			return path += boost::filesystem::unique_path(".%%%%-%%%%");
		}

		//! Ensures everything is set up properly and checks if a conflict check
		//! is required.
		template <typename Rpc>
		bool conflict_check_required(const Rpc& rpc, const boost::filesystem::path& path);

		//! Handles conflict management for the sync cache
		template <typename Rpc>
		void manage_sync_cache(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& recovery);

		template <typename Rpc>
		void prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& parent, ordinary_prepare_tag);

		template <typename Rpc>
		void prepare_apply(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& parent, rename_prepare_tag);

		template <typename Rpc>
		void manage_dcache(const Rpc& rpc, const boost::filesystem::path& path,
				const boost::filesystem::path& recovery);

		template <typename Rpc>
		void manage_commit(const Rpc& rpc);

		//! Checks a path exists
		bool exists(const boost::filesystem::path& path) const;

		//! Checks if a path is in the rcache
		bool in_rcache(const boost::filesystem::path& path) const;

		//! Retrieve a file or directory (throws if it does not exist or is in
		//! rcache)
		node_info& get(const boost::filesystem::path& path);

		//! Get an rcache entry
		std::tuple<std::string, std::string> get_rcache(const boost::filesystem::path& path) const;

		//! Rename implementation
		int rename_impl(const boost::filesystem::path& from, const boost::filesystem::path& to);

		std::string get_key(const boost::filesystem::path& path) const;

		//! Handles the rename markers for tick; removes them if they're done.
		void tick_handle_rename(node_info& ni, std::deque<node_info>& cache_queue);

		//! Checks a request isn't done & handles it either way
		template <typename Rpc>
		void handle_request(const Rpc& rpc, std::deque<node_info>& cache_queue);

		void create_impl(node_info& ni, const boost::filesystem::path& path,
				struct fuse_file_info* fi);


		Client& client_;
		ChangeTx& changetx_;
		std::string id_;


		//! Functor to check for the name of a node_info
		class check_name
		{
		public:
			check_name(const std::string& name)
				:name_(name)
			{
			}

			bool operator()(const node_info& value)
			{
				return value.name == name_;
			}

		protected:
			std::string name_;
		};



		//! Directory cache. Single map of list so we can represent empty dirs
		std::unordered_map<std::string, std::list<node_info>> dcache_;

		//! Read cache. Used for storing the key & version of files opened for
		//! reading when updates came in. Cleaned up when those files are released
		std::unordered_map<std::string, std::tuple<std::string, std::string>> rcache_;

		//! The latest allocated inode number. Increases monotonically.
		uint64_t next_inode_;


		//! Translation table for fuse -> raft
		std::unordered_map<std::string, std::tuple<redirect_to, std::string>> fusetl_;

		typedef std::unordered_map<std::string, std::deque<node_info>> sync_cache_type;
		//! Holds node_info entries that need syncing.
		/*!
		 *  The name entry of these nodes is the full path, not the truncated path
		 *  provided in the dcache.
		 *
		 *  The cache is stored as an associative array of queues because only the
		 *  first item of each queue needs to be sent to Raft (the rest will be
		 *  rejected). This also makes conflict management & completion detection
		 *  easier.
		 */
		sync_cache_type sync_cache_;

		//! The UID to use for file ownership
		uid_t uid_;

		//! The GID to use for file ownership
		gid_t gid_;
	};

	template <typename Rpc>
	struct rpc_traits;


	template <>
	struct rpc_traits<raft::request::Update>
	{
		typedef raft::request::Update rpc_type;
		typedef ordinary_prepare_tag prepare_tag;

		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			return ni.version == rpc.version()
				&& ni.state == node_info::dirty;
		}

		template<typename node_info>
		static void cleanup(const rpc_type&, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type& rpc,
				const boost::filesystem::path& path, const boost::filesystem::path& parent,
				const ChangeTx& changetx)
		{
			node_info ni{path.filename().string(), rpc.version(),
				changetx.exists(rpc.key(), rpc.version())};
			if(ni.state == node_info::pending)
				ni.previous_version = rpc.old_version();
			return std::make_tuple(parent.string(), ni);
		}

	};

	template <>
	struct rpc_traits<raft::request::Delete>
	{
		typedef raft::request::Delete rpc_type;
		typedef ordinary_prepare_tag prepare_tag;

		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			return ni.version == rpc.version()
				&& ni.state == node_info::dead
				&& !ni.rename_info;
		}

		template<typename node_info>
		static void cleanup(const rpc_type&, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type&,
				const boost::filesystem::path&, const boost::filesystem::path&,
				const ChangeTx&)
		{
			return std::make_tuple("", node_info{});
		}
	};

	typedef basic_state<raft::Client, change::change_transfer<>> state;

	template <>
	struct rpc_traits<raft::request::Rename>
	{
		typedef raft::request::Rename rpc_type;
		typedef rename_prepare_tag prepare_tag;

		//! Note that this isn't sufficient: need to check the other signpost
		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			return ni.version == rpc.version()
				&& ni.state == node_info::dead
				&& static_cast<bool>(ni.rename_info)
				&& *ni.rename_info == decode_path(rpc.new_key());
		}

		template<typename node_info>
		static void cleanup(const rpc_type& rpc, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			std::string new_path = decode_path(rpc.new_key());
			//find the other end of the signpost
			if(sync_cache.count(new_path) && ni.rename_info)
			{
				//The other side may not be first
				auto other = boost::range::find_if(sync_cache.at(new_path),
						[&ni](const node_info& value)
						{
							return value.state == node_info::novel
								&& value.rename_info
								&& *value.rename_info == ni.name;
						});

				if(other != sync_cache.at(new_path).end())
				{
					if(other->rename_info &&
							*(other->rename_info) == ni.name)
						sync_cache.at(new_path).erase(other);
					else
						BOOST_LOG_TRIVIAL(warning) << "Malformed rename targets in sync cache for "
							<< ni.name << " and " << other->name;
				}
				else
					BOOST_LOG_TRIVIAL(warning)
						<< "Dangling rename pointer for path " << ni.name;
			}
			else
				BOOST_LOG_TRIVIAL(warning)
					<< "Dangling rename pointer for path " << ni.name;

			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type& rpc,
				const boost::filesystem::path&, const boost::filesystem::path&,
				const ChangeTx& changetx)
		{
			boost::filesystem::path to_path = decode_path(rpc.new_key());
			return std::make_tuple(to_path.parent_path().string(),
				node_info{to_path.filename().string(), rpc.version(),
				changetx.exists(rpc.new_key(), rpc.version())});
		}
	};

	template <>
	struct rpc_traits<raft::request::Add>
	{
		typedef raft::request::Add rpc_type;
		typedef ordinary_prepare_tag prepare_tag;

		//! Check if the given RPC matches the given node info (keys should
		//! already have been checked).
		template<typename node_info>
		static bool completed(const rpc_type& rpc, const node_info& ni)
		{
			if(ni.version == rpc.version())
			{
				//if it's marked as dirty, we'll recover but log the
				//anomaly
				if(ni.state != node_info::novel)
					BOOST_LOG_TRIVIAL(warning) << "Invalid state on completed add entry for path "
						<< ni.name << ": " << ni.state;

				return true;
			}
			return false;
		}

		template<typename node_info>
		static void cleanup(const rpc_type&, const node_info& ni,
				std::unordered_map<std::string, std::deque<node_info>>& sync_cache)
		{
			sync_cache.at(ni.name).pop_front();
			if(sync_cache.at(ni.name).empty())
				sync_cache.erase(ni.name);
		}

		template<typename node_info, typename ChangeTx>
		static std::tuple<std::string, node_info> generate(const rpc_type& rpc,
				const boost::filesystem::path& path, const boost::filesystem::path& parent,
				const ChangeTx& changetx)
		{
			return std::make_tuple(parent.string(),
					node_info{path.filename().string(), rpc.version(),
				changetx.exists(rpc.key(), rpc.version())});
		}

	};

	template <typename Rpc>
	struct log_on_missing_parent
	{
		enum {value = 1};
	};

	template <>
	struct log_on_missing_parent<raft::request::Add>
	{
		enum {value = 0};
	};

	template <typename Rpc>
	struct rpc_adds_entry
	{
		enum {value = 1};
	};

	template <>
	struct rpc_adds_entry<raft::request::Delete>
	{
		enum {value = 0};
	};
}

//template definitions
#include "fsstate.tpp"
