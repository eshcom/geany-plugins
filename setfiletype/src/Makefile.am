include $(top_srcdir)/build/vars.build.mk
plugin = setfiletype

geanyplugins_LTLIBRARIES = setfiletype.la

setfiletype_la_SOURCES = setfiletype.c
setfiletype_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"SetFileType\"
setfiletype_la_LIBADD = $(COMMONLIBS)

include $(top_srcdir)/build/cppcheck.mk
