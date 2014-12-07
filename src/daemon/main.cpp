#include <iostream>

#include "configure.hpp"

int main(int argc, char** argv)
{
	std::cout << "Distributed Filesystem (c) Tom Johnson 2014\n"
		<< "Daemon v0.0\n";
	DaemonConfigure conf(argc, argv);

	return 0;
}
