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

