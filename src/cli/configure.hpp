#pragma once

#include <string>
#include <vector>

#include "../common/configure.hpp"

//! Class handling the configuration of the control cli.
class CtlConfigure : public Configure
{
	//! Shared init code.
	void init(const std::string& program_name);
public:
	//! Constructor for the control configuration class
	CtlConfigure(int argc, const char** argv);

	//! Constructor for use in tests
	CtlConfigure(const std::vector<std::string>& args);

	//! Used to retrieve the arguments passed to the daemon
	//! \returns A vector of strings 
	std::vector<std::string> daemon_arguments() const;

	friend std::ostream& operator<<(std::ostream& os, const CtlConfigure& conf);
};
