AC_INIT([dfs], [0.0], [tdj24@cam.ac.uk])
AC_CONFIG_SRCDIR([src/daemon/main.cpp])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign -Wall])

AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
BOOST_REQUIRE
BOOST_PROGRAM_OPTIONS
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

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

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile src/daemon/Makefile src/cli/Makefile src/common/Makefile])
AC_OUTPUT
