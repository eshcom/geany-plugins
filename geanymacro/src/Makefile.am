include $(top_srcdir)/build/vars.build.mk
plugin = geanymacro

geanyplugins_LTLIBRARIES = geanymacro.la

geanymacro_la_SOURCES = geanymacro.c

geanymacro_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyMacro\" \
						 -Wno-deprecated-declarations
geanymacro_la_LIBADD = $(COMMONLIBS) $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
