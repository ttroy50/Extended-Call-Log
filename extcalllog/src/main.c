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

/* Includes */
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
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include "main.h"
#include "time.h"
#include "dbus/dbus.h"

#include "localisation.h"

#define DEFAULT_LIMIT 30

static void
get_time_string (
    gchar     *time_str,
    size_t     length,
    time_t     timestamp)
{
    const gchar *time_format = NULL;
    const gchar *date_format = NULL;
    struct tm *loc_time;
    time_t t;
    gint year, month, day;
    gboolean skip_date;

    g_return_if_fail (time_str != NULL);

    t = time(NULL);
    loc_time = localtime(&t);
    year = loc_time->tm_year;
    day = loc_time->tm_mday;
    month = loc_time->tm_mon;

    loc_time = localtime(&timestamp);

    skip_date = ((loc_time->tm_year == year) &&
        (loc_time->tm_mon == month) &&
        (loc_time->tm_mday == day));

    if (!skip_date) {
        date_format = dgettext ("hildon-libs", "wdgt_va_date");
    }

    time_format = dgettext ("hildon-libs", "wdgt_va_24h_time");

    if (date_format) {
        gchar *full_format;

        full_format = g_strdup_printf ("%s | %s", date_format, time_format);

        strftime (time_str, length, full_format, loc_time);
        g_free (full_format);
    } else {
        strftime (time_str, length, time_format, loc_time);
    }
}

static void delete_record(GtkButton* button, gpointer data)
{
	g_debug("delete record...");
	gint id = (int) data;
	g_debug("id is %d", id);
	RTComEl *el = rtcom_el_new ();
	if(!RTCOM_IS_EL(el))
	{
		g_debug("couldn't setup RTComEl\n");
		return;
	}

	rtcom_el_delete_event(el, id, NULL);

}


