#include <vector>
#include <string>

#include "../common/configure.hpp"
#include "configure.hpp"

void CtlConfigure::init(const std::string& program_name)
{
	hidden_.add_options()
		("verb", po::value<std::vector<std::string>>()->multitoken(), "The action to perform on the daemon.");

	pos_.add("verb", -1);

	std::string usage{"Usage: "};
	usage += program_name;
	usage += " [options]... verb\n"
		"Available options";

	parse(usage);
}

CtlConfigure::CtlConfigure(int argc, const char** argv)
	:Configure(argc, argv)
{
	init(argv[0]);
}

CtlConfigure::CtlConfigure(const std::vector<std::string>& args)
	:Configure(args)
{
	init(args[0]);
}

std::vector<std::string> CtlConfigure::daemon_arguments() const
{
	return vm_["verb"].as<std::vector<std::string>>();
}

std::ostream& operator<<(std::ostream& os, const CtlConfigure& conf)
{
	auto verbs = conf.daemon_arguments();
	os << "CtlConfigure={"
		<< static_cast<const Configure&>(conf)
		<< ", args=[";

	for(auto it = verbs.begin(); it != verbs.end(); ++it)
	{
		if(it != verbs.begin())
			os << ", ";

		os << *it;
	}

	os << "]}";

	return os;
}
