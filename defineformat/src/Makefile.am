include $(top_srcdir)/build/vars.build.mk
plugin = defineformat

geanyplugins_LTLIBRARIES = defineformat.la

defineformat_la_SOURCES = defineformat.c

defineformat_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"DefineFormat\" \
						   -Wno-deprecated-declarations
defineformat_la_LIBADD = $(COMMONLIBS) \
						 $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
