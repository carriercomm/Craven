#pragma once

#include <unordered_map>
#include <list>

#include <boost/optional.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>


#define FUSE_USE_VERSION 26
#include <fuse.h>


namespace dfs
{
	std::string encode_path(const std::string& path);
	std::string decode_path(const std::string& path);

	template <typename Client, typename ChangeTx>
	class basic_state
	{
		basic_state(Client& client, ChangeTx& changetx)
			:client_(client),
			changetx_(changetx)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Handler for commit notification from the raft client.
		/*!
		 *  This function handles an incoming committed change to the filesystem.
		 *  It first checks to see if this change is one we've requested (if it
		 *  is, it marks those changes as synced), then checks to see if the
		 *  change conflicts before caching the information it provides & handling
		 *  any conflicts it causes.
		 */
		void commit_update(const raft::request::Update& rpc)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! \overload
		void commit_delete(const raft::request::Delete& rpc)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! \overload
		void commit_rename(const raft::request::Rename& rpc)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! \overload
		void commit_add(const raft::request::Add& rpc)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Handler for arrival notifications from the change_transfer instance
		void notify_arrival(const std::string& key, const std::string& version)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Get the attributes of a path
		int getattr(const std::string& path, struct stat* stat_info)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Make a directory at path.
		//! \returns 0 on success, a system-like error number otherwise.
		int mkdir(const std::string& path)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Remove a directory at path
		int rmdir(const std::string& path)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Remove a regular file
		int unlink(const std::string& path)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Rename a file or directory
		int rename(const std::string& from, const std::string& to)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Resize a file
		int truncate(const std::string& path, off_t newsize)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Open a file
		int open(const std::string& path, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Read from an open file
		int read(const std::string& path, char* buf, std::size_t size, off_t offset, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Write to an open file
		int write(const std::string& path, char* buf, std::size_t size, off_t offset, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}

		//! Release -- close & commit an open file
		int release(const std::string& path, fuse_file_info* fi)
		{
			throw std::runtime_error("Not yet implemented");
		}


	protected:
		Client client_;
		ChangeTx changetx_;

		//! Node information stored in the dcache
		struct node_info
		{
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
				active,	//!< The node is actively being written to.
				active_read,	//!< The node is actively being read from (i.e. read-only access).
				novel,	//!< The node is new.
				dead	//!< The node has been deleted but the delete is not yet synced
			};

			node_type type;
			state_type state;

			//! Inode number (must be unique)
			uint64_t inode;

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

			//! Previous version, if we're pending or dirty
			boost::optional<std::string> previous_version;

			//! The name of the node
			/*!
			 *  In the dcache, this is the filename and the full path is implied.
			 *  Elsewhere, this is the full name.
			 */
			std::string name;

			//! The version of the node. Empty for novel files & directories.
			std::string version;

			//! The key of the node (what goes in raft & changetx)
			std::string key;
		};

		//! Directory cache. Single map of list so we can represent empty dirs
		std::unordered_map<std::string, std::list<node_info>> dcache_;

		//! Read cache. Used for storing the key & version of files opened for
		//! reading when updates came in. Cleaned up when those files are released
		std::unordered_map<std::string, std::tuple<std::string, std::string>> rcache_;

		//! The latest allocated inode number. Increases monotonically.
		uint64_t latest_inode_;

		//! Enum signifying where the translation table is pointing
		enum redirect_to {
			dcache,	//!< Look in the dcache
			rcache	//!< Look in the readcache
		};

		//! Translation table for fuse -> raft
		std::unordered_map<std::string, std::tuple<redirect_to, std::string>> fusetl_;

		//! Holds node_info entries that need syncing.
		/*!
		 *  The name entry of these nodes is the full path, not the truncated path
		 *  provided in the dcache.
		 */
		std::vector<node_info> sync_cache_;
	};
}