void row_activated(GtkTreeView *tree_view, GtkTreePath *path,
    GtkTreeViewColumn *column, gpointer user_data)
{
    g_debug("row activated\n");
    AppData * data = user_data;

    HildonStackableWindow * detailsWindow;
	GtkWidget * main_box;
	GtkWidget * timestamp_box;
	GtkWidget * remote_box;
	GtkWidget * local_box;
	GtkWidget * button_box;

	GtkWidget * timestamp_label;
	GtkWidget * icon_widget, * remote_details;
	GtkWidget * service_icon_widget, * local_details;
	GtkWidget * delete_button, * call_button, * contact_button;


	/* Get the information we want to display from the model.
	 *
	 */
    OssoABookContact *contact;
    guint event_id;
    gchar *remote_name;
    gchar *remote_uid;
    gchar *local_account;
    gchar *remote_account;
    gchar *text;
    gint timestamp;
    gint count;
    gchar *group_uid;
    gchar *group_title;
    gchar *service;
    gchar * event_type;
    GdkPixbuf *icon;
    GdkPixbuf *service_icon;

    const gchar *title_col = NULL;
    const gchar *name_str = NULL;
    gchar time_str[256];

    GtkTreeIter iter;
    RTComLogModel *model = gtk_tree_view_get_model(tree_view);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
              RTCOM_LOG_VIEW_COL_TEXT, &text,
              RTCOM_LOG_VIEW_COL_CONTACT, &contact,
              RTCOM_LOG_VIEW_COL_REMOTE_NAME, &remote_name,
              RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &remote_uid,
              RTCOM_LOG_VIEW_COL_TIMESTAMP, &timestamp,
              RTCOM_LOG_VIEW_COL_COUNT, &count,
              RTCOM_LOG_VIEW_COL_GROUP_UID, &group_uid,
              RTCOM_LOG_VIEW_COL_GROUP_TITLE, &group_title,
              RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT, &local_account,
              RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &remote_account,
              RTCOM_LOG_VIEW_COL_EVENT_ID, &event_id,
              RTCOM_LOG_VIEW_COL_SERVICE, &service,
              RTCOM_LOG_VIEW_COL_EVENT_TYPE, &event_type,
              RTCOM_LOG_VIEW_COL_ICON, &icon,
              RTCOM_LOG_VIEW_COL_SERVICE_ICON, &service_icon,
              -1);

    get_time_string (time_str, 256, timestamp);
    g_debug("time : %s",  time_str);

	g_debug("remote_name %s", remote_name);
    g_debug("remote_account %s", remote_account);
    g_debug("local_account %s", local_account);
    g_debug("remote_uid %s", remote_uid);
    g_debug("group_uid %s", group_uid);
    g_debug("group_title %s", group_title);
    g_debug("service %s", service);
    g_debug("event_type %s", event_type);
    g_debug("count %d", count);
    g_debug("text %s", text);
    g_debug("event_id %d", event_id);



	/*
	 * Create and display the window showing the details of the call
	 */
    detailsWindow = HILDON_STACKABLE_WINDOW(hildon_stackable_window_new());

	main_box = gtk_vbox_new(FALSE, 10);
    timestamp_box = gtk_hbox_new(FALSE, 0);
	remote_box = gtk_hbox_new(FALSE, 0);
	local_box = gtk_hbox_new(FALSE, 0);
	button_box = gtk_hbox_new(FALSE, 0);

	icon_widget = gtk_image_new_from_pixbuf(icon);
	timestamp_label = gtk_label_new (time_str);
	gtk_box_pack_start(GTK_BOX(timestamp_box), icon_widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(timestamp_box), timestamp_label, FALSE, FALSE, 0);


	service_icon_widget = gtk_image_new_from_pixbuf(service_icon);
	gchar * remote_str = g_strdup_printf("%s (%s)", remote_name, remote_account);
	remote_details = gtk_label_new (remote_str);
	gtk_box_pack_start(GTK_BOX(remote_box), service_icon_widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(remote_box), remote_details, FALSE, FALSE, 0);
	g_free(remote_str);

	gchar * local_str = g_strdup_printf("Account : (%s)", local_account);
	local_details = gtk_label_new (local_str);
	gtk_box_pack_start(GTK_BOX(local_box), local_details, FALSE, FALSE, 0);


	delete_button = gtk_button_new_with_label("Delete");
	g_signal_connect(
	          G_OBJECT(delete_button),
	          "clicked",
	          G_CALLBACK(delete_record),
	          event_id);
	gtk_box_pack_start(GTK_BOX(button_box), delete_button, FALSE, FALSE, 0);
	gtk_widget_show(delete_button);

	call_button = gtk_button_new_with_label("Call");
	/*g_signal_connect(
	          G_OBJECT(all_button),
	          "clicked",
	          G_CALLBACK(delete_record),
	          event_id);*/
	/*gtk_box_pack_start(GTK_BOX(button_box), call_button, FALSE, FALSE, 0);
	gtk_widget_show(call_button);*/

	contact_button = gtk_button_new_with_label("Contact");
	/*g_signal_connect(
	          G_OBJECT(all_button),
	          "clicked",
	          G_CALLBACK(delete_record),
	          event_id);*/
	/*gtk_box_pack_start(GTK_BOX(button_box), contact_button, FALSE, FALSE, 0);
	gtk_widget_show(contact_button);*/

	gtk_box_pack_start(GTK_BOX(main_box), timestamp_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), remote_box, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(main_box), local_box, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);

	gtk_container_add(
                GTK_CONTAINER(detailsWindow),
                main_box);

    hildon_program_add_window(data->program, HILDON_WINDOW(detailsWindow));
    gtk_widget_show_all (GTK_WIDGET(detailsWindow));


	g_free (text);
	g_free (remote_name);
	g_free (remote_uid);
	g_free (group_uid);
	g_free (group_title);
	g_free (service);
	g_free (event_type);


	if (contact != NULL)
		g_object_unref (contact);

	data->showing_details = FALSE;
}

static void button_clicked (GtkButton* button, gpointer data)
{
    gtk_main_quit();
}

