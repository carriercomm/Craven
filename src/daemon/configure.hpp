#pragma once

#include "../common/configure.hpp"

//!Class for the daemon's configuration.
class DaemonConfigure : public Configure
{
public:
	//!Constructor for the daemon config class.
	/*!
	 * \param argc passed through to the base class.
	 * \param argv passed through to the base class.
	 */
	DaemonConfigure(int argc, char** argv);

};
