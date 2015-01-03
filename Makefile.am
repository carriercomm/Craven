ACLOCAL_AMFLAGS = -I m4
AM_CXXFLAGS = @AM_CXXFLAGS@ -I$(top_srcdir)/src/common

bin_PROGRAMS = src/cli/dfsctl src/daemon/dfsd
check_PROGRAMS = src/cli/test/comms src/common/test/expansion src/common/test/linebuffer \
				 src/common/test/connection src/daemon/test/connection_pool \
				 src/daemon/test/dispatch src/daemon/test/raftlog src/daemon/test/raftstate \
				 src/daemon/test/raftclient src/daemon/test/persist src/daemon/test/changetx

noinst_HEADERS = src/common/configure.hpp src/common/linebuffer.hpp src/common/connection.hpp src/common/json_help.hpp

src_cli_dfsctl_SOURCES = src/cli/main.cpp src/common/configure.cpp src/cli/configure.cpp\
						 src/cli/comms.cpp src/cli/comms.hpp src/cli/configure.hpp

src_cli_dfsctl_CPPFLAGS = $(BOOST_CPPFLAGS)
src_cli_dfsctl_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_FILESYSTEM_LIBS) $(BOOST_THREAD_LIBS) $(BOOST_LOG_LIBS) $(BOOST_LOG_SETUP_LIBS)
src_cli_dfsctl_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(BOOST_FILESYSTEM_LDFLAGS) $(BOOST_THREAD_LDFLAGS) $(BOOST_LOG_LDFLAGS) $(BOOST_LOG_SETUP_LDFLAGS)


src_daemon_dfsd_SOURCES = src/daemon/main.cpp src/common/configure.cpp \
						  src/daemon/configure.cpp src/daemon/daemon.cpp src/daemon/signals.cpp \
						  src/daemon/remcon.cpp src/daemon/raftlog.cpp src/daemon/configure.hpp \
						  src/daemon/connection_pool.hpp src/daemon/daemon.hpp \
						  src/daemon/dispatch.hpp src/daemon/raftlog.hpp src/daemon/raftstate.hpp \
						  src/daemon/remcon.hpp src/daemon/signals.hpp src/daemon/raftstate.hpp \
						  src/daemon/raftstate.cpp src/daemon/raftrpc.hpp src/daemon/raftrpc.cpp \
						  src/common/json_help.cpp src/daemon/raftclient.hpp \
						  src/daemon/raftclient.cpp src/daemon/raftrequest.hpp src/daemon/raftrequest.cpp \
						  src/daemon/raftctl.hpp src/daemon/raftctl.cpp \
						  src/daemon/changetx.hpp src/daemon/changetx.cpp

src_daemon_dfsd_CPPFLAGS = $(BOOST_CPPFLAGS) $(JSONCPP_CFLAGS) $(LIBB64_CPPFLAGS)
src_daemon_dfsd_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_FILESYSTEM_LIBS) \
						$(BOOST_THREAD_LIBS) $(BOOST_LOG_LIBS) $(BOOST_LOG_SETUP_LIBS) $(JSONCPP_LIBS) \
						$(LIBB64_LIBS)

src_daemon_dfsd_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LDFLAGS) \
						  $(BOOST_FILESYSTEM_LDFLAGS) $(BOOST_THREAD_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
						  $(BOOST_LOG_SETUP_LDFLAGS) $(JSONCPP_LDFLAGS) $(LIBB64_LDFLAGS)


src_cli_test_comms_SOURCES = src/cli/test/comms-test.cpp src/cli/comms.cpp src/cli/configure.cpp \
							 src/common/configure.cpp

src_cli_test_comms_CPPFLAGS = $(BOOST_CPPFLAGS)
src_cli_test_comms_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_FILESYSTEM_LIBS) \
						   $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) \
						   $(BOOST_LOG_LIBS) $(BOOST_LOG_SETUP_LIBS)

src_cli_test_comms_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_FILESYSTEM_LDFLAGS) \
							 $(BOOST_PROGRAM_OPTIONS_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) \
							 $(BOOST_LOG_LDFLAGS) $(BOOST_LOG_SETUP_LIBS)

src_common_test_expansion_SOURCES = src/common/test/expansion.cpp src/common/configure.cpp

src_common_test_expansion_CPPFLAGS = $(BOOST_CPPFLAGS)
src_common_test_expansion_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) \
								  $(BOOST_FILESYSTEM_LIBS) $(BOOST_THREAD_LIBS) \
								  $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)

src_common_test_expansion_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LDFLAGS) \
									$(BOOST_FILESYSTEM_LDFLAGS) $(BOOST_THREAD_LDFLAGS) \
									$(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)

src_common_test_linebuffer_SOURCES = src/common/test/linebuffer-test.cpp

src_common_test_linebuffer_CPPFLAGS = $(BOOST_CPPFLAGS)
src_common_test_linebuffer_LDADD = $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
src_common_test_linebuffer_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)

src_common_test_connection_SOURCES = src/common/test/connection-test.cpp

src_common_test_connection_CPPFLAGS = $(BOOST_CPPFLAGS)
src_common_test_connection_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) \
								   $(BOOST_LOG_LIBS) $(BOOST_LOG_SETUP_LIBS)
