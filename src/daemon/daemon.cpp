#include <cstdlib>

#include <iostream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

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

	init_log(config.log_path(), config.output_loudness(), logging::trivial::debug);
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

void Daemon::init_log(fs::path const& log_path, DaemonConfigure::loudness stderr_level, logging::trivial::severity_level level) const
{
	//Add attributes

	//Add LineID, TimeStamp, ProcessID and ThreadID.
	logging::add_common_attributes;


	if(stderr_level != DaemonConfigure::daemon)
	{
		logging::trivial::severity_level stderr_severity;
		switch(stderr_level)
		{
		case DaemonConfigure::quiet:
			stderr_severity = logging::trivial::fatal;
			break;

		case DaemonConfigure::verbose:
			stderr_severity = logging::trivial::info;
			break;

		case DaemonConfigure::normal:
		default:
			stderr_severity = logging::trivial::warning;
		}

		logging::add_console_log(std::cerr)->set_filter(logging::trivial::severity >= stderr_severity);
	}

	logging::add_file_log(logging::keywords::file_name = log_path.string(),
			logging::keywords::open_mode = std::ios::app)
		->set_filter(logging::trivial::severity >= level);


	BOOST_LOG_TRIVIAL(info) << "Logging to " << log_path;
}
