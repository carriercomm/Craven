AM_CXXFLAGS = @AM_CXXFLAGS@ -I$(top_srcdir)/src/common
bin_PROGRAMS = rafttest

rafttest_SOURCES = main.cpp ../raftlog.cpp ../raftstate.cpp ../raftrpc.cpp ../raftclient.cpp ../raftctl.cpp ../../common/json_help.cpp
rafttest_CPPFLAGS = $(BOOST_CPPFLAGS) $(JSONCPP_CFLAGS)
rafttest_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_FILESYSTEM_LIBS) $(BOOST_THREAD_LIBS) $(BOOST_LOG_LIBS) $(BOOST_LOG_SETUP_LIBS) $(JSONCPP_LIBS)
rafttest_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_FILESYSTEM_LDFLAGS) $(BOOST_THREAD_LDFLAGS) $(BOOST_LOG_LDFLAGS) $(BOOST_LOG_SETUP_LDFLAGS) $(JSONCPP_LDFLAGS)
