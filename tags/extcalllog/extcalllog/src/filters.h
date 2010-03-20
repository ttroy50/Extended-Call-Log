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

#ifndef _FILTERS_H
#define _FILTERS_H

#include <rtcom-eventlogger/eventlogger.h>
#include <rtcom-eventlogger/eventlogger-query.h>
#include <libebook/e-book.h>
#include <libosso-abook/osso-abook.h>
#include "rtcom-eventlogger-ui/rtcom-log-view.h"
#include "rtcom-eventlogger-ui/rtcom-log-model.h"
#include "rtcom-eventlogger-ui/rtcom-log-columns.h"
#include "rtcom-eventlogger-ui/rtcom-log-search-bar.h"
#include "settings.h"
#include "main.h"

G_BEGIN_DECLS

typedef enum {
  ALL_DIRECTIONS,
  INBOUND,
  OUTBOUND,
  MISSED
} call_direction;

void query_prepare(RTComElQuery* query, gint type, gint direction);
void missed_calls (GtkButton* button, gpointer data);
void recieved_calls (GtkButton* button, gpointer data);
void dialed_calls (GtkButton* button, gpointer data);
void voip_calls (GtkButton* button, gpointer data);
void gsm_calls (GtkButton* button, gpointer data);
void all_call_types (GtkButton* button, gpointer data);
void all_call_directions (GtkButton* button, gpointer data);
void populate_calls(GtkWidget * widget, gpointer data);
void populate_calls_default(AppData* data);
void refresh(GtkWidget * widget, gpointer data);

G_END_DECLS

#endif

