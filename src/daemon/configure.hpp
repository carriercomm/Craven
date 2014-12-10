#pragma once

#include "../common/configure.hpp"

//!Class for the daemon's configuration.
class DaemonConfigure : public Configure
{
	static std::map<std::string, boost::log::trivial::severity_level> level_map;

	//! Initialises the configuration class
	/*!
	 *  This function is shared by both constructors to handle shared init.
	 *
	 *  \param program_name The 0th argument in the argument list, used for the
	 *  usage string.
	 */
	void init(const std::string& program_name);

public:
	//! Constructor for the daemon config class.
	/*!
	 *  \param argc passed through to the base class.
	 *  \param argv passed through to the base class.
	 */
	DaemonConfigure(int argc, const char** argv);

	//! Constructor for testing
	/*!
	 *  This constructor exists for use in testing, where the argument list has
	 *  not come from the system.
	 *
	 *  \param args A list of arguments, formatted like argv would be.
	 */
	DaemonConfigure(const std::vector<std::string> & args);

	//! Returns true if a daemon has been requested.
	bool daemonise() const;

	boost::filesystem::path log_path() const;

	//! Enum specifying how much output goes to stdout.
	enum loudness {
		normal, //!< normal: warning and above
		quiet, //!< quiet: critical and above
		verbose, //!< verbose: info and above
		daemon, //!< nothing -- we're a daemon, so we don't have a console.
	};

	//! Get the desired loudness on stderr.
	loudness output_loudness() const;

	//! Returns the desired log level for the file log.
	boost::log::trivial::severity_level log_level() const;

protected:
	boost::log::trivial::severity_level log_level_;
};
