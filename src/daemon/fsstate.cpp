#include <cstdint>

#include <string>
#include <iomanip>
#include <sstream>

#include <stdexcept>

#include "raftrequest.hpp"
#include "fsstate.hpp"


std::string craven::encode_path(const std::string& path)
{
	std::ostringstream encoded;

	//Set up format
	encoded.fill('0');
	encoded << std::hex;

	for(const char& x : path)
	{
		if(std::isalnum(x) || x == '-' || x == '_' || x == '.')
			encoded.put(x);
		else
			encoded << '%' << std::setw(2) << static_cast<uint32_t>(x);
	}

	return encoded.str();
}

std::string craven::decode_path(const std::string& path)
{
	std::ostringstream decoded;
	std::istringstream encoded(path);

	for(char x = encoded.get(); !encoded.eof(); x = encoded.get())
	{

		if(x == '%')
		{
			std::string num;
			for(unsigned i = 0; i < 2; ++i)
			{
				num += encoded.get();
				if(encoded.eof())
					throw std::runtime_error("Bad coding for string " + path);
			}

			decoded.put(static_cast<unsigned char>(std::stoi(num, 0, 16)));
		}
		else
			decoded.put(x);
	}

	return decoded.str();
}