static gboolean set_limit(gint limit)
{
	/* Get the default client */
	GConfClient *client = gconf_client_get_default();

	gconf_client_add_dir (client, "/apps/extcalllog",
	                GCONF_CLIENT_PRELOAD_NONE, NULL);

	if(limit < -1 || limit == 0)
		limit = DEFAULT_LIMIT;

	return gconf_client_set_int (client,
			"/apps/extcalllog/limit",
            limit,
            NULL);
}

static gint get_limit()
{
	gint limit;
	/* Get the default client */
	GConfClient *client = gconf_client_get_default();

	/*Add GConf node if absent*/
	/*if(!gconf_client_dir_exists(client, GCONF_NODE, NULL))
	{*/
	gconf_client_add_dir (client, "/apps/extcalllog",
	                GCONF_CLIENT_PRELOAD_NONE, NULL);

	limit = gconf_client_get_int(client,"/apps/extcalllog/limit", NULL);

	if(limit < -1 || limit == 0)
	{
		limit = DEFAULT_LIMIT;
		set_limit(limit);
	}

	return limit;

}



static void missed_calls (GtkButton* button, gpointer data)
{
    g_debug("missed calls...");
	RTComLogModel * model = NULL;
    model = RTCOM_LOG_MODEL(data);
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(model);
    query = rtcom_el_query_new(eventlogger);
    rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"event-type", "RTCOM_EL_EVENTTYPE_CALL_MISSED", RTCOM_EL_OP_EQUAL,
    		NULL);

    rtcom_log_model_populate_query(model, query);
    g_object_unref(query);
}

static void recieved_calls (GtkButton* button, gpointer data)
{
    g_debug("recieved calls...");
	RTComLogModel * model = NULL;
    model = RTCOM_LOG_MODEL(data);
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(model);
    query = rtcom_el_query_new(eventlogger);
    rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"event-type", "RTCOM_EL_EVENTTYPE_CALL_INBOUND", RTCOM_EL_OP_EQUAL,
    		NULL);

    rtcom_log_model_populate_query(model, query);
    g_object_unref(query);
}

static void dialed_calls (GtkButton* button, gpointer data)
{
    g_debug("dialed calls...");
    RTComLogModel * model = NULL;
    model = RTCOM_LOG_MODEL(data);
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(model);
    query = rtcom_el_query_new(eventlogger);
    rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"event-type", "RTCOM_EL_EVENTTYPE_CALL_OUTBOUND", RTCOM_EL_OP_EQUAL,
    		NULL);

    rtcom_log_model_populate_query(model, query);
    g_object_unref(query);
}

static void voip_calls (GtkButton* button, gpointer data)
{
    g_debug("voip calls...");
    RTComLogModel * model = NULL;
    model = RTCOM_LOG_MODEL(data);
    RTComElQuery *query = NULL;
    RTComEl * eventlogger = NULL;
    eventlogger = rtcom_log_model_get_eventlogger(model);
    query = rtcom_el_query_new(eventlogger);
    rtcom_el_query_prepare(query,
    		"service", "RTCOM_EL_SERVICE_CALL", RTCOM_EL_OP_EQUAL,
    		"local_uid", "ring/tel/ring", RTCOM_EL_OP_NOT_EQUAL,
    		NULL);

    rtcom_log_model_populate_query(model, query);
    g_object_unref(query);
}

static void
populate_calls(
        GtkWidget * widget,
        gpointer data)
{
    RTComLogModel * model = NULL;
    const gchar * services[] = {"RTCOM_EL_SERVICE_CALL", NULL};

    g_debug("Populating calls...");

    model = RTCOM_LOG_MODEL(data);
    rtcom_log_model_populate(model, services);
}

static void
refresh(
        GtkWidget * widget,
        gpointer data)
{
    RTComLogModel * model = NULL;

    g_debug("Refreshing...");

    model = RTCOM_LOG_MODEL(data);
    rtcom_log_model_refresh(model);
}

static gint
dbus_callback (const gchar *interface, const gchar *method,
               GArray *arguments, gpointer data,
               osso_rpc_t *retval)
{
  printf ("dbus: %s, %s\n", interface, method);

  if (!strcmp (method, "top_application"))
      gtk_window_present (GTK_WINDOW (data));

  return DBUS_TYPE_INVALID;
}

