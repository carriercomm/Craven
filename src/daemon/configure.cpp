#include "../common/configure.hpp"
#include "configure.hpp"


DaemonConfigure::DaemonConfigure(int argc, char** argv)
 :Configure(argc, argv)
{
	cli_.add_options()
		("daemonise,d", "Fork the daemon to background.");

	all_.add_options()
		("level", "Fine-grain control of the log level; verbose overrides.")
		("log", "Path to the log file.");

	parse();


}

bool DaemonConfigure::daemonise()
{
	return vm_.count("daemonise");
}

