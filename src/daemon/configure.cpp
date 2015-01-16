#include <iostream>
#include <unordered_map>

#include <unistd.h>
#include <sys/types.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

#include <json/json.h>
#include <json_help.hpp>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../common/configure.hpp"
#include "configure.hpp"

std::map<std::string, boost::log::trivial::severity_level> DaemonConfigure::level_map =
{
	{"trace", boost::log::trivial::trace},
	{"debug", boost::log::trivial::debug},
	{"info", boost::log::trivial::info},
	{"warning", boost::log::trivial::warning},
	{"error", boost::log::trivial::error},
	{"fatal", boost::log::trivial::fatal},
};

void DaemonConfigure::init(const std::string& program_name)
{
	cli_.add_options()
		("daemonise,d", "Fork the daemon to background.");

	all_.add_options()
		("level", po::value<std::string>()->default_value("info"), "Fine-grain control of the log level; verbose overrides.")
		("log", po::value<std::string>()->default_value(LOG_LOCATION), "Path to the log file.")
		("working_directory", po::value<std::string>()->default_value("~/." PACKAGE_NAME), "Working directory.")
		("id", po::value<std::string>(), "The ID of this node")
		("nodes", po::value<std::string>(), "JSON node info");

	hidden_.add_options()
		("fuse_mount", po::value<std::string>()->required(), "The mount point");

	pos_.add("fuse_mount", 1);

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

	id_ = vm_["id"].as<std::string>();

	auto nodes = json_help::parse(vm_["nodes"].as<std::string>());

	for(unsigned i = 0; i < nodes.size(); ++i)
	{
		if(nodes[i]["id"].asString() == id_)
			port_ = nodes[i]["port"].asString();
		else
			nodes_[nodes[i]["id"].asString()] = std::make_tuple(
					nodes[i]["host"].asString(),
					nodes[i]["port"].asString());
	}

	working_root_ = boost::filesystem::absolute(expand(vm_["working_directory"].
			as<std::string>()));
	fuse_mount_ = vm_["fuse_mount"].as<std::string>();
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
	if(vm_.count("daemonise"))
		return DaemonConfigure::daemon;

	if(vm_.count("quiet"))
		return DaemonConfigure::quiet;
	else if(vm_.count("verbose"))
		return DaemonConfigure::verbose;

	return DaemonConfigure::normal;
}

boost::log::trivial::severity_level DaemonConfigure::log_level() const
{
	return log_level_;
}

std::string DaemonConfigure::id() const
{
	return id_;
}

boost::asio::ip::tcp::endpoint DaemonConfigure::listen() const
{
	return boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(),
		static_cast<short unsigned int>(std::stoi(port_))};
}

std::unordered_map<std::string, std::tuple<std::string, std::string>> DaemonConfigure::node_info() const
{
	return nodes_;
}

std::tuple<uint32_t, uint32_t, uint32_t> DaemonConfigure::raft_timer() const
{
	return std::make_tuple(150, 300, 50);
}

std::vector<std::string> DaemonConfigure::node_list() const
{
	std::vector<std::string> ret;
	ret.reserve(nodes_.size());
	boost::push_back(ret,
			nodes_ | boost::adaptors::transformed(
				[](const std::pair<std::string, std::tuple<std::string, std::string>>& value)
				{
					return std::get<0>(value);
				}));
	return ret;
}

boost::filesystem::path DaemonConfigure::raft_log() const
{
	return working_root_ / "raftlog";
}

boost::filesystem::path DaemonConfigure::persistence_root() const
{
	return working_root_ / "persistence";
}

uid_t DaemonConfigure::fuse_uid() const
{
	return getuid();
}

gid_t DaemonConfigure::fuse_gid() const
{
	return getgid();
}

boost::filesystem::path DaemonConfigure::fuse_mount() const
{
	return fuse_mount_;
}

uint32_t DaemonConfigure::tick_timeout() const
{
	return 500;
}