int main( int argc, char* argv[] )
{
    /* Create needed variables */
	AppData appdata;
	appdata.static_group_by = RTCOM_EL_QUERY_GROUP_BY_NONE;
	appdata.showing_details = FALSE;

    osso_context_t *osso_cont;
	osso_return_t ret;

    RTComEl * eventlogger = NULL;
    OssoABookAggregator * aggr;
    GtkWidget * box = NULL;
    GtkWidget * refresh_button = NULL;
    GtkWidget * dialed_button = NULL;
	GtkWidget * recieved_button = NULL;
	GtkWidget * missed_button = NULL;
    GtkWidget * all_button = NULL;
    GtkWidget * voip_button = NULL;
    HildonAppMenu *menu;

    GtkWidget * scrolled_window = NULL;

	locale_init();

    osso_cont = osso_initialize(APP_NAME, APP_VER, TRUE, NULL);
	if (osso_cont == NULL)
    {
    	fprintf (stderr, "osso_initialize failed.\n");
    	exit (1);
    }

    /* Initialize the GTK. */
    gtk_init( &argc, &argv );

    /* Init abook */
    if (!osso_abook_init (&argc, &argv, osso_cont)) {
        g_critical ("Error initializing libosso-abook");
        osso_deinitialize (osso_cont);
        return 1;
    }

    gint limit = get_limit();
    /* Create the hildon program and setup the title */
    appdata.program = HILDON_PROGRAM(hildon_program_get_instance());
    g_set_application_name("Extended Call Log");

    /* Create HildonWindow and set it to HildonProgram */
    appdata.mainWindow = HILDON_STACKABLE_WINDOW(hildon_stackable_window_new());
    hildon_program_add_window(appdata.program, HILDON_WINDOW(appdata.mainWindow));



    appdata.log_model = rtcom_log_model_new();
    rtcom_log_model_set_limit(appdata.log_model, limit);
    aggr = OSSO_ABOOK_AGGREGATOR(osso_abook_aggregator_new(NULL, NULL));
    osso_abook_roster_start(OSSO_ABOOK_ROSTER(aggr));

    rtcom_log_model_set_abook_aggregator(appdata.log_model, aggr);
    appdata.log_view = rtcom_log_view_new();
    appdata.search_bar = rtcom_log_search_bar_new();

    rtcom_log_model_set_group_by(appdata.log_model, appdata.static_group_by);
    eventlogger = rtcom_log_model_get_eventlogger(appdata.log_model);
    const gchar * services[] = {"RTCOM_EL_SERVICE_CALL", NULL};

    g_debug("Populating calls...");

    rtcom_log_model_populate(appdata.log_model, services);
    rtcom_log_search_bar_set_model(
            RTCOM_LOG_SEARCH_BAR(appdata.search_bar),
            appdata.log_model);
    rtcom_log_search_bar_widget_hook(
            RTCOM_LOG_SEARCH_BAR(appdata.search_bar),
            GTK_WIDGET(appdata.mainWindow),
            GTK_TREE_VIEW(appdata.log_view));

    rtcom_log_view_set_model(
                RTCOM_LOG_VIEW(appdata.log_view),
                rtcom_log_search_bar_get_model_filter(
                    RTCOM_LOG_SEARCH_BAR(appdata.search_bar)));

	/* The view has acquired its own reference to the
     * model, so we can drop ours. That way the model will
     * be freed automatically when the view is destroyed.
     */
    g_debug("Unreffing the model, because the view reffed it.");
    g_object_unref(appdata.log_model);


	/* Setup the app menu */
    menu = HILDON_APP_MENU (hildon_app_menu_new ());

	refresh_button = gtk_button_new_with_label("Refresh");
    g_signal_connect(
             G_OBJECT(refresh_button),
              "clicked",
              G_CALLBACK(refresh),
              appdata.log_model);
    hildon_app_menu_append (menu, GTK_BUTTON (refresh_button));
    gtk_widget_show(refresh_button);

    /*setup the filters for the app menu*/
    all_button = hildon_gtk_radio_button_new (HILDON_SIZE_AUTO, NULL);
    gtk_button_set_label (GTK_BUTTON (all_button), "All Calls");
    g_signal_connect(
              G_OBJECT(all_button),
              "clicked",
              G_CALLBACK(populate_calls),
              appdata.log_model);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (all_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (all_button), FALSE);

    dialed_button = hildon_gtk_radio_button_new_from_widget (HILDON_SIZE_AUTO, GTK_RADIO_BUTTON(all_button));
    gtk_button_set_label (GTK_BUTTON (dialed_button), "Dialed");
    g_signal_connect(
              G_OBJECT(dialed_button),
              "clicked",
              G_CALLBACK(dialed_calls),
              appdata.log_model);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (dialed_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (dialed_button), FALSE);

	recieved_button = hildon_gtk_radio_button_new_from_widget (HILDON_SIZE_AUTO, GTK_RADIO_BUTTON(dialed_button));
	gtk_button_set_label (GTK_BUTTON (recieved_button), "Received");
    g_signal_connect(
              G_OBJECT(recieved_button),
              "clicked",
              G_CALLBACK(recieved_calls),
              appdata.log_model);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (recieved_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (recieved_button), FALSE);

    missed_button = hildon_gtk_radio_button_new_from_widget (HILDON_SIZE_AUTO, GTK_RADIO_BUTTON(recieved_button));
    gtk_button_set_label (GTK_BUTTON (missed_button), "Missed");
    g_signal_connect(
              G_OBJECT(missed_button),
              "clicked",
              G_CALLBACK(missed_calls),
              appdata.log_model);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (missed_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (missed_button), FALSE);


    /*voip_button = gtk_button_new_with_label("VoIP Calls");
    g_signal_connect(
              G_OBJECT(voip_button),
              "clicked",
              G_CALLBACK(voip_calls),
              appdata.log_model);
    hildon_app_menu_append (menu, GTK_BUTTON (voip_button));
    gtk_widget_show(voip_button);*/

    gtk_widget_show_all (GTK_WIDGET (menu));
    hildon_window_set_app_menu (HILDON_WINDOW (appdata.mainWindow), menu);

    /*gtk_box_pack_start(GTK_BOX(box), buttons_box, FALSE, FALSE, 0);*/

	/* setup the pannable area to show the calls */
    box = gtk_vbox_new(FALSE, 10);
    scrolled_window = hildon_pannable_area_new();

    hildon_pannable_area_add_with_viewport((HildonPannableArea *)scrolled_window, appdata.log_view);

    gtk_box_pack_start(GTK_BOX(box), scrolled_window, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(box), appdata.search_bar, FALSE, FALSE, 0);
    gtk_container_add(
                GTK_CONTAINER(appdata.mainWindow),
                box);

    g_signal_connect(G_OBJECT(appdata.log_view), "row-activated",
    		G_CALLBACK(row_activated), &appdata);


    /* Quit program when window is closed. */
    g_signal_connect (G_OBJECT (appdata.mainWindow), "delete_event",
		      G_CALLBACK (gtk_main_quit), NULL);

    /* Quit program when window is otherwise destroyed. */
    g_signal_connect (G_OBJECT (appdata.mainWindow), "destroy",
		      G_CALLBACK (gtk_main_quit), NULL);


    ret = osso_rpc_set_cb_f (osso_cont,
                           APP_SERVICE,
                           APP_METHOD,
                           APP_SERVICE,
                           dbus_callback, GTK_WIDGET( appdata.mainWindow ));
	if (ret != OSSO_OK) {
		fprintf (stderr, "osso_rpc_set_cb_f failed: %d.\n", ret);
	    exit (1);
	}

    /* Begin the main application */
    gtk_widget_show_all ( GTK_WIDGET ( appdata.mainWindow ) );
    gtk_main();

    /* Exit */
    return 0;
}
