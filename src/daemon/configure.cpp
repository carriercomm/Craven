#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../common/configure.hpp"
#include "configure.hpp"

std::map<std::string, boost::log::trivial::severity_level> DaemonConfigure::level_map =
{
	std::make_pair("trace", boost::log::trivial::trace),
	std::make_pair("debug", boost::log::trivial::debug),
	std::make_pair("info", boost::log::trivial::info),
	std::make_pair("warning", boost::log::trivial::warning),
	std::make_pair("error", boost::log::trivial::error),
	std::make_pair("fatal", boost::log::trivial::fatal),
};

void DaemonConfigure::init(const std::string& program_name)
{
	cli_.add_options()
		("daemonise,d", "Fork the daemon to background.");

	all_.add_options()
		("level", po::value<std::string>()->default_value("info"), "Fine-grain control of the log level; verbose overrides.")
		("log", po::value<std::string>()->default_value(LOG_LOCATION), "Path to the log file.");

	std::string usage{"Usage: "};
	usage += program_name;
	usage += " [options]...\n"
		"Available options";

	parse(usage);

	//Setup the loglevel:
	std::string level_string = vm_["level"].as<std::string>();
	if(level_map.count(level_string))
		log_level_ = level_map[level_string];
	else
	{
		std::cerr << "Configuration warning: log level `" << level_string
			<<"' invalid, defaulting to info.\n";
		log_level_ = boost::log::trivial::info;
	}

}


DaemonConfigure::DaemonConfigure(int argc, const char** argv)
 :Configure(argc, argv)
{
	init(argv[0]);
}

DaemonConfigure::DaemonConfigure(const std::vector<std::string>& args)
 :Configure(args)
{
	init(args[0]);
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

boost::log::trivial::severity_level DaemonConfigure::log_level() const
{
	return log_level_;
}
