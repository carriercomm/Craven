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
};
