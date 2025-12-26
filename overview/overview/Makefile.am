include $(top_srcdir)/build/vars.build.mk

plugin = overview

geanyplugins_LTLIBRARIES = overview.la

overview_la_SOURCES = \
	overviewcolor.c \
	overviewcolor.h \
	overviewplugin.c \
	overviewplugin.h \
	overviewprefs.c \
	overviewprefs.h \
	overviewprefspanel.c \
	overviewprefspanel.h \
	overviewscintilla.c \
	overviewscintilla.h \
	overviewui.c \
	overviewui.h

overview_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"Overview\"
overview_la_CFLAGS = $(AM_CFLAGS) -I$(srcdir)/..
overview_la_LIBADD = $(COMMONLIBS) \
					 $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
