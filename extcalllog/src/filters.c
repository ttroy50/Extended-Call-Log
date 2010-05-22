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

#include "filters.h"
#include "settings.h"
#include "main.h"
#include <rtcom-eventlogger/eventlogger.h>
#include <libebook/e-book.h>
#include <libosso-abook/osso-abook.h>
#include "rtcom-eventlogger-ui/rtcom-log-view.h"
#include "rtcom-eventlogger-ui/rtcom-log-model.h"
#include "rtcom-eventlogger-ui/rtcom-log-columns.h"
#include "rtcom-eventlogger-ui/rtcom-log-search-bar.h"


void query_prepare(RTComElQuery* query, gpointer data)
{
	gchar * eventtype = NULL;
	gint outgoing = -1;
	AppData *appdata = data;


	if(appdata->filter_by_date)
	{
		//disable the limit filter because we want to get all calls between certain dates
		rtcom_log_model_set_limit(appdata->log_model, -1);
	}
	else
	{
		//re-enable the limit filter
		rtcom_log_model_set_limit(appdata->log_model, appdata->settings.limit);
	}

	switch(appdata->current_direction){
		case INBOUND:
		{
			g_debug("inbound");
			eventtype = "RTCOM_EL_EVENTTYPE_CALL";
			outgoing = 0;
			break;
		}
		case OUTBOUND:
		{
			g_debug("outbound");
			eventtype = "RTCOM_EL_EVENTTYPE_CALL";
			outgoing = 1;
			break;
		}
		case MISSED:
		{
			g_debug("missed");
			eventtype = "RTCOM_EL_EVENTTYPE_CALL_MISSED";
			outgoing = 0;
			break;
		}
		default:
			eventtype = NULL;
	}


	if(eventtype == NULL)
	{
		g_debug("eventype is null so all call types displayed\n");
		if(appdata->current_type == GSM)
		{
			if(appdata->filter_by_date)
			{
				rtcom_el_query_prepare(query,
				   	"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
				   	"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
				   	"start-time", appdata->start_date, RTCOM_EL_OP_GREATER,
				   	"start-time", appdata->end_date, RTCOM_EL_OP_LESS,
				   	NULL);
			}
			else
			{
				rtcom_el_query_prepare(query,
				 	"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
				  	"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
				  	NULL);
			}

		}
		else if(appdata->current_type == VOIP)
		{
			if(appdata->filter_by_date)
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"local-uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
					"start-time", appdata->start_date, RTCOM_EL_OP_GREATER,
					"start-time", appdata->end_date, RTCOM_EL_OP_LESS,
					NULL);
			}
			else
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"local-uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
					NULL);
			}
		}
		else
		{
			if(appdata->filter_by_date)
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"start-time", appdata->start_date, RTCOM_EL_OP_GREATER,
				   	"start-time", appdata->end_date, RTCOM_EL_OP_LESS,
					NULL);
			}
			else
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					NULL);
			}
		}
	}
	else
	{
		g_debug("eventtype set\n");
		g_debug("event type is %s", eventtype);
		g_debug("\n");
		if(appdata->current_type == GSM)
		{
			if(appdata->filter_by_date)
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"event-type", eventtype, RTCOM_EL_OP_EQUAL,
					"outgoing", outgoing, RTCOM_EL_OP_EQUAL,
					"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
					"start-time", appdata->start_date, RTCOM_EL_OP_GREATER,
				   	"start-time", appdata->end_date, RTCOM_EL_OP_LESS,
					NULL);
			}
			else
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"outgoing", outgoing, RTCOM_EL_OP_EQUAL,
					"event-type", eventtype, RTCOM_EL_OP_EQUAL,
					"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
					NULL);
			}
		}
		else if(appdata->current_type == VOIP)
		{
			if(appdata->filter_by_date)
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"event-type", eventtype, RTCOM_EL_OP_EQUAL,
					"outgoing", outgoing, RTCOM_EL_OP_EQUAL,
					"local-uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
					"start-time", appdata->start_date, RTCOM_EL_OP_GREATER,
				 	"start-time", appdata->end_date, RTCOM_EL_OP_LESS,
					NULL);
			}
			else
			{
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"event-type", eventtype, RTCOM_EL_OP_EQUAL,
					"outgoing", outgoing, RTCOM_EL_OP_EQUAL,
					"local-uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
					NULL);
			}
		}
		else
		{
			if(appdata->filter_by_date)
			{
				g_debug("creating date filter where event type set and call type all");
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"outgoing", outgoing, RTCOM_EL_OP_EQUAL,
					"event-type", eventtype, RTCOM_EL_OP_EQUAL,
					"start-time", appdata->start_date, RTCOM_EL_OP_GREATER,
				 	"start-time", appdata->end_date, RTCOM_EL_OP_LESS,
					NULL);

			}
			else
			{
				g_debug("creating filter where event type set and call type all");
				rtcom_el_query_prepare(query,
					"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
					"outgoing", outgoing, RTCOM_EL_OP_EQUAL,
					"event-type", eventtype, RTCOM_EL_OP_EQUAL,
					NULL);
			}
		}
	}
}

