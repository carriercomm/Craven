
#include "configure.hpp"

CtlConfigure::CtlConfigure(int argc, char** argv)
	:Configure(argc, argv)
{
	hidden_.add_options()
		("verb", po::value<std::string>(), "The action to perform on the daemon.");

	pos_.add("verb", -1);

	std::string usage{"Usage: "};
	usage += argv_[0];
	usage += " [options]... verb\n"
		"Available options";

	parse(usage);
}
