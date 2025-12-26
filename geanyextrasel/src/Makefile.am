include $(top_srcdir)/build/vars.build.mk
plugin = geanyextrasel

geanyplugins_LTLIBRARIES = geanyextrasel.la

geanyextrasel_la_SOURCES = extrasel.c

geanyextrasel_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"ExtraSelection\" \
							-Wno-deprecated-declarations
geanyextrasel_la_LIBADD = $(COMMONLIBS) \
						  $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
