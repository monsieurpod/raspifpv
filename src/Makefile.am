## Process this file with automake to produce Makefile.in

AM_CFLAGS = \
    @GLIB_CFLAGS@ \
    @GSTREAMER_CFLAGS@ \
    @CAIRO_CFLAGS@

bin_PROGRAMS =

if WITH_RX
bin_PROGRAMS += raspifpvrx
endif

if WITH_TX
bin_PROGRAMS += raspifpvtx
endif

raspifpvrx_SOURCES = \
    main-rx.c common.h render.h render.c telemetry_common.h telemetry_common.c telemetry_rx.h telemetry_rx.c \
    geometry.h geometry.c

raspifpvtx_SOURCES = \
    main-tx.c common.h telemetry_common.h telemetry_common.c telemetry_tx.h telemetry_tx.c spi.h spi.c

raspifpvrx_LDADD = \
    @GLIB_LIBS@ \
    @GSTREAMER_LIBS@ \
    @CAIRO_LIBS@

raspifpvtx_LDADD = \
    @GLIB_LIBS@ \
    @GSTREAMER_LIBS@
