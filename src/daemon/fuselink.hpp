#pragma once

#include <mutex>
#include <future>

//! A borg containing the IO service & state instance
class fuselink
{
public:
	//! This is a borg; all members & state are static.
	fuselink() = delete;

	static boost::asio::io_service* io();

	static void io(boost::asio::io_service* io_service);

	static dfs::state* state();

	static void state(dfs::state* state);

	static std::string mount_point();
	static void mount_point(const std::string& point);

	static void shutdown();

	template <typename Callable>
	static void shutdown_handler(Callable&& f)
	{
		shutdown_ = std::forward<Callable>(f);
	}

	static void run_fuse();

protected:
	static std::mutex io_mutex_;
	static boost::asio::io_service* io_;

	static std::mutex state_mutex_;
	static dfs::state* state_;

	static std::mutex mp_mutex_;
	static std::string mount_point_;

	static std::function<void ()> shutdown_;
};


class try_call
{
	std::function<int ()> f_;
	std::string err_msg_;

	try_call(const std::function<int ()>& f, const std::string& err_msg);
public:
	try_call(const try_call&) = delete;
	try_call(try_call&&) = default;

	int operator()() const;

	static std::shared_ptr<std::packaged_task<int()>> create_packaged(const std::function<int()>& f,
			const std::string& err_msg);
};
