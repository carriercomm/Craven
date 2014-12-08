#pragma once

//! Daemon control class
class Daemon
{
public:
	Daemon(DaemonConfigure const& config);

private:
	void double_fork() const;

	void init_log() const;
};
