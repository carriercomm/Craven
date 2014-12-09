#pragma once

//! Daemon control class
class Daemon
{
public:
	Daemon(DaemonConfigure const& config);

private:
	void double_fork() const;

	void init_log(boost::filesystem::path const& log_path, DaemonConfigure::loudness
			stderr_loud, boost::log::trivial::severity_level level) const;
};
