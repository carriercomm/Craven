#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>

#include <boost/filesystem.hpp>

namespace logging = boost::log;
namespace fs = boost::filesystem;

#include "configure.hpp"
#include "daemon.hpp"

Daemon::Daemon(DaemonConfigure const& config)
{

	// Double-fork to avoid zombification on parent exit
	if(config.daemonise())
		double_fork();

	init_log(config.log_path(), logging::trivial::debug);
}

void Daemon::double_fork() const
{
	pid_t pid1, pid2;
	int status;

	if(pid1 = fork()) // parent process
		exit(0);
	else if (!pid1) // child process
	{
		int sid = setsid();
		if(pid2 = fork()) // second parent
			exit(0);
		else if(!pid2)
		{
			// Change current directory
			chdir("/");

			// Reset our umask:
			umask(0);

			// Close stdin, stdout and stderr:
			close(0);
			close(1);
			close(2);
		}
		else
			BOOST_LOG_TRIVIAL(warning) << "Second fork of daemonise failed. Continuing...";
	}
	else
		BOOST_LOG_TRIVIAL(warning) << "First fork of daemonise failed. Continuing...";
}

void Daemon::init_log(fs::path const& log_path, logging::trivial::severity_level level) const
{
	BOOST_LOG_TRIVIAL(info) << "Logging to " << log_path;
	logging::add_file_log(logging::keywords::file_name = log_path.string(), logging::keywords::open_mode = std::ios::app);

	logging::core::get()->set_filter(logging::trivial::severity >= level);

}
