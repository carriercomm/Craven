#pragma once

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//! Class for configuration code shared by the cli and daemon
class Configure
{
public:
	//! Constructor for the generic configuration class.
	/*!
	 * \param argc The number of arguments in argv (pass through from main).
	 * \param argv An array of c-style strings (pass through from main).
	 * \param rc_name The file name of the rc file (default "~/.<PACKAGE NAME>rc").
	 */
	Configure(int argc, char ** argv, std::string const& rc_name = RCFILE);


protected:
	//! Command-line--only options
	po::options_description cli_;

	//! Command-line and rc file options
	po::options_description all_;

	//! Path to the rc file
	std::string rc_file_;
};
