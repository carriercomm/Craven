#define BOOST_TEST_MODULE "RemoteControl test"
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <functional>
#include <deque>

#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>

namespace fs = boost::filesystem;

#include "../remcon.hpp"

