include $(top_srcdir)/build/vars.build.mk
plugin = lipsum

geanyplugins_LTLIBRARIES = lipsum.la

lipsum_la_SOURCES = lipsum.c

lipsum_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"Lipsum\" \
					 -Wno-deprecated-declarations
lipsum_la_LIBADD = $(COMMONLIBS) $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
