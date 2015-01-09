#include <cstring>

#include <future>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <linux/limits.h> // for PATH_MAX

#include <boost/asio.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/filesystem.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>

#include "raftrequest.hpp"
#include "raftclient.hpp"
#include "changetx.hpp"
#include "fsstate.hpp"

#include "fuselink.hpp"


std::mutex fuselink::io_mutex_{};
boost::asio::io_service* fuselink::io_{};

std::mutex fuselink::state_mutex_{};
dfs::state* fuselink::state_{};

std::mutex fuselink::mp_mutex_{};
std::string fuselink::mount_point_{};



boost::asio::io_service* fuselink::io()
{
	std::lock_guard<std::mutex> guard(io_mutex_);
	return io_;
}

void fuselink::io(boost::asio::io_service* io_service)
{
	std::lock_guard<std::mutex> guard(io_mutex_);
	io_ = io_service;
}

dfs::state* fuselink::state()
{
	std::lock_guard<std::mutex> guard(state_mutex_);
	return state_;
}

void fuselink::state(dfs::state* state)
{
	std::lock_guard<std::mutex> guard(state_mutex_);
	state_ = state;
}

std::string fuselink::mount_point()
{
	std::lock_guard<std::mutex> guard(mp_mutex_);
	return mount_point_;
}

void fuselink::mount_point(const std::string& point)
{
	std::lock_guard<std::mutex> guard(mp_mutex_);
	mount_point_ = point;
}

try_call::try_call(const std::function<int()>& f, const std::string& err_msg)
	:f_(f),
	err_msg_(err_msg)
{
}

int try_call::operator()() const
{
	try
	{
		return f_();
	}
	catch(std::exception& ex)
	{
		BOOST_LOG_TRIVIAL(error) << err_msg_ << ex.what();
		return -EIO;
	}
}

std::shared_ptr<std::packaged_task<int()>> try_call::create_packaged(const std::function<int()>& f,
		const std::string& err_msg)
{
	return std::make_shared<std::packaged_task<int()>>(try_call{f, err_msg});
}

namespace fuse_handlers
{

	//The fuse handlers need to be free functions
	int getattr(const char* path, struct stat* stat_info)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::getattr,
					fuselink::state(),
					path,
					stat_info), "Error in getattr: ");

		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		//waits for the result
		return result.get();
	}

	int mkdir(const char* path, mode_t mode)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::mkdir,
					fuselink::state(),
					path, mode), "Error in mkdir: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Remove a directory at path
	int rmdir(const char* path)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::rmdir,
					fuselink::state(),
					path), "Error in rmdir: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Remove a regular file
	int unlink(const char* path)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::unlink,
					fuselink::state(),
					path), "Error in unlink: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Create and open a file node (mode is ignored)
	int create(const char* path, mode_t mode, struct fuse_file_info *fi)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::create,
					fuselink::state(),
					path,
					mode,
					fi), "Error in create: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Rename a file or directory
	int rename(const char* from, const char* to)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::rename,
					fuselink::state(),
					from,
					to), "Error in rename: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Resize a file
	int truncate(const char* path, off_t newsize)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::truncate,
					fuselink::state(),
					path,
					newsize), "Error in truncate: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Open a file
	int open(const char* path, fuse_file_info* fi)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::open,
					fuselink::state(),
					path,
					fi), "Error in open: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Read from an open file
	int read(const char* path, char* buf, std::size_t size, off_t offset, fuse_file_info* fi)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::read,
					fuselink::state(),
					path, buf, size, offset, fi), "Error in read: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Write to an open file
	int write(const char* path, const char* buf, std::size_t size, off_t offset, fuse_file_info* fi)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::write,
					fuselink::state(),
					path, buf, size, offset, fi), "Error in write: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	//! Release -- close & commit an open file
	int release(const char* path, fuse_file_info* fi)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::release,
					fuselink::state(),
					path, fi), "Error in release: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}

	int readdir(const char* path, void *buf, fuse_fill_dir_t filler, off_t offset,
			struct fuse_file_info *fi)
	{
		auto task = try_call::create_packaged(std::bind(&dfs::state::readdir,
					fuselink::state(),
					path, buf, filler, offset, fi), "Error in readdir: ");
		auto result = task->get_future();
		fuselink::io()->post(std::bind(&std::packaged_task<int()>::operator(), task));

		return result.get();
	}
}


void fuselink::run_fuse()
{
	fuse_operations fuse_ops;
	memset(&fuse_ops, 0, sizeof(fuse_ops));
	//Supported ops
	fuse_ops.getattr = fuse_handlers::getattr;
	fuse_ops.mkdir = fuse_handlers::mkdir;
	fuse_ops.rmdir = fuse_handlers::rmdir;
	fuse_ops.unlink = fuse_handlers::unlink;
	fuse_ops.create = fuse_handlers::create;
	fuse_ops.rename = fuse_handlers::rename;
	fuse_ops.truncate = fuse_handlers::truncate;
	fuse_ops.open = fuse_handlers::open;
	fuse_ops.read = fuse_handlers::read;
	fuse_ops.write = fuse_handlers::write;
	fuse_ops.release = fuse_handlers::release;
	fuse_ops.readdir = fuse_handlers::readdir;

	fuse_ops.flag_nullpath_ok = 0;

	std::string mp = mount_point();
	if(mp.size() > PATH_MAX)
		throw std::length_error("Mount point is longer than PATH_MAX");

	char* args[4];
	char arg0[3];
	char arg1[3];
	char arg2[3];
	char arg3[PATH_MAX];

	std::strcpy(arg0, "-s"); //single thread
	std::strcpy(arg1, "-f"); //foreground
	std::strcpy(arg2, "-d"); //debug
	std::strcpy(arg3, mp.c_str());

	args[0] = arg0;
	args[1] = arg1;
	args[2] = arg2;
	args[3] = arg3;

	fuse_main(4, args, &fuse_ops, nullptr);
}
