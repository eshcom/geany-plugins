include $(top_srcdir)/build/vars.build.mk
plugin = geanydoc

geanyplugins_LTLIBRARIES = geanydoc.la

geanydoc_la_SOURCES = \
	config.c \
	geanydoc.c \
	geanydoc.h

geanydoc_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyDoc\" \
					   -Wno-deprecated-declarations
geanydoc_la_LIBADD = $(COMMONLIBS) $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
