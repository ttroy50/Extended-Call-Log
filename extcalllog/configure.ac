dnl Process this file with autoconf to produce a configure script.

AC_INIT(extcalllog,0.5)
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE([1.7.9])

AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_INSTALL
dnl AC_PREFIX_DEFAULT(/var/lib/install/usr)
AC_PREFIX_DEFAULT(/usr)

CFLAGS="$CFLAGS -Wall -ansi -Wmissing-prototypes -Wmissing-declarations"

PKG_CHECK_MODULES(DEPS, gtk+-2.0 >= 2.2 glib-2.0 >= 2.2 gnome-vfs-2.0 >= 2.2)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

PKG_CHECK_MODULES(HILDON, hildon-1 >= 0.8.4 hildon-fm-2 >= 0.8.4)
AC_SUBST(HILDON_LIBS)
AC_SUBST(HILDON_CFLAGS)

PKG_CHECK_MODULES(OSSO, libosso >= 0.8.4) 
AC_SUBST(OSSO_LIBS)
AC_SUBST(OSSO_CFLAGS)

PKG_CHECK_MODULES(GCONF, [gconf-2.0 >= 2.6.2])
AC_SUBST(GCONF_LIBS)
AC_SUBST(GCONF_CFLAGS)

PKG_CHECK_MODULES(OSSO_ABOOK, [libosso-abook-1.0])
AC_SUBST(OSSO_ABOOK_CFLAGS)
AC_SUBST(OSSO_ABOOK_LIBS)

PKG_CHECK_MODULES(RTCOM_EVENTLOGGER, [rtcom-eventlogger])
AC_SUBST(RTCOM_EVENTLOGGER_CFLAGS)
AC_SUBST(RTCOM_EVENTLOGGER_LIBS)

PKG_CHECK_MODULES(
	RTCOM_EVENTLOGGER_UI,
	glib-2.0
	gobject-2.0
	gtk+-2.0
	hildon-1
	rtcom-eventlogger >= 1.1
	rtcom-eventlogger-plugins >= 1.0
	dbus-1
	libosso-abook-1.0 >= 4.20081010
	libmcclient >= 5
    gconf-2.0)
AC_SUBST(RTCOM_EVENTLOGGER_UI_CFLAGS)
AC_SUBST(RTCOM_EVENTLOGGER_UI_LIBS)

PKG_CHECK_MODULES(OSSOSETTINGS, osso-af-settings >= 0.8.4)

# Application icon install directories
icon_26x26dir=$datadir/icons/hicolor/26x26/hildon
icon_48x48dir=$datadir/icons/hicolor/48x48/hildon

localedir=`$PKG_CONFIG osso-af-settings --variable=localedir`
hildondesktopentrydir=/usr/share/dbus-1/services
hildoncpdesktopentrydir=/usr/
hildonstatusbarplugindir=`$PKG_CONFIG hildon-status-bar-lib --variable=pluginlibdir`
hildoncplibdir=`$PKG_CONFIG hildon-control-panel --variable=pluginlibdir`
hildoniconthemedir=$HOME/.icons/hicolor

AC_SUBST(localedir)
AC_DEFINE_UNQUOTED(localedir,"$localedir",[localedir])
AC_SUBST(desktopentrydir)
AC_SUBST(hildondesktopentrydir)
AC_SUBST(hildoncpdesktopentrydir)
AC_SUBST(hildonstatusbarplugindir)
AC_SUBST(hildoncplibdir)
AC_SUBST(hildoniconthemedir)
AC_SUBST(icon_26x26dir)
AC_SUBST(icon_48x48dir)

IT_PROG_INTLTOOL([0.35.0])
GETTEXT_PACKAGE="$PACKAGE_NAME"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
		                      [The domain to use with gettext])
AM_GLIB_GNU_GETTEXT

USE_NLS=yes
AC_SUBST(USE_NLS)


AC_CONFIG_FILES([
    Makefile  
    src/Makefile
    data/Makefile 
    po/Makefile.in
    data/org.maemo.extcalllog.service
    data/extcalllog.desktop
])

AC_OUTPUT
