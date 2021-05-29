include $(top_srcdir)/build/vars.build.mk
plugin = jsonprettifier

geanyplugins_LTLIBRARIES = jsonprettifier.la

jsonprettifier_la_SOURCES = jsonprettifier.c
jsonprettifier_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"JSONPrettifier\"
jsonprettifier_la_CFLAGS = $(AM_CFLAGS) $(YAJL_CFLAGS)
jsonprettifier_la_LIBADD = $(COMMONLIBS) $(YAJL_LIBS)

include $(top_srcdir)/build/cppcheck.mk
