#include <iostream>
#include <string>
#include <vector>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif //HAVE_CONFIG_H

#include "../common/configure.hpp"
#include "configure.hpp"

int main(int argc, const char** argv)
{
	CtlConfigure conf(argc, argv);

	if(conf.version_requested())
		std::cout << "Distributed Filesystem (c) Tom Johnson 2014\n"
			<< "Project v" << VERSION
			<< " Control client v0.0\n";


	return 0;
}
