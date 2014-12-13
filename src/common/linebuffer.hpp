#pragma once

namespace util
{
	template <class Buffer>
	class line_buffer
	{
	public:
		typedef Buffer buffer_type;
		typedef std::basic_string<typename buffer_type::value_type> string_type;
		typedef std::basic_stringstream<typename buffer_type::value_type> stringstream_type;
		typedef std::basic_streambuf<typename stringstream_type::char_type,
				typename stringstream_type::traits_type> streambuf_type;

		line_buffer() = default;

		line_buffer(const string_type& buf)
			:remainder_(buf)
		{}

		std::vector<string_type> operator()(const buffer_type& buf, typename buffer_type::size_type size)
		{
			/* For it to read the initialised data, we need to seek to the end
			 *-- the ios_base::ate flag. The other two are there for a normal open.
			 */
			stringstream_type ss(remainder_, std::ios_base::in | std::ios_base::out | std::ios_base::ate);
			remainder_.clear();

			//Read from the buffer
			ss.write(buf.data(), size);

			std::vector<string_type> lines;

			std::string line;
			while(std::getline(ss, line))
			{
				ss.unget();
				if('\n' == ss.get())
					lines.push_back(line);
				else
					remainder_ = line;
			}

			return lines;
		}

		std::vector<string_type> operator()(const buffer_type& buf)
		{
			return (*this)(buf, buf.size());
		}

		string_type remainder() const
		{
			return remainder_;
		}

	protected:
			string_type remainder_;
	};


}
