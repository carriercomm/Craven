#pragma once

#include <boost/asio/io_service.hpp>

#include <json/json.h>

#include "dispatch.hpp"
#include "raftrpc.hpp"
#include "raftctl.hpp"
#include "changetx.hpp"
#include "fsstate.hpp"
#include "comms_man.hpp"
#include "remcon.hpp"

//! Daemon control class
/*!
 * This class handles the control of the daemon; it's responsible for setting
 * exit codes, providing system signal handlers, deciding if a quit is required
 * and owning the asio io_service.
 */
class Daemon
{
	//! Ctor helper
	raft::Controller::TimerLength timer(const std::tuple<uint32_t, uint32_t, uint32_t>& lengths) const;
public:
	//! Construct the Daemon
	//! \param config The configuration for this daemon
	Daemon(DaemonConfigure const& config);

	//! Enum providing numeric exit codes and for the storage of daemon state.
	enum daemon_state {
		exit = 0, //!< Used to indicate a clean exit
		error = 1, //!< Used to indicate an error has occurred.
		running = 2, //!< The daemon is running; if this is returned there's been a critical error.
	};

	//! Retrieve the exit code
	int exit_code() const;

	typedef TopLevelDispatch<TCPConnectionPool> dispatch_type;

protected:
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

	void start_ctx_timer(uint32_t tick_timeout);
	void start_fst_timer(uint32_t tick_timeout);

	//! The asio io_service for the daemon.
	boost::asio::io_service io_;
	std::string id_;

	//! Timer for changetx
	boost::asio::deadline_timer ctx_tick_;

	//! Timer for fsstate
	boost::asio::deadline_timer fst_tick_;

	RemoteControl remcon_;
	TCPConnectionPool pool_;
	dispatch_type dispatch_;
	comms_man comms_;
	raft::Controller raft_;
	change::change_transfer<> changetx_;
	std::function<void(const std::string&, const std::string&,
			const Json::Value&)> changetx_send_;

	dfs::state fsstate_;

	//! The state of the daemon
	daemon_state state_ = running;
};
