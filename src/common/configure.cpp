#include <string>
#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "configure.hpp"

Configure::Configure(int argc, char ** argv)
	:cli_("CLI-only"),
	all_("CLI and rc file"),
	argc_(argc),
	argv_(argv),
	rc_file_("")
{
	cli_.add_options()
		("version", "Print the version")
		("help,h", "Print the help message")
		("conf,c", po::value<std::string>()->default_value(RCFILE),
			"Use <file> instead of the default(" RCFILE ")")
		("v", "Be verbose");


	all_.add_options()
		("socket,s", po::value<std::string>()->default_value(COMMS_SOCKET), "Location of the control socket");

}

void Configure::parse()
{
	po::options_description cmd_line;
	cmd_line.add(cli_).add(all_);


	po::variables_map vm;
	po::store(po::parse_command_line(argc_, argv_, cmd_line), vm);

	rc_file_ = vm["conf"].as<std::string>();

	//If the file doesn't exist, output a warning and move on
	//TODO (#7): add warning once logging's in
	std::ifstream rc_file(rc_file_);

	po::store(po::parse_config_file(rc_file, all_), vm);

	if(vm.count("help"))
		std::cout << cmd_line << "\n";

	po::notify(vm);
}
