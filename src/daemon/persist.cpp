#include <string>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

#include "persist.hpp"

namespace change
{
	persistence::persistence(const fs::path& root)
		:root_(root)
	{
		throw std::runtime_error("Not yet implemented");
	}

	bool persistence::exists(const std::string& key) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	bool persistence::exists(const std::string& key, const std::string& version) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	boost::filesystem::path persistence::operator()(const std::string& key, const std::string& version) const
	{
		throw std::runtime_error("Not yet implemented");
	}

	boost::filesystem::path persistence::add(const std::string& key, const std::string& version)
	{
		throw std::runtime_error("Not yet implemented");
	}


}
