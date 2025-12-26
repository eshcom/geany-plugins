include $(top_srcdir)/build/vars.build.mk
plugin = geanypg

geanyplugins_LTLIBRARIES = geanypg.la

geanypg_la_SOURCES = \
	helper_functions.c \
	encrypt_cb.c \
	key_selection_dialog.c \
	sign_cb.c \
	verify_cb.c \
	decrypt_cb.c \
	geanypg.c \
	geanypg.h \
	pinentry.c \
	verify_aux.c

geanypg_la_LIBADD = $(COMMONLIBS) $(GPGME_LIBS) \
					$(top_builddir)/utils/src/libgeanypluginutils.la
geanypg_la_CFLAGS = $(AM_CFLAGS) $(GPGME_CFLAGS)
geanypg_la_CPPFLAGS = $(AM_CPPFLAGS) -DG_LOG_DOMAIN=\"GeanyPG\" \
					  -Wno-deprecated-declarations

include $(top_srcdir)/build/cppcheck.mk
