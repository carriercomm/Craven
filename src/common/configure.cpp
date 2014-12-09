#include <wordexp.h>

#include <string>
#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "configure.hpp"

std::runtime_error expansion::factory(int err)
{
	switch (err)
	{
	case WRDE_BADCHAR:
		return badchar();
		break;

	case WRDE_BADVAL:
		return badval();
		break;

	case WRDE_CMDSUB:
		return cmdsub();
		break;

	case WRDE_NOSPACE:
		return nospace();
		break;

	case WRDE_SYNTAX:
		return syntax();
		break;

	default:
		return expansion_error("Unknown expansion error");
	}

}

expansion::expansion_error::expansion_error(const std::string& what_arg)
	:std::runtime_error(what_arg){}

expansion::expansion_error::expansion_error(const char* what_arg)
	:std::runtime_error(what_arg){}

expansion::badchar::badchar()
	:expansion::expansion_error("Illegal occurence of newline or one of `|&;<>(){}' in expansion."){}

expansion::badval::badval()
	:expansion::expansion_error("Undefined shell variable referenced in expansion."){}

expansion::cmdsub::cmdsub()
	:expansion::expansion_error("Illegal command substitution occurred in expansion."){}

expansion::nospace::nospace()
	:expansion::expansion_error("Out of memory error occured during expansion."){}

expansion::syntax::syntax()
	:expansion::expansion_error("Syntax error in expansion."){}

expansion::noexpand::noexpand()
	:expansion::expansion_error("No expansions produced."){}

invalid_config::invalid_config(const std::string& msg)
	:std::runtime_error("Fatal configuration error: " + msg)
{
}

Configure::Configure(int argc, char ** argv)
	:cli_("CLI-only"),
	all_("CLI and rc file"),
	argc_(argc),
	argv_(argv)
{
	cli_.add_options()
		("version", "Print the version")
		("help,h", "Print the help message")
		("conf,c", po::value<std::string>()->default_value(RCFILE),
			"Use <file> instead of the default(" RCFILE ")")
		(",q", "Be quiet")
		(",v", "Be verbose");


	all_.add_options()
		("socket,s", po::value<std::string>()->default_value(COMMS_SOCKET), "Location of the control socket");

}

bool Configure::version_requested() const
{
	return vm_.count("version");
}

void Configure::parse(const std::string& usage)
{
	po::options_description cmd_line;
	cmd_line.add(cli_).add(all_).add(hidden_);

	po::options_description visible(usage);
	visible.add(cli_).add(all_);


	//Parse the command line
	po::store(po::command_line_parser(argc_, argv_).
			options(cmd_line).positional(pos_).run(), vm_);

	rc_file_ = static_cast<fs::path>(vm_["conf"].as<std::string>());
	rc_file_ = expand(rc_file_);

	//If the file doesn't exist, output a warning and move on
	if(fs::exists(rc_file_))
	{
		std::ifstream rc_file(rc_file_.string());

		po::store(po::parse_config_file(rc_file, all_, true), vm_);
	}
	else if(vm_.count("q") == 0)
		std::cerr << "Warning: specified rc file does not exist: " << rc_file_ << "\n";

	if(vm_.count("help"))
		std::cout << visible << "\n";

	po::notify(vm_);
}

boost::filesystem::path Configure::expand(boost::filesystem::path const& path) const
{
	wordexp_t p;
	int err = wordexp(path.c_str(), &p, 0);
	if(err != 0)
		throw expansion::factory(err); //throw the correct error

	if(p.we_wordc == 0)
		throw expansion::noexpand();

	fs::path expanded_path{p.we_wordv[0]};

	wordfree(&p);

	return expanded_path;
}
