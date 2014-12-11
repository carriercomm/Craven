#pragma once

//! Daemon control class
/*!
 * This class handles the control of the daemon; it's responsible for setting
 * exit codes, providing system signal handlers, deciding if a quit is required
 * and owning the asio io_service.
 */
class Daemon
{
public:
	//! Construct the Daemon
	//! \param config The configuration for this daemon
	Daemon(DaemonConfigure const& config);

private:
	//! Executes a double-fork to escape a shell.
	void double_fork() const;

	//! Initialises logging.
	/*!
	 *	This function sets up logging for the daemon -- it creates a log file
	 *	and also a logger to stderr, using the different loudnesses requested.
	 *
	 *  \param log_path The path to the log file.
	 *  \param stderr_loud How much to log to stderr
	 *  \param level How much to log to the log file
	 */
	void init_log(boost::filesystem::path const& log_path, DaemonConfigure::loudness
			stderr_loud, boost::log::trivial::severity_level level) const;

	//! The asio io_service for the daemon.
	boost::asio::io_service io_;
};
