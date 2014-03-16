AC_INIT([raspifpv], [1.0], [monsieur.pod@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC

AC_ARG_WITH(rx,
    [AS_HELP_STRING([--with-rx], [support receive (raspifpvrx) @<:@default=check@:>@])],
    [],
    [with_rx=check])

AC_ARG_WITH(tx,
    [AS_HELP_STRING([--with-tx], [support transmit (raspifpvtx) @<:@default=check@:>@])],
    [],
    [with_tx=check])

AC_CANONICAL_BUILD
target_rpi=
case "$build_os" in
    linux*|*BSD*)
        AC_DEFINE([TARGET_LINUX], [1], [Whether we are building for Linux/BSD])
        case "$build_cpu" in
            *arm*|*thumb*)
                target_rpi=yes
                AC_DEFINE([TARGET_RPI], [1], [Whether we are building for Raspberry Pi])
                ;;
        esac
        ;;
    darwin*)
        AC_DEFINE([TARGET_DARWIN], [1], [Whether we are building for Mac OS X])
        ;;
    *)
        AC_MSG_ERROR([Your platform is not currently supported])
        ;;
esac

PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GSTREAMER, gstreamer-1.0)
GSTREAMER_LIBS="$GSTREAMER_LIBS -lgstvideo-1.0"
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

AS_IF([test "x$with_rx" != "xno"],
    [PKG_CHECK_MODULES(CAIRO, cairo, 
       [AC_SUBST(CAIRO_CFLAGS)
        AC_SUBST(CAIRO_LIBS)
        AC_CHECK_LIB(m, cos,
           [LIBS="-lm $LIBS"; with_rx=yes; AC_MSG_NOTICE([Will build raspifpvrx])], 
           [AS_IF([test "x$with_rx" != xcheck], [AC_MSG_FAILURE([--with-rx was given, but test for libm failed])])])],
       [AS_IF([test "x$with_rx" != xcheck], [AC_MSG_FAILURE([--with-rx was given, but test for cairo failed])])])])

AM_CONDITIONAL(WITH_RX, [test x$with_rx = xyes])

AS_IF([test "x$with_tx" != "xno"],
   [AS_IF([test "x$target_rpi" = xyes], 
       [AC_CHECK_HEADERS([linux/spi/spidev.h],
            [with_tx=yes; AC_MSG_NOTICE([Will build raspifpvtx])],
            [AS_IF([test "x$with_tx" != xcheck], [AC_MSG_FAILURE([--with-tx was given, but test for linux/spi/spidev.h failed])])])],
       [AS_IF([test "x$with_tx" = xyes], [AC_MSG_FAILURE([--with-tx was given, but build target is not Raspberry Pi])])])])

AM_CONDITIONAL(WITH_TX, [test x$with_tx = xyes])

AC_CONFIG_FILES([
        Makefile
        src/Makefile
])
AC_OUTPUT

