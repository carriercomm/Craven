#include <cstdlib>
#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "signals.hpp"
#include "configure.hpp"
#include "daemon.hpp"

int main(int argc, const char** argv)
{
	// Parse the configuration
	DaemonConfigure conf(argc, argv);

	// Run the daemon.
	Daemon daemon(conf);

	return daemon.exit_code();
}
