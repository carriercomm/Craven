#include <string>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "configure.hpp"

Configure::Configure(int argc, char ** argv, std::string const& rc_name)
:cli_("CLI-only"),
 all_("CLI and rc file"),
 rc_file_(rc_name)
{
	cli_.add_options()
		("version", "Print the version")
		("help,h", "Print the help message")
		("conf,c", "Use <file> instead of the default(" $RCFILE ")")
		("v", "Be verbose")

}

