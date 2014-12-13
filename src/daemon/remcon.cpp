#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>

namespace fs = boost::filesystem;

#include "../common/connection.hpp"
#include "remcon.hpp"

RemoteControl::RemoteControl(boost::asio::io_service& io, const fs::path& socket)
	:io_(io)
{

}