src_common_test_connection_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) \
									 $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
									 $(BOOST_LOG_SETUP_LDFLAGS)

src_daemon_test_connection_pool_SOURCES = src/daemon/test/connection_pool-test.cpp

src_daemon_test_connection_pool_LDADD = $(BOOST_SYSTEM_LIBS) \
										$(BOOST_UNIT_TEST_FRAMEWORK_LIBS) $(BOOST_LOG_LIBS) \
										$(BOOST_LOG_SETUP_LIBS)

src_daemon_test_connection_pool_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) \
										  $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) \
										  $(BOOST_LOG_LDFLAGS) $(BOOST_LOG_SETUP_LDFLAGS)

src_daemon_test_dispatch_SOURCES = src/daemon/test/dispatch-test.cpp src/common/json_help.cpp

src_daemon_test_dispatch_CPPFLAGS = $(BOOST_CPPFLAGS) $(JSONCPP_CFLAGS)
src_daemon_test_dispatch_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_LOG_LIBS) \
								 $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) $(JSONCPP_LIBS) \
								 $(BOOST_LOG_LIBS) $(BOOST_LOG_SETUP_LIBS)

src_daemon_test_dispatch_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
								   $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) \
								   $(JSONCPP_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
								   $(BOOST_LOG_SETUP_LDFLAGS)

src_daemon_test_raftlog_SOURCES = src/daemon/test/raftlog-test.cpp \
								  src/daemon/raftlog.cpp src/common/json_help.cpp

src_daemon_test_raftlog_CPPFLAGS = $(BOOST_CPPFLAGS) $(JSONCPP_CFLAGS)
src_daemon_test_raftlog_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_LOG_LIBS) \
								$(BOOST_FILESYSTEM_LIBS) \
								$(BOOST_UNIT_TEST_FRAMEWORK_LIBS) $(JSONCPP_LIBS)

src_daemon_test_raftlog_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
								  $(BOOST_FILESYSTEM_LDFLAGS) \
								  $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) \
								  $(JSONCPP_LDFLAGS)

src_daemon_test_raftstate_SOURCES = src/daemon/test/raftstate-test.cpp \
									src/daemon/raftstate.cpp \
									src/daemon/raftlog.cpp \
									src/daemon/raftrpc.cpp \
									src/common/json_help.cpp

src_daemon_test_raftstate_CPPFLAGS = $(BOOST_CPPFLAGS) $(JSONCPP_CFLAGS)
src_daemon_test_raftstate_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_LOG_LIBS) \
								  $(BOOST_FILESYSTEM_LIBS) \
								  $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) $(JSONCPP_LIBS)

src_daemon_test_raftstate_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
									$(BOOST_FILESYSTEM_LDFLAGS) \
									$(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) \
									$(JSONCPP_LDFLAGS)

src_daemon_test_raftclient_SOURCES = src/daemon/test/raftclient-test.cpp \
									 src/daemon/raftrequest.cpp \
									 src/daemon/raftclient.cpp \
									 src/daemon/raftstate.cpp \
									 src/daemon/raftlog.cpp \
									 src/daemon/raftrpc.cpp \
									 src/common/json_help.cpp

src_daemon_test_raftclient_CPPFLAGS = $(BOOST_CPPFLAGS) $(JSONCPP_CFLAGS)
src_daemon_test_raftclient_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_LOG_LIBS) \
								   $(BOOST_FILESYSTEM_LIBS) \
								   $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) \
								   $(JSONCPP_LIBS)

src_daemon_test_raftclient_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
									 $(BOOST_FILESYSTEM_LDFLAGS) \
									 $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) \
									 $(JSONCPP_LDFLAGS)

src_daemon_test_persist_SOURCES =  src/daemon/test/persist-test.cpp src/daemon/persist.cpp

src_daemon_test_persist_CPPFLAGS = $(BOOST_CPPFLAGS) $(LIBB64_CPPFLAGS)
src_daemon_test_persist_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_LOG_LIBS) \
								$(BOOST_FILESYSTEM_LIBS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS)
src_daemon_test_persist_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
								  $(BOOST_FILESYSTEM_LDFLAGS) \
								  $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)

src_daemon_test_changetx_SOURCES = src/daemon/test/changetx-test.cpp \
								   src/daemon/changetx.cpp src/daemon/persist.cpp \
								   src/common/json_help.cpp

src_daemon_test_changetx_CPPFLAGS = $(BOOST_CPPFLAGS) $(LIBB64_CPPFLAGS) \
									$(JSONCPP_CFLAGS)
src_daemon_test_changetx_LDADD = $(BOOST_SYSTEM_LIBS) $(BOOST_LOG_LIBS) \
								 $(BOOST_FILESYSTEM_LIBS) $(BOOST_UNIT_TEST_FRAMEWORK_LIBS) \
								 $(JSONCPP_LIBS) $(LIBB64_LIBS)
src_daemon_test_changetx_LDFLAGS = $(BOOST_SYSTEM_LDFLAGS) $(BOOST_LOG_LDFLAGS) \
								   $(BOOST_FILESYSTEM_LDFLAGS) \
								   $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS) \
								   $(JSONCPP_LDFLAGS) $(LIBB64_LDFLAGS)



TESTS = $(check_PROGRAMS)

if HAVE_DOXYGEN
SUBDIRS = doc
endif
