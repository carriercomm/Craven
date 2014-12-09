#include <iostream>

#include "configure.hpp"

int main(int argc, char** argv)
{
	std::cout << "Distributed Filesystem (c) Tom Johnson 2014\n"
		<< "Control client v0.0\n";

	CtlConfigure conf(argc, argv);

	return 0;
}
