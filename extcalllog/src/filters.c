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


void query_prepare(RTComElQuery* query, gint type, gint direction)
{
	gchar * eventtype = NULL;

	switch(direction){
		case INBOUND:
		{
			eventtype = "RTCOM_EL_EVENTTYPE_CALL_INBOUND";
			break;
		}
		case OUTBOUND:
		{
			eventtype = "RTCOM_EL_EVENTTYPE_CALL_OUTBOUND";
			break;
		}
		case MISSED:
		{
			eventtype = "RTCOM_EL_EVENTTYPE_CALL_MISSED";
			break;
		}
		default:
			eventtype = NULL;
	}


	if(eventtype == NULL)
	{
		printf("eventype is null so all call types displayed\n");
		if(type == GSM)
		{
			rtcom_el_query_prepare(query,
		    	"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
		    	"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
		    	NULL);
		}
		else if(type == VOIP)
		{
			rtcom_el_query_prepare(query,
			 	"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
			   	"local-uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
			   	NULL);
		}
		else
		{
			rtcom_el_query_prepare(query,
				"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
				NULL);
		}
	}
	else
	{
		printf("eventtype set\n");
		printf("event type is %s", eventtype);
		printf("\n");
		if(type == GSM)
		{
			rtcom_el_query_prepare(query,
		    	"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
		    	"event-type", eventtype, RTCOM_EL_OP_EQUAL,
		    	"local-uid", "ring/tel/ring", RTCOM_EL_OP_EQUAL,
		    	NULL);
		}
		else if(type == VOIP)
		{
			rtcom_el_query_prepare(query,
			 	"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
			 	"event-type", eventtype, RTCOM_EL_OP_EQUAL,
			   	"local-uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
			   	NULL);
		}
		else
		{
			rtcom_el_query_prepare(query,
				"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
				"event-type", eventtype, RTCOM_EL_OP_EQUAL,
				NULL);
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
    query_prepare(query, appdata->current_type, appdata->current_direction);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void recieved_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(button))
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
    query_prepare(query, appdata->current_type, appdata->current_direction);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void dialed_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(button))
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
    query_prepare(query, appdata->current_type, appdata->current_direction);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void voip_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(button))
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
    query_prepare(query, appdata->current_type, appdata->current_direction);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void gsm_calls (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(button))
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
    query_prepare(query, appdata->current_type, appdata->current_direction);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void all_call_types (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(button))
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
    query_prepare(query, appdata->current_type, appdata->current_direction);

    rtcom_log_model_populate_query(appdata->log_model, query);
    g_object_unref(query);
}

void all_call_directions (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(button))
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

    query_prepare(query, appdata->current_type, appdata->current_direction);

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
	RTComElQuery *query = NULL;
	RTComEl * eventlogger = NULL;
	eventlogger = rtcom_log_model_get_eventlogger(appdata->log_model);
	query = rtcom_el_query_new(eventlogger);

	query_prepare(query, appdata->current_type, ALL_DIRECTIONS);

	rtcom_log_model_populate_query(appdata->log_model, query);
	g_object_unref(query);
}

void refresh(GtkWidget * widget, gpointer data)
{
	AppData *appdata = data;

    g_debug("Refreshing...");

    rtcom_log_model_refresh(appdata->log_model);
}