void missed_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(button))
	{
		return;
	}

	AppData *appdata = data;
	appdata->current_direction = MISSED;

    g_debug("missed calls...");

    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);
    /*rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"event-type", "RTCOM_EL_EVENTTYPE_CALL_MISSED", RTCOM_EL_OP_EQUAL,
    		NULL);*/
    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void recieved_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}

	AppData *appdata = data;
	appdata->current_direction = INBOUND;
    g_debug("recieved calls...");
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);
    /*rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"event-type", "RTCOM_EL_EVENTTYPE_CALL_INBOUND", RTCOM_EL_OP_EQUAL,
    		NULL);
    		*/
    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void dialed_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}

	AppData *appdata = data;
	appdata->current_direction = OUTBOUND;
    g_debug("dialed calls...");
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);
    /*rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"event-type", "RTCOM_EL_EVENTTYPE_CALL_OUTBOUND", RTCOM_EL_OP_EQUAL,
    		NULL);*/
    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void voip_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}

	AppData *appdata = data;
	appdata->current_type = VOIP;

    g_debug("voip calls...");
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);
    /*rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"local-uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
    		NULL);*/
    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void gsm_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}

	AppData *appdata = data;
	appdata->current_type = GSM;

    g_debug("gsm calls...");
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);
    /*rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
    		NULL);
    		*/
    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void all_call_types (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}

	AppData *appdata = data;
	appdata->current_type = ALL;

    g_debug("gsm calls...");
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);
    /*rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
    		NULL);
    		*/
    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void all_call_directions (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}

	AppData *appdata = data;
	appdata->current_direction = ALL_DIRECTIONS;

    g_debug("all directions...");
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);

    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void populate_calls(GtkWidget * widget, gpointer data)
{
	AppData *appdata = data;
    const gchar * services[] = {"RTCOM_EL_SERVICE_CALL", NULL};

    g_debug("Populating calls...");

    rtcom_log_model_populate(appdata->log_model, services);
}

void populate_calls_default(AppData* data)
{
	AppData *appdata = data;

	g_debug("populationg calls for first time");
	appdata->current_direction = ALL_DIRECTIONS;
	RTComElQuery *query = NULL;
	RTComEl * eventlogger = NULL;
	eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
	query = rtcom_el_query_new(eventlogger);

	query_prepare(query, appdata);

	rtcom_log_model_populate_query(appdata->log_model, query);
	g_object_unref(query);
}

void filter_by_date (gpointer data)
{
	AppData *appdata = data;

    g_debug("filtering by date");
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
    query = rtcom_el_query_new(eventlogger);

    query_prepare(query, appdata);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void refresh(GtkWidget * widget, gpointer data)
{
	AppData *appdata = data;

    g_debug("Refreshing...");

    rtcom_log_model_refresh(appdata->log_model);
}

