include $(top_srcdir)/build/vars.build.mk
plugin = geanyctags

geanyplugins_LTLIBRARIES = geanyctags.la

geanyctags_la_SOURCES = \
	geanyctags.c \
	readtags.h \
	readtags.c

geanyctags_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DG_LOG_DOMAIN=\"GeanyCtags\"
geanyctags_la_CFLAGS = $(AM_CFLAGS)
geanyctags_la_LIBADD = $(COMMONLIBS) \
	$(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
