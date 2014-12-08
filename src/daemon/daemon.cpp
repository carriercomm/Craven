#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "configure.hpp"
#include "daemon.hpp"

Daemon::Daemon(DaemonConfigure const& config)
{

	// Double-fork to avoid zombification on parent exit
	if(config.daemonise())
		double_fork();

}

void Daemon::double_fork() const
{
	pid_t pid1, pid2;
	int status;

	//TODO (#8): handle failure cases to log.
	if(pid1 = fork()) // parent process
		exit(0);
	else if (!pid1) // child process
	{
		int sid = setsid();
		if(pid2 = fork()) // second parent
			exit(0);
		else if(!pid2)
		{
			// Change current directory
			chdir("/");

			// Reset our umask:
			umask(0);

			// Close cin, cout and cerr:
			close(0);
			close(1);
			close(2);
		}
	}

}

void Daemon::init_log() const
{

}
