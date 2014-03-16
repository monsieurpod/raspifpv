## Process this file with automake to produce Makefile.in

SUBDIRS = src
dist_doc_DATA = README

EXTRA_DIST = \
    setup/raspifpvrx \
    setup/raspifpvtx \
    setup/raspifpv.conf

init_ddir = $(sysconfdir)/init.d
init_d_SCRIPTS = setup/raspifpvrx setup/raspifpvtx

sysconf_DATA = setup/raspifpv.conf
