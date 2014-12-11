#include <cstdlib>

#include <iostream>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace fs = boost::filesystem;

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif //HAVE_CONFIG_H

#include "configure.hpp"
#include "daemon.hpp"

Daemon::Daemon(DaemonConfigure const& config)
{
	if(config.version_requested())
		std::cout << "Distributed Filesystem (c) Tom Johnson 2014\n"
			<< "Project v" << VERSION
			<< " Daemon v0.0\n";
	else
	{
		// Double-fork to avoid zombification on parent exit
		if(config.daemonise())
			double_fork();

		//Setup our logs
		init_log(config.log_path(), config.output_loudness(), config.log_level());
	}
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
	logging::add_common_attributes();

	//Formatter
	auto formatter = expr::stream
		<< "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%dT%H:%M:%S%q")
		<< "] {" << expr::attr<logging::attributes::current_thread_id::value_type>("ThreadID")
		<< "} (" << logging::trivial::severity
		<< "): " << expr::message;


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

		logging::add_console_log(std::cerr, logging::keywords::format = formatter)->set_filter(logging::trivial::severity >= stderr_severity);
	}

	logging::add_file_log(logging::keywords::file_name = log_path.string(),
			logging::keywords::open_mode = std::ios::app,
			logging::keywords::format = formatter)
		->set_filter(logging::trivial::severity >= level);


	BOOST_LOG_TRIVIAL(info) << "Log start.";
	BOOST_LOG_TRIVIAL(info) << "Logging to " << log_path;
}
