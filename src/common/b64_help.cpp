#include <sstream>

#include <boost/algorithm/string/erase.hpp>

#include <b64/encode.h>
#include <b64/decode.h>

#include "b64_help.hpp"

std::string b64_help::encode(std::istream& is)
{
	base64::encoder enc;
	std::ostringstream os;
	enc.encode(is, os);

	std::string encoded = os.str();

	//TODO: ewww
	boost::erase_all(encoded, "\n");

	return encoded;
}

void b64_help::decode(std::string data, std::ostream& os)
{
	base64::decoder dec;
	std::istringstream is(data);

	dec.decode(is, os);
}
