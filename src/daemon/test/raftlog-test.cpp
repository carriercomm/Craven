#define BOOST_TEST_MODULE "Raft write-ahead logging tests"
#include <boost/test/unit_test.hpp>

#include <cstdint>

#include <string>
#include <exception>
#include <fstream>
#include <iostream>

#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>

#include <json/json.h>

#include "../raftlog.hpp"

BOOST_AUTO_TEST_CASE(test_test_case)
{

	BOOST_REQUIRE(false);
}
