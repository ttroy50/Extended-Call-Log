/* This file is part of Extended Call Log
 *
 * Copyright (C) 2010 Thom Troy
 *
 * WebTexter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (GPL) as published by
 * the Free Software Foundation
 *
 * WebTexter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Extended Call Log. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 ============================================================================
 Name        : main.h
 Author      : Matrim
 Version     : 0.1
 Description : Main part of the gui application
 ============================================================================
 */

#ifndef _MAIN_H
#define _MAIN_H


#include <hildon/hildon-program.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkbutton.h>
#include <libosso.h>
#include <string.h>
#include <stdlib.h>
#include <rtcom-eventlogger/eventlogger.h>
#include <libebook/e-book.h>
#include <libosso-abook/osso-abook.h>
#include "rtcom-eventlogger-ui/rtcom-log-view.h"
#include "rtcom-eventlogger-ui/rtcom-log-model.h"
#include "rtcom-eventlogger-ui/rtcom-log-columns.h"
#include "rtcom-eventlogger-ui/rtcom-log-search-bar.h"
#include "settings.h"

G_BEGIN_DECLS

/* Defines to add the application to dbus and keep it running
 * Please do not modify "APP_NAME" (or other defines) to different name
 */
#define APP_NAME "extcalllog"
#define APP_VER "0.4"
#define APP_SERVICE "org.maemo.extcalllog"
#define APP_METHOD "/org/maemo/extcalllog"
/* end defines */


/* Define structure for data that's passed around
 * the application */
typedef struct
{
	HildonProgram *program;
	HildonStackableWindow *mainWindow;
	GtkWidget * log_view;
	RTComLogModel * log_model;
	GtkWidget * search_bar;
	RTComElQueryGroupBy static_group_by;
	gboolean showing_details;
	AppSettings settings;
	gint current_type;
	gint current_direction;
	gboolean filter_by_date;
	gint start_day;
	gint start_month;
	gint start_year;
	gint start_date;
	gint end_day;
	gint end_month;
	gint end_year;
	gint end_date;

} AppData;

G_END_DECLS

#endif
