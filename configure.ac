AC_INIT([libarchiver], [0.0.1], [mignov@gmail.com])
LT_INIT([shared])
AC_ENABLE_SHARED
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror])

AC_PROG_CC
AM_PROG_CC_C_O
AC_CHECK_LIB([mincrypt], [mincrypt_set_password], [], AC_MSG_ERROR([You need libmincrypt to compile this application]))
AC_CHECK_LIB([lzma], [lzma_stream_encoder], [], AC_MSG_ERROR([You need liblzma to compile this application]))
AC_CHECK_TOOL([MKDIR], [mkdir])
AC_CHECK_TOOL([ECHO], [echo])
AC_CHECK_TOOL([RM], [rm])
AC_CHECK_TOOL([CP], [cp])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile include/Makefile examples/Makefile])
AC_OUTPUT
LT_OUTPUT
