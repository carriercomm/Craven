#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#define RCFILE ("~/." PACKAGE_NAME "rc")
#endif

class Configure
{
public:
	Configure(int argc, char ** argv, std::string const& rc_name = RCFILE);
};
