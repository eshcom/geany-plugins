include $(top_srcdir)/build/vars.build.mk
plugin = keyrecord

geanyplugins_LTLIBRARIES = keyrecord.la

keyrecord_la_SOURCES = keyrecord.c

keyrecord_la_CPPFLAGS = $(AM_CPPFLAGS) -Wno-deprecated-declarations
keyrecord_la_LIBADD = $(COMMONLIBS) $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
