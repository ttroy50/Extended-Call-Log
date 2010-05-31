/*******************************************************************************
 * Copyright (c) 2007-2008 INdT, (c) 2009 Nokia.
 *
 * This code example is licensed under a MIT-style license,
 * that can be found in the file called "COPYING" in the package
 *
 */

/*
 ============================================================================
 Name        : localisation.h
 Author      : Thom Troy
 Version     : 0.1
 Description : Header with localization facilities
 ============================================================================
 */
#ifndef LOCALISATION_H
#define LOCALISATION_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif                          /* HAVE_CONFIG_H */

#ifdef ENABLE_NLS
#include <locale.h>
#include <libintl.h>
#define _(String) gettext(String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#define locale_init() setlocale(LC_ALL, "");\
    bindtextdomain(GETTEXT_PACKAGE, localedir);\
    textdomain(GETTEXT_PACKAGE);
#else                           /* NLS is disabled */
#define locale_init()
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain)
#define bind_textdomain_codeset(Domain,Codeset) (Codeset)
#endif                          /* ENABLE_NLS */

#endif /* LOCALISATION_H */
#ifndef LOCALISATION_H
#define LOCALISATION_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif                          /* HAVE_CONFIG_H */

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#define locale_init() setlocale(LC_ALL, "");\
    bindtextdomain(GETTEXT_PACKAGE, localedir);\
    textdomain(GETTEXT_PACKAGE);
#else                           /* NLS is disabled */
#define locale_init()
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain)
#define bind_textdomain_codeset(Domain,Codeset) (Codeset)
#endif                          /* ENABLE_NLS */

#endif /* LOCALISATION_H */

