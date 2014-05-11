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

dnl Check platform
target_rpi=
case "$build_os" in
    linux*|*BSD*)
        AC_DEFINE([TARGET_LINUX], [1], [Whether we are building for Linux/BSD])
        case "$build_cpu" in
            *arm*|*thumb*)
                target_rpi=yes
                AC_DEFINE([TARGET_RPI], [1], [Whether we are building for Raspberry Pi])
                ;;
            *)
                AC_MSG_ERROR([Your platform is not currently supported])
                ;;
        esac
        ;;
    *)
        AC_MSG_ERROR([Your platform is not currently supported])
        ;;
esac

dnl Common libs
PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GSTREAMER, gstreamer-1.0)
GSTREAMER_LIBS="$GSTREAMER_LIBS -lgstvideo-1.0"
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

PKG_CHECK_MODULES(FREETYPE, freetype2)
AC_SUBST(FREETYPE_CFLAGS)
AC_SUBST(FREETYPE_LIBS)

dnl Check RX build
AS_IF([test "x$with_rx" != "xno"], [
    AC_CHECK_LIB(m, cos, [
        LIBS="-lm $LIBS";
        oldCFLAGS="$CPPFLAGS"
        RPI_CFLAGS="-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux"
        CPPFLAGS="$CPPFLAGS $RPI_CFLAGS"
        AC_CHECK_HEADER([bcm_host.h], [
            AC_SUBST(RPI_CFLAGS)
            CPPFLAGS="$oldCPPFLAGS"
            oldLDFLAGS="$LDFLAGS"
            RPI_LIBS="-L/opt/vc/lib"
            LDFLAGS="$LDFLAGS $RPI_LIBS"
            AC_CHECK_LIB([bcm_host], [bcm_host_init], [
                RPI_LIBS="-lbcm_host $RPI_LIBS"
                AC_SUBST(RPI_LIBS)
                LDFLAGS="$oldLDFLAGS"
                AC_CHECK_LIB([EGL], [eglInitialize], [
                    EGLGLES_LIBS="-lEGL"
                    AC_CHECK_LIB([GLESv2], [glEnable], [
                        EGLGLES_LIBS="-lGLESv2 $EGLGLES_LIBS"
                        AC_SUBST(EGLGLES_LIBS)
                        with_rx=yes
                        AC_MSG_NOTICE([Will build raspifpvrx])
                    ])
                ])
            ])
        ])
    ])
])

AM_CONDITIONAL(WITH_RX, [test x$with_rx = xyes])

dnl Check TX build
AS_IF([test "x$with_tx" != "xno"], [
    AC_CHECK_HEADERS([linux/spi/spidev.h], [
        with_tx=yes
        AC_MSG_NOTICE([Will build raspifpvtx])
    ])
])

AM_CONDITIONAL(WITH_TX, [test x$with_tx = xyes])

AC_CONFIG_FILES([
        Makefile
        src/Makefile
])
AC_OUTPUT

