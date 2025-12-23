include $(top_srcdir)/build/vars.build.mk
plugin = git-manager

geanyplugins_LTLIBRARIES = git-manager.la

git_manager_la_SOURCES  = git-manager.c

git_manager_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GitManager\" \
                          -Wno-deprecated-declarations
git_manager_la_CFLAGS   = $(AM_CFLAGS) $(GITCHANGEBAR_CFLAGS)
git_manager_la_LIBADD   = $(COMMONLIBS) $(GITMANAGER_LIBS) \
                          $(top_builddir)/utils/src/libgeanypluginutils.la

include $(top_srcdir)/build/cppcheck.mk
