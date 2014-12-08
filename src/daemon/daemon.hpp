#pragma once

//! Daemon control class
class Daemon
{
public:
	Daemon(DaemonConfigure const& config);

private:
	void double_fork() const;

	void init_log(boost::filesystem::path const& log_path,
			logging::trivial::severity_level level) const;
};
