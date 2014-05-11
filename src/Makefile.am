## Process this file with automake to produce Makefile.in

AM_CFLAGS = \
    @GLIB_CFLAGS@ \
    @GSTREAMER_CFLAGS@ \
    @FREETYPE_CFLAGS@ \
    @RPI_CFLAGS@

bin_PROGRAMS =

if WITH_RX
bin_PROGRAMS += raspifpvrx
endif

if WITH_TX
bin_PROGRAMS += raspifpvtx
endif

raspifpvrx_SOURCES = \
    main-rx.c common.h gstreamer_renderer.h gstreamer_renderer.c egl_telemetry_renderer.h \
    egl_telemetry_renderer.c telemetry_common.h telemetry_common.c telemetry_rx.h telemetry_rx.c

raspifpvtx_SOURCES = \
    main-tx.c common.h telemetry_common.h telemetry_common.c telemetry_tx.h telemetry_tx.c spi.h spi.c

raspifpvrx_LDADD = \
    @GLIB_LIBS@ \
    @GSTREAMER_LIBS@ \
    @FREETYPE_LIBS@ \
    @RPI_LIBS@ \
	@EGLGLES_LIBS@

raspifpvtx_LDADD = \
    @GLIB_LIBS@ \
    @GSTREAMER_LIBS@
