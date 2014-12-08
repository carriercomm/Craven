#pragma once

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;

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
	/*! This command should be run by a child class after it has set up all
	 *  options it desires.
	 *
	 *  Uses cli_ and all_ to parse argv_ (with argc_) and places the result in
	 *  vm_.
	 */
	void parse();

	//! Command-line--only options
	po::options_description cli_;

	//! Command-line and rc file options
	po::options_description all_;

	//! Results of parse
	po::variables_map vm_;

	//! Number of command-line options
	int argc_;

	//! Command-line arguments
	char ** argv_;

	//! Path to the rc file
	boost::filesystem::path rc_file_;
};
