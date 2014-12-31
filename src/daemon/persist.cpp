#include <string>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

namespace fs = boost::filesystem;

#include "persist.hpp"

namespace change
{
	persistence::persistence(const fs::path& root)
		:root_(root)
	{
		if(fs::exists(root))
		{
			if(!fs::is_directory(root))
				throw std::logic_error("Persistence root is not a directory");

			BOOST_LOG_TRIVIAL(info) << "Recovering persistence from " << root;
			fs::directory_iterator end;
			for(fs::directory_iterator it(root); it != end; ++it)
			{
				if(fs::is_directory(it->status()))
				{
					std::string key = it->path().filename().string();
					for(fs::directory_iterator key_it(it->path()); key_it != end; ++key_it)
					{
						if(fs::is_regular_file(key_it->status()))
							versions_.insert(std::make_pair(key, key_it->path().filename().string()));
						else
							BOOST_LOG_TRIVIAL(warning) << "Version " << key_it->path().filename() << " is not a regular file; skipping.";

					}
				}
				else
					BOOST_LOG_TRIVIAL(warning) << "Non-directory entry in persistence root: " << it->path().filename();
			}
		}
		else
		{
			BOOST_LOG_TRIVIAL(info) << "Persistence root being created at: " << root;
			fs::create_directories(root);
		}

	}

	bool persistence::exists(const std::string& key) const
	{
		return versions_.count(key) > 0;
	}

	bool persistence::exists(const std::string& key, const std::string& version) const
	{
		auto range = versions_.equal_range(key);
		for(auto it = range.first; it != range.second; ++it)
		{
			if(it->second == version)
				return true;
		}
		return false;
	}

	boost::filesystem::path persistence::operator()(const std::string& key, const std::string& version) const
	{
		if(!exists(key, version))
			throw std::logic_error(boost::str(boost::format(
					"Key, version combo \"%|s|, %|s|\" does not exist.") % key % version));

		return root_ / key / version;
	}

	boost::filesystem::path persistence::add(const std::string& key, const std::string& version)
	{
		if(exists(key, version))
			throw std::logic_error(boost::str(boost::format(
					"Key, version combo \"%|s|, %|s|\" already exists.") % key % version));

		versions_.insert(std::make_pair(key, version));

		if(!fs::exists(root_ / key))
			fs::create_directory(root_ / key);

		return root_ / key / version;
	}

}
