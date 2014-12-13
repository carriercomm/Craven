#define BOOST_TEST_MODULE "Utility"
#include <boost/test/unit_test.hpp>

#include <vector>
#include <sstream>

#include "../linebuffer.hpp"

typedef util::line_buffer<std::basic_string<char>> lbuf_type;

BOOST_AUTO_TEST_CASE(default_ctor_empty_buffer)
{
	std::ostringstream os;

	lbuf_type sut;

	BOOST_REQUIRE_EQUAL(sut.remainder(), "");
}

BOOST_AUTO_TEST_CASE(string_init)
{
	std::ostringstream os;

	lbuf_type sut("fnord");

	BOOST_REQUIRE_EQUAL(sut.remainder(), "fnord");
}

BOOST_AUTO_TEST_CASE(functor_splits_on_line)
{
	lbuf_type::buffer_type buf = "foo bar\nbaz\n";
	lbuf_type sut;

	auto res = sut(buf);

	BOOST_REQUIRE_EQUAL(res.size(), 2);
	BOOST_REQUIRE_EQUAL(res[0], "foo bar");
	BOOST_REQUIRE_EQUAL(res[1], "baz");
}


BOOST_AUTO_TEST_CASE(functor_preserves_remainder)
{
	lbuf_type::buffer_type buf = "foo bar\nbaz";
	lbuf_type sut;

	auto res = sut(buf);

	BOOST_CHECK_EQUAL(res.size(), 1);
	BOOST_CHECK_EQUAL(res[0], "foo bar");

	BOOST_REQUIRE_EQUAL(sut.remainder(), "baz");
}

BOOST_AUTO_TEST_CASE(functor_uses_init)
{
	lbuf_type::buffer_type buf = "\nfoo\n";
	lbuf_type sut("fnord");

	auto res = sut(buf);

	BOOST_REQUIRE_EQUAL(res.size(), 2);
	BOOST_REQUIRE_EQUAL(res[0], "fnord");
	BOOST_REQUIRE_EQUAL(res[1], "foo");
}

BOOST_AUTO_TEST_CASE(functor_repeats_uses_remainder)
{
	lbuf_type::buffer_type buf = "fnord\nfoo\nHail Eris!";
	lbuf_type::buffer_type buf2 = "\nfoobarbaz\nthud\n";

	lbuf_type sut;

	auto res = sut(buf);

	BOOST_REQUIRE_EQUAL(res.size(), 2);
	BOOST_REQUIRE_EQUAL(res[0], "fnord");
	BOOST_REQUIRE_EQUAL(res[1], "foo");

	auto res2 = sut(buf2);

	BOOST_REQUIRE_EQUAL(res2.size(), 3);
	BOOST_REQUIRE_EQUAL(res2[0], "Hail Eris!");
	BOOST_REQUIRE_EQUAL(res2[1], "foobarbaz");
	BOOST_REQUIRE_EQUAL(res2[2], "thud");
	BOOST_REQUIRE_EQUAL(sut.remainder(), "");
}

BOOST_AUTO_TEST_CASE(remainder_empty)
{
	lbuf_type sut;
	lbuf_type::buffer_type buf = "fnord\nfoo\nHail Eris!\n";

	auto res = sut(buf);

	
	BOOST_REQUIRE_EQUAL(res.size(), 3);
	BOOST_REQUIRE_EQUAL(res[0], "fnord");
	BOOST_REQUIRE_EQUAL(res[1], "foo");
	BOOST_REQUIRE_EQUAL(res[2], "Hail Eris!");

	BOOST_REQUIRE_EQUAL(sut.remainder(), "");
}


