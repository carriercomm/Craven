#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../common/configure.hpp"
#include "configure.hpp"


DaemonConfigure::DaemonConfigure(int argc, char** argv)
 :Configure(argc, argv)
{
	cli_.add_options()
		("daemonise,d", "Fork the daemon to background.");

	all_.add_options()
		("level", "Fine-grain control of the log level; verbose overrides.")
		("log", po::value<std::string>()->default_value(LOG_LOCATION), "Path to the log file.");

	std::string usage{"Usage: "};
	usage += argv_[0];
	usage += " [options]...\n"
		"Available options";

	parse(usage);


}

bool DaemonConfigure::daemonise() const
{
	return vm_.count("daemonise");
}

fs::path DaemonConfigure::log_path() const
{
	return expand(static_cast<fs::path>(vm_["log"].as<std::string>()));
}

DaemonConfigure::loudness DaemonConfigure::output_loudness() const
{
	if(vm_.count("d"))
		return DaemonConfigure::daemon;

	if(vm_.count("q"))
		return DaemonConfigure::quiet;
	else if(vm_.count("v"))
		return DaemonConfigure::verbose;

	return DaemonConfigure::normal;
}
