AC_INIT([dfs], [0.4.0], [tdj24@cam.ac.uk])
AC_CONFIG_SRCDIR([src/daemon/main.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

PKG_CHECK_MODULES([JSONCPP], [jsoncpp])
PKG_CHECK_MODULES([FUSE], [fuse])
AC_CHECK_HEADERS(b64/encode.h, [], [AC_MSG_ERROR([This project requires libb64])],
[
#define BUFFERSIZE 1048576
#include <b64/encode.h>
])
AC_CHECK_HEADERS(b64/decode.h,
				 [AC_SUBST(LIBB64_CPPFLAGS, ["-DBUFFERSIZE=1048576"])]
				 [AC_SUBST(LIBB64_LIBS, ["-lb64"])]
				 [AC_SUBST(LIBB64_LDFLAGS, [])],
				 [AC_MSG_ERROR([This project requires libb64])],
[
#define BUFFERSIZE 1048576
#include <b64/decode.h>
])

BOOST_REQUIRE([1.56])
BOOST_ASIO
BOOST_BIND
BOOST_CONVERSION
BOOST_FILESYSTEM
BOOST_FORMAT
BOOST_LOG
BOOST_LOG_SETUP
BOOST_OPTIONAL
BOOST_PROGRAM_OPTIONS
BOOST_SIGNALS2
BOOST_STRING_ALGO
BOOST_SYSTEM
BOOST_TEST
BOOST_THREAD
BOOST_UUID
BOOST_RANGE

AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

AC_SUBST([AM_CXXFLAGS], ["-DBOOST_ALL_DYN_LINK -pthread"])
AC_SUBST([AM_LDADD], ["-pthread"])

AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "${DOXYGEN}";
then
	AC_MSG_WARN([Doxygen not installed -- continuing without documentation generation])
fi

AM_CONDITIONAL([HAVE_DOXYGEN],
			   [test -n "$DOXYGEN"])AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([doc/Doxyfile doc/Makefile])])

AC_DEFINE([DOT_DIR], ["~/." PACKAGE_NAME "/"], [The default location of the dot directory])
AC_DEFINE([RCFILE], [DOT_DIR PACKAGE_NAME "rc"], [The default location of the rcfile])
AC_DEFINE([COMMS_SOCKET], [DOT_DIR "comms"], [The default location of the communication socket])
AC_DEFINE([LOG_LOCATION], [DOT_DIR PACKAGE_NAME ".log"], [The default location of the daemon log])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
