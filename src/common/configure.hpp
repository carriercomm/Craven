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
	 */
	Configure(int argc, char ** argv);


protected:
	//! Parse command for the use of the classes.
	/*! This command should be run by a child class after it's set up all
	 *  options it desires.
	 */
	void parse();

	//! Command-line--only options
	po::options_description cli_;

	//! Command-line and rc file options
	po::options_description all_;

	//! Number of command-line options
	int argc_;

	//! Command-line arguments
	char ** argv_;

	//! Path to the rc file
	std::string rc_file_;
};
