# List extras to package
EXTRA_DIST = \
        autogen.sh \
        debian/postinst \
        debian/copyright \
        debian/control \
        debian/rules \
		debian/changelog \
		debian/postinst \
		src/*

# Sub directories
SUBDIRS = data po

# Includes
INCLUDES = $(DEPS_CFLAGS) $(HILDON_CFLAGS) $(OSSO_CFLAGS) $(GCONF_CFLAGS) $(OSSO_ABOOK_CFLAGS) $(RTCOM_EVENTLOGGER_CFLAGS) $(RTCOM_EVENTLOGGER_UI_CFLAGS) $(CLOCKCORE_CFLAGS)

# Binary
bin_PROGRAMS = \
        extcalllog
# /Binary

# Sources
extcalllog_SOURCES = \
        src/main.c src/main.h src/settings.c src/settings.h src/filters.c src/filters.h \
        src/he-about-dialog.h src/he-about-dialog.c \
        src/rtcom-eventlogger-ui/rtcom-log-columns.h \
	    src/rtcom-eventlogger-ui/rtcom-log-model.h \
	    src/rtcom-eventlogger-ui/rtcom-log-model.c \
	    src/rtcom-eventlogger-ui/rtcom-log-search-bar.h \
	    src/rtcom-eventlogger-ui/rtcom-log-search-bar.c \
	    src/rtcom-eventlogger-ui/rtcom-log-view.h \
	    src/rtcom-eventlogger-ui/rtcom-log-view.c \
	    src/rtcom-eventlogger-ui/utf8.h \
	    src/rtcom-eventlogger-ui/utf8.c
# /Sources

# LDADD
extcalllog_LDADD = \
        $(DEPS_LIBS) $(HILDON_LIBS) $(OSSO_LIBS) $(GCONF_LIBS) $(OSSO_ABOOK_LIBS) $(RTCOM_EVENTLOGGER_LIBS) $(RTCOM_EVENTLOGGER_UI_LIBS) $(CLOCKCORE_LIBS)
# /LDADD

deb: dist
	-mkdir $(top_builddir)/debian-build
	cd $(top_builddir)/debian-build && tar zxf ../$(top_builddir)/$(PACKAGE)-$(VERSION).tar.gz
	cd $(top_builddir)/debian-build/$(PACKAGE)-$(VERSION) && dpkg-buildpackage -rfakeroot
	rm -rf $(top_builddir)/debian-build/$(PACKAGE)-$(VERSION)
