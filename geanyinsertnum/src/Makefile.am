include $(top_srcdir)/build/vars.build.mk
plugin = geanyinsertnum

geanyplugins_LTLIBRARIES = geanyinsertnum.la

geanyinsertnum_la_SOURCES = insertnum.c

geanyinsertnum_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"InsertNum\" \
							 -Wno-deprecated-declarations
geanyinsertnum_la_LIBADD = $(COMMONLIBS) \
						   $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
