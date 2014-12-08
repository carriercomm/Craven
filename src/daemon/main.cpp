#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "configure.hpp"

int main(int argc, char** argv)
{
	std::cout << "Distributed Filesystem (c) Tom Johnson 2014\n"
		<< "Daemon v0.0\n";
	DaemonConfigure conf(argc, argv);

	BOOST_LOG_TRIVIAL(warning) << "Testing 123";

	return 0;
}
