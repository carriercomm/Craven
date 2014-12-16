check_PROGRAMS = remcon connection_pool
remcon_SOURCES = remcon-test.cpp ../remcon.cpp

remcon_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) $(BOOST_LOG_LIBS)
remcon_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_LOG_LDADD)

connection_pool_SOURCES = connection_pool-test.cpp

connection_pool_LDADD = $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
connection_pool_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)

TESTS = $(check_PROGRAMS)
XFAIL_TESTS = remcon
