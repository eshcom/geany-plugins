include $(top_srcdir)/build/vars.build.mk
plugin = runscript

geanyplugins_LTLIBRARIES = runscript.la

runscript_la_SOURCES = runscript.c
runscript_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"RunScript\"
runscript_la_LIBADD = $(COMMONLIBS) \
	$(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
