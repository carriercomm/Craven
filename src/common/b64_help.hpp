#pragma once

namespace b64_help
{
	std::string encode(std::istream& is);
	void decode(std::string data, std::ostream& os);
}
