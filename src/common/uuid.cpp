#include <string>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "uuid.hpp"

//! Only visible in this file
boost::uuids::random_generator uuid_gen{};

std::string uuid::gen()
{
	boost::uuids::uuid u = uuid_gen();
	return boost::uuids::to_string(u);
}
