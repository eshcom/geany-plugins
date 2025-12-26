include $(top_srcdir)/build/vars.build.mk
plugin = tableconvert

geanyplugins_LTLIBRARIES = tableconvert.la

tableconvert_la_SOURCES = \
	tableconvert.c \
	tableconvert.h \
	tableconvert_ui.c \
	tableconvert_ui.h

tableconvert_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"TableConvert\" \
						   -Wno-deprecated-declarations
tableconvert_la_LIBADD = $(COMMONLIBS) \
						 $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
