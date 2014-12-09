#pragma once

#include "../common/configure.hpp"

//! Class handling the configuration of the control cli.
class CtlConfigure : public Configure
{
public:
	//! Constructor for the control configuration class
	CtlConfigure(int argc, const char** argv);
};
