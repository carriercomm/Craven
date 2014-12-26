AM_CXXFLAGS = @AM_CXXFLAGS@ -I$(top_srcdir)/common
bin_PROGRAMS = dfsd
dfsd_SOURCES = main.cpp ../common/configure.cpp configure.cpp daemon.cpp signals.cpp remcon.cpp raftlog.cpp configure.hpp connection_pool.hpp daemon.hpp dispatch.hpp raftlog.hpp raftstate.hpp remcon.hpp signals.hpp raftstate.hpp raftstate.cpp raftrpc.hpp raftrpc.cpp ../common/json_help.cpp raftclient.hpp raftclient.cpp
dfsd_CPPFLAGS = $(BOOST_CPPFLAGS) $(JSONCPP_CFLAGS)
dfsd_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_FILESYSTEM_LIBS) $(BOOST_THREAD_LIBS) $(BOOST_LOG_LIBS) $(BOOST_LOG_SETUP_LIBS) $(JSONCPP_LIBS)
dfsd_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(BOOST_FILESYSTEM_LDFLAGS) $(BOOST_THREAD_LDFLAGS) $(BOOST_LOG_LDFLAGS) $(BOOST_LOG_SETUP_LDFLAGS) $(JSONCPP_LDFLAGS)

SUBDIRS=test
