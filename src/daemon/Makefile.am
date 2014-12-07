AM_CPPFLAGS = -I$(top_srcdir)/common
bin_PROGRAMS = dfsd
dfsd_SOURCES = main.cpp ../common/configure.cpp configure.cpp
dfsd_LDADD = $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_FILESYSTEM_LIBS)
dfsd_LDFLAGS = $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(BOOST_FILESYSTEM_LDFLAGS)
