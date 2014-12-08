#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "configure.hpp"
#include "daemon.hpp"

int main(int argc, char** argv)
{
	std::cout << "Distributed Filesystem (c) Tom Johnson 2014\n"
		<< "Daemon v0.0\n";
	DaemonConfigure conf(argc, argv);

	Daemon daemon(conf);

	BOOST_LOG_TRIVIAL(warning) << "Testing 123";

	return 0;
}
