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
#include <stdint.h>
#include "main.h"
#include "time.h"
#include "dbus/dbus.h"

#include "localisation.h"
#include "settings.h"
#include "filters.h"
#include "he-about-dialog.h"

#define CSD_CALL_BUS_NAME	"com.nokia.csd.Call"
#define CSD_CALL_INTERFACE	"com.nokia.csd.Call"
#define CSD_CALL_INSTANCE	"com.nokia.csd.Call.Instance"
#define CSD_CALL_CONFERENCE	"com.nokia.csd.Call.Conference"
#define CSD_CALL_PATH		"/com/nokia/csd/call"


static time_t date_to_epoch(gint year, gint month, gint day, gboolean start_of_day)
{
	time_t t;
	struct tm time_str;
	time_str.tm_year = year - 1900;
	time_str.tm_mon = month;
	time_str.tm_mday = day;
	if(start_of_day)
	{
		time_str.tm_hour = 0;
		time_str.tm_min = 0;
		time_str.tm_sec = 1;
	}
	else
	{
		time_str.tm_hour = 23;
		time_str.tm_min = 59;
		time_str.tm_sec = 59;
	}
	time_str.tm_isdst = 0;

	t = mktime(&time_str);
	g_debug("time is %d", t);
	return t;
}

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



static int send_method_call(const char *dest, const char *path,
				const char *interface, const char *method,
				DBusPendingCallNotifyFunction cb,
				void *user_data, int type, ...)
{
	/*
	 * ret = send_method_call(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
				CSD_CALL_INTERFACE, "CreateWith",
				NULL, NULL,
				DBUS_TYPE_STRING, &number,
				DBUS_TYPE_UINT32, &flags,
				DBUS_TYPE_INVALID);

#define CSD_CALL_BUS_NAME	"com.nokia.csd.Call"
#define CSD_CALL_INTERFACE	"com.nokia.csd.Call"
#define CSD_CALL_INSTANCE	"com.nokia.csd.Call.Instance"
#define CSD_CALL_CONFERENCE	"com.nokia.csd.Call.Conference"
#define CSD_CALL_PATH		"/com/nokia/csd/call"
	 */
	DBusError * dberror = NULL;
	DBusConnection * connection = dbus_bus_get(DBUS_BUS_SYSTEM, dberror);
	if(dberror != NULL)
	{
		printf("dbus error\n");
		printf("%s", dberror->message);
		printf("\n");
	}
	DBusMessage *msg;
	DBusPendingCall *call;
	va_list args;

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		printf("unable to create dbus message");
		error("Unable to allocate new D-Bus %s message", method);

		return -1;
	}

	va_start(args, type);

	if (!dbus_message_append_args_valist(msg, type, args)) {
		dbus_message_unref(msg);
		va_end(args);
		return -1;
	}

	va_end(args);

	if (!cb) {
		printf("dbus send\n");
		dbus_connection_send(connection, msg, NULL);
		return 0;
	}

	if (!dbus_connection_send_with_reply(connection, msg, &call, -1)) {
		error("Sending %s failed", method);
		dbus_message_unref(msg);
		return -1;
	}

	dbus_pending_call_set_notify(call, cb, user_data, NULL);
	dbus_pending_call_unref(call);
	dbus_message_unref(msg);
	dbus_connection_unref(connection);
	connection = NULL;

	return 0;
}

static void make_call(GtkButton* button, gpointer data)
{

	int ret;
	uint32_t flag = 0;
	char * number = data;
	printf("make call to %s", number);
	printf("\n");

	ret = send_method_call(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
					CSD_CALL_INTERFACE, "CreateWith",
					NULL, NULL,
					DBUS_TYPE_STRING, &number,
					DBUS_TYPE_INT32, &flag,
					DBUS_TYPE_INVALID);
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
	GtkWidget *banner = hildon_banner_show_information(NULL, NULL,
							"Call Deleted from Call Log");

}

static void show_contact(GtkButton* button, gpointer data)
{
	/*
	 * I'd prefer to have the contact come up in a dialog popup but for some reason
	 * the below code doesn't display the dialog properly.
	GtkWidget *dialog;
    OssoABookContact *contact = data;
    GtkWidget *starter;
    starter = GTK_WIDGET(osso_abook_touch_contact_starter_new_with_contact(NULL, contact));
    dialog = GTK_WIDGET(osso_abook_touch_contact_starter_dialog_new(NULL, starter));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(GTK_WIDGET(dialog));
    */

    GtkWidget *starter, *window;
    OssoABookContact *contact = data;
    starter = osso_abook_touch_contact_starter_new_with_contact(NULL, contact);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_add (GTK_CONTAINER (window), starter);

    gtk_widget_show (starter);
    gtk_widget_show (window);

}

static void delete_button_clicked (GtkButton* button, gpointer data)
{
	g_print("button clicked\n");
	GtkWidget *detailsWindow = GTK_WIDGET(data);
	gtk_widget_destroy(detailsWindow);

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

	GtkWidget * timestamp_label, *end_timestamp_label;
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
    gint end_timestamp;
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
    gchar end_time_str[256];

    GtkTreeIter iter;
    RTComLogModel *model = gtk_tree_view_get_model(tree_view);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL(model), &iter,
              RTCOM_LOG_VIEW_COL_TEXT, &text,
              RTCOM_LOG_VIEW_COL_CONTACT, &contact,
              RTCOM_LOG_VIEW_COL_REMOTE_NAME, &remote_name,
              RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &remote_uid,
              RTCOM_LOG_VIEW_COL_TIMESTAMP, &timestamp,
              RTCOM_LOG_VIEW_COL_END_TIMESTAMP, &end_timestamp,
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

    if(end_timestamp != 0)
    {
    	get_time_string (end_time_str, 256, end_timestamp);
    	g_debug("end_time : %s",  end_time_str);
    }

	g_printf("remote_name %s ", remote_name);
    g_printf("remote_account %s ", remote_account);
    g_printf("local_account %s ", local_account);
    g_printf("remote_uid %s ", remote_uid);
    g_printf("end_timestamp %d", end_timestamp);
    g_debug("group_uid %s", group_uid);
    g_debug("group_title %s", group_title);
    g_printf("service %s", service);
    g_printf("event_type %s", event_type);
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
	if(end_timestamp != 0)
	{
		gint length = end_timestamp - timestamp;
		if(length > 0)
		{
			gint minutes = length / 60;
			gint seconds = length - (minutes * 60);

			gchar *length_str;
			if(length < 60)
				length_str = g_strdup_printf(" for 0m %ds", length);
			else
				length_str = g_strdup_printf(" for %dm %ds", minutes, seconds);

			end_timestamp_label = gtk_label_new(length_str);
			gtk_box_pack_start(GTK_BOX(timestamp_box), end_timestamp_label, FALSE, FALSE, 0);
			if(length_str != NULL)
				g_free (length_str);
		}
	}


	service_icon_widget = gtk_image_new_from_pixbuf(service_icon);

	gchar * remote_str;
	gboolean is_contact = FALSE;
	if(remote_name == NULL || (*remote_name == '\0'))
	{
		g_printf("remote_name is null\n");
		if(remote_uid == NULL || (*remote_uid == '\0'))
			remote_str = g_strdup_printf("Unknown Number");
		else
			remote_str = g_strdup_printf("%s", remote_account);
	}
	else
	{
		g_printf("remote_name is not null\n");
		is_contact = TRUE;
		remote_str = g_strdup_printf("%s (%s)", remote_name, remote_account);
	}

	remote_details = gtk_label_new (remote_str);
	gtk_box_pack_start(GTK_BOX(remote_box), service_icon_widget, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(remote_box), remote_details, FALSE, FALSE, 0);

	gchar * local_str = g_strdup_printf("Account : (%s)", local_account);
	local_details = gtk_label_new (local_str);
	gtk_box_pack_start(GTK_BOX(local_box), local_details, FALSE, FALSE, 0);

	gtk_box_set_spacing(GTK_BOX(button_box), 10);
	delete_button = gtk_button_new_with_label("Delete");
	hildon_gtk_widget_set_theme_size(delete_button, HILDON_SIZE_FINGER_HEIGHT);
	g_signal_connect(
	          G_OBJECT(delete_button),
	          "clicked",
	          G_CALLBACK(delete_record),
	          event_id);
	g_signal_connect(
		          G_OBJECT(delete_button),
		          "clicked",
		          G_CALLBACK(delete_button_clicked),
		          detailsWindow);
	gtk_box_pack_start(GTK_BOX(button_box), delete_button, FALSE, FALSE, 0);
	gtk_widget_show(delete_button);

	if(strcmp(local_account, "ring/tel/ring") == 0 && (*remote_account != '\0'))
	{
		call_button = gtk_button_new_with_label(" Call Number ");

		hildon_gtk_widget_set_theme_size(call_button, HILDON_SIZE_FINGER_HEIGHT);
		g_signal_connect(
				G_OBJECT(call_button),
				"clicked",
				G_CALLBACK(make_call),
				remote_account);

		gtk_box_pack_start(GTK_BOX(button_box), call_button, FALSE, FALSE, 0);
		gtk_widget_show(call_button);
	}

	/*only display contact button if it's a contact*/
	if(contact != NULL)
	{
		contact_button = gtk_button_new_with_label("Open Contact Card");
		g_signal_connect(
				  G_OBJECT(contact_button),
				  "clicked",
				  G_CALLBACK(show_contact),
				  contact);
		gtk_box_pack_start(GTK_BOX(button_box), contact_button, FALSE, FALSE, 0);
		gtk_widget_show(contact_button);

	}

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
	g_free (local_str);
	if(remote_str != NULL)
		g_free (remote_str);



	if (contact != NULL)
		g_object_unref (contact);

	data->showing_details = FALSE;
}

void row_tapped(GtkTreeView *tree_view, GtkTreePath *path, gpointer user_data)
{
	row_activated(tree_view, path, NULL, user_data);
}

void cursor_changed(GtkTreeView *tree_view, gpointer user_data)
{
	AppData * appdata = user_data;
	GtkTreePath *path = 0;
	GdkRectangle rect;
	gint x = 0;
	gint y = 0;

	gtk_tree_view_get_cursor(tree_view,&path,NULL);
	gtk_tree_view_get_background_area (GTK_TREE_VIEW (tree_view),
	                                    path, NULL, &rect);
	gtk_tree_view_convert_bin_window_to_tree_coords (GTK_TREE_VIEW (tree_view),
	                                                  0, rect.y, NULL, &y);
	hildon_pannable_area_scroll_to ((HildonPannableArea *)appdata->scrolled_window, x, y);
	gtk_tree_path_free (path);

}

static void aboutButton_clicked (GtkButton* button, gpointer data)
{
	AppData * appdata = data;
	he_about_dialog_present(GTK_WINDOW(appdata->mainWindow),
	                        "Extended Call Log",
	                        NULL,
	                        APP_VER,
	                        "View your complete call history",
	                        "(c) 2010 Thom Troy",
	                        "http://extcalllog.garage.maemo.org",
	                        "https://bugs.maemo.org/enter_bug.cgi?product=Extended%20Call%20Log",
	                        "https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=3S5DWH4C95UG8");


}

static void toggle_date_filter (GtkButton* button, gpointer data)
{
	AppData *appdata = data;

	if(hildon_check_button_get_active(HILDON_CHECK_BUTTON(button)))
	{
		g_debug("enabling date filter\n");
		appdata->filter_by_date = TRUE;
		return;
	}

	g_debug("disabling date filter\n");
	appdata->filter_by_date = FALSE;

}


static void start_date_changed (HildonPickerButton* button, gpointer data)
{
	AppData *appdata = data;
	gint day = 0;
	gint month = 0;
	gint year = 0;

	hildon_date_button_get_date (HILDON_DATE_BUTTON(button), &year, &month, &day);
	appdata->start_day = day;
	appdata->start_month = month;
	appdata->start_year = year;
	g_debug("year %d, month %d, day %d", year, month, day);

}

static void end_date_changed (HildonPickerButton* button, gpointer data)
{
	AppData *appdata = data;
	gint day = 0;
	gint month = 0;
	gint year = 0;

	hildon_date_button_get_date (HILDON_DATE_BUTTON(button), &year, &month, &day);
	appdata->end_day = day;
	appdata->end_month = month;
	appdata->end_year = year;
	g_debug("year %d, month %d, day %d", year, month, day);

}

static void date_dialog_response (GtkDialog *dialog,
                                     gint response_id, gpointer user_data)
{
	AppData *appdata = user_data;
	time_t end_date;
	time_t start_date;
	if(appdata->filter_by_date)
	{
		g_debug("we will filter by date");
		start_date = date_to_epoch(appdata->start_year, appdata->start_month, appdata->start_day, TRUE);

		if(appdata->end_year != 0)
			end_date = date_to_epoch(appdata->end_year, appdata->end_month, appdata->end_day, FALSE);
		else
			end_date = time(NULL);

		g_debug("start date is %d", start_date);
		g_debug("end date is %d", end_date);

		if(start_date > end_date)
		{
			g_debug("start date greater than end date");
			GtkWidget *banner = hildon_banner_show_information(GTK_WIDGET(appdata->mainWindow), NULL,
					"Start Date must be before End date. Disabling filter");
			appdata->filter_by_date = FALSE;
			gtk_widget_destroy(GTK_WIDGET(dialog));
			return;
		}
		else
		{
			g_debug("end date greater than start date so can continue");
			appdata->start_date = (int)start_date;
			appdata->end_date = (int)end_date;
			filter_by_date(appdata);

		}
	}
	else
	{
		g_debug("we won't filter by date");
		filter_by_date(appdata);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void date_dialog(GtkButton* button, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *box;

    GtkWidget *date_toggle_button;
    GtkWidget *start_date_button;
    GtkWidget *end_date_button;

    AppData *appdata = data;



    dialog = gtk_dialog_new_with_buttons("Date",
			appdata->mainWindow,
			GTK_DIALOG_MODAL,
			GTK_STOCK_OK,
			GTK_RESPONSE_OK,
			NULL);

    box = GTK_DIALOG(dialog)->vbox;



    start_date_button = hildon_date_button_new(HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    hildon_button_set_title(HILDON_BUTTON(start_date_button), "Start Date");
    if(appdata->start_year != 0)
    {
    	hildon_date_button_set_date(HILDON_DATE_BUTTON(start_date_button),
    			appdata->start_year, appdata->start_month, appdata->start_day);
    }
    else
    {
    	hildon_date_button_get_date (HILDON_DATE_BUTTON(start_date_button), &appdata->start_year,
    			&appdata->start_month, &appdata->start_day);
    }

    g_signal_connect(
        	G_OBJECT(start_date_button),
            "value-changed",
            G_CALLBACK(start_date_changed),
            appdata);
    gtk_widget_show(start_date_button);

    end_date_button = hildon_date_button_new(HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    hildon_button_set_title(HILDON_BUTTON(end_date_button), "End Date");
    if(appdata->end_year != 0)
    {
    	hildon_date_button_set_date(HILDON_DATE_BUTTON(end_date_button),
    			appdata->end_year, appdata->end_month, appdata->end_day);
    }
    else
    {
     	hildon_date_button_get_date (HILDON_DATE_BUTTON(end_date_button), &appdata->end_year,
     			&appdata->end_month, &appdata->end_day);
    }

    g_signal_connect(
            G_OBJECT(end_date_button),
            "value-changed",
            G_CALLBACK(end_date_changed),
            appdata);
    gtk_widget_show(end_date_button);

    date_toggle_button = hildon_check_button_new(HILDON_SIZE_AUTO );
    gtk_button_set_label(GTK_BUTTON(date_toggle_button), "Filter By Date");

    if(appdata->filter_by_date)
    	hildon_check_button_set_active(HILDON_CHECK_BUTTON(date_toggle_button), TRUE);
    else
    	hildon_check_button_set_active(HILDON_CHECK_BUTTON(date_toggle_button), FALSE);

    hildon_gtk_widget_set_theme_size(date_toggle_button, HILDON_SIZE_FINGER_HEIGHT);
    gtk_widget_show(date_toggle_button);

    g_signal_connect(
    		G_OBJECT(date_toggle_button),
            "toggled",
            G_CALLBACK(toggle_date_filter),
            appdata);

    gtk_box_pack_start (GTK_BOX (box), date_toggle_button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), start_date_button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), end_date_button, FALSE, FALSE, 0);




    /*GtkWidget *content_area = GTK_DIALOG(dialog)->vbox;
    GtkWidget *content_widget = settings_widget_create(GTK_WINDOW(dialog));*/

    /* Add the widget to the dialog
    gtk_box_pack_start(GTK_BOX(content_area), content_widget, TRUE, TRUE, 10);*/

    /* When a button (ok/cancel/etc.) is clicked or the dialog is closed - destroy it */
    g_signal_connect(dialog, "response", G_CALLBACK(date_dialog_response), appdata);

    gtk_dialog_run(GTK_DIALOG(dialog));

}



static void call_type_dialog(GtkButton* button, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *box;

    GtkWidget *all_button;
    GtkWidget *voip_button;
    GtkWidget *gsm_button;

    AppData *appdata = data;



    dialog = gtk_dialog_new_with_buttons("Call Type",
			appdata->mainWindow,
			GTK_DIALOG_MODAL,
			GTK_STOCK_OK,
			GTK_RESPONSE_OK,
			NULL);

    box = GTK_DIALOG(dialog)->vbox;

    all_button = hildon_gtk_radio_button_new(HILDON_SIZE_AUTO , NULL);
    gtk_button_set_label(GTK_BUTTON(all_button), "All Types");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(all_button), FALSE);
    hildon_gtk_widget_set_theme_size(all_button, HILDON_SIZE_FINGER_HEIGHT);
    gtk_widget_show(all_button);

    g_signal_connect(
    		G_OBJECT(all_button),
            "clicked",
            G_CALLBACK(all_call_types),
            appdata);

    voip_button = hildon_gtk_radio_button_new_from_widget(HILDON_SIZE_AUTO , GTK_RADIO_BUTTON(all_button));
    gtk_button_set_label(GTK_BUTTON(voip_button), "VoIP");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(voip_button), FALSE);
    hildon_gtk_widget_set_theme_size(voip_button, HILDON_SIZE_FINGER_HEIGHT);
    gtk_widget_show(voip_button);

    g_signal_connect(
        	G_OBJECT(voip_button),
            "clicked",
            G_CALLBACK(voip_calls),
            appdata);

    gsm_button = hildon_gtk_radio_button_new_from_widget(HILDON_SIZE_AUTO , GTK_RADIO_BUTTON(voip_button));
    gtk_button_set_label(GTK_BUTTON(gsm_button), "GSM");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(gsm_button), FALSE);
    hildon_gtk_widget_set_theme_size(gsm_button, HILDON_SIZE_FINGER_HEIGHT);
    gtk_widget_show(gsm_button);

    g_signal_connect(
        	G_OBJECT(gsm_button),
            "clicked",
            G_CALLBACK(gsm_calls),
            appdata);

    switch(appdata->current_type)
    {
		case ALL:
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(all_button), TRUE);
			break;
		}
		case VOIP:
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(voip_button), TRUE);
			break;
		}
		case GSM:
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gsm_button), TRUE);
			break;
		}
    }

    gtk_box_pack_start (GTK_BOX (box), all_button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), voip_button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), gsm_button, FALSE, FALSE, 0);


    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

    gtk_dialog_run(GTK_DIALOG(dialog));

}

void wizard_response(GtkDialog *dialog,
        gint       response_id,
        gint *response)
{
	*response = response_id;

}


static gboolean
some_page_func (GtkNotebook *nb,
                gint current,
                gpointer data)
{
	g_debug("checking page");
	GtkWidget *entry;
	entry = gtk_notebook_get_nth_page (nb, current);
	GList *children = gtk_container_get_children(GTK_CONTAINER(entry));

	/* Validate data and if valid set the app settings*/
	switch (current) {
		case 0:
		{
			g_debug("page 0");
			HildonEntry *limit;
			limit= HILDON_ENTRY(g_list_nth_data(children, 1));


			const gchar *limit_s = hildon_entry_get_text (HILDON_ENTRY(limit));
			char *pstr = limit_s;
			g_debug("checking int");
			while (*pstr) {
			    if (!g_ascii_isdigit(*pstr) )
			    {
			    	if(*pstr != '-')
			    	{
			    		g_debug("false %s", pstr);
			    		return FALSE;
			    	}
			    }
			    pstr++;
			}

			g_debug("casting to int");
			gint tmp = atoi(limit_s);
			g_debug("setting limit");
			set_limit(tmp);
			g_debug("limit set");


		}
	}

	return TRUE;
}

void all_default (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}
	AppData* appdata = data;
	appdata->settings.default_type = ALL;
	set_default_type(ALL);
}

void voip_default (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}
	AppData* appdata = data;
	appdata->settings.default_type = VOIP;
	set_default_type(VOIP);
}

void gsm_default (GtkButton* button, gpointer data)
{
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
	{
		return;
	}
	AppData* appdata = data;
	appdata->settings.default_type = GSM;
	set_default_type(GSM);

}
create_settings_wizard(AppData *appdata)
{
	g_debug("creating settings...");
	GtkWidget *wizard, *notebook;
	GtkWidget *limit_label;
	GtkWidget *desc_label;
	GtkWidget *limit_entry;
	GtkWidget *all_button, *voip_button, *gsm_button;
	GtkWidget *main_vbox;
	GtkWidget *type_vbox;
	GtkWidget *done_desc_label;
	GtkWidget *done_label;
	GtkWidget *type_label;

	notebook = gtk_notebook_new ();

	main_vbox = gtk_vbox_new(FALSE, 8);
	type_vbox = gtk_vbox_new(FALSE, 8);


	/*
	 * Setup the limit page
	 */
	limit_label = gtk_label_new ("Limit");
	desc_label = gtk_label_new ("Number of calls to display.\n-1 for No Limit");

	limit_entry = hildon_entry_new (HILDON_SIZE_AUTO);
	g_object_set (G_OBJECT (limit_entry), "hildon-input-mode", HILDON_GTK_INPUT_MODE_NUMERIC, NULL);
	/*
	 * Set Default or already set values
	 */
	g_debug("getting limit");
	gint limit = get_limit();
	gchar * limit_string = g_strdup_printf("%d", limit);


	hildon_entry_set_text (HILDON_ENTRY(limit_entry), limit_string);
	g_free(limit_string);
	g_debug("default limit set");

	gtk_box_pack_start (GTK_BOX (main_vbox), limit_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_vbox), limit_entry, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (main_vbox), desc_label, FALSE, FALSE, 0);

	/*
	 * default type page
	 */
	type_label = gtk_label_new ("Default Call Type");

	all_button = hildon_gtk_radio_button_new(HILDON_SIZE_AUTO , NULL);
	gtk_button_set_label(GTK_BUTTON(all_button), "All Types");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(all_button), FALSE);
	hildon_gtk_widget_set_theme_size(all_button, HILDON_SIZE_FINGER_HEIGHT);
	gtk_widget_show(all_button);

	g_signal_connect(
	 		G_OBJECT(all_button),
	        "clicked",
	        G_CALLBACK(all_default),
	        appdata);

	voip_button = hildon_gtk_radio_button_new_from_widget(HILDON_SIZE_AUTO , GTK_RADIO_BUTTON(all_button));
	gtk_button_set_label(GTK_BUTTON(voip_button), "VoIP");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(voip_button), FALSE);
	hildon_gtk_widget_set_theme_size(voip_button, HILDON_SIZE_FINGER_HEIGHT);
	gtk_widget_show(voip_button);

	g_signal_connect(
	     	G_OBJECT(voip_button),
	        "clicked",
	        G_CALLBACK(voip_default),
	        appdata);

	gsm_button = hildon_gtk_radio_button_new_from_widget(HILDON_SIZE_AUTO , GTK_RADIO_BUTTON(voip_button));
	gtk_button_set_label(GTK_BUTTON(gsm_button), "GSM");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(gsm_button), FALSE);
	hildon_gtk_widget_set_theme_size(gsm_button, HILDON_SIZE_FINGER_HEIGHT);
	gtk_widget_show(gsm_button);

    g_signal_connect(
    		G_OBJECT(gsm_button),
	        "clicked",
	        G_CALLBACK(gsm_default),
	        appdata);

	switch(appdata->settings.default_type)
	{
		case ALL:
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(all_button), TRUE);
			break;
		}
		case VOIP:
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(voip_button), TRUE);
			break;
		}
		case GSM:
		{
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gsm_button), TRUE);
			break;
		}
	}
	gtk_box_pack_start (GTK_BOX (type_vbox), all_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (type_vbox), voip_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (type_vbox), gsm_button, FALSE, FALSE, 0);


	/*
	 * Done page
	 */
	done_label = gtk_label_new("Finished");
	done_desc_label = gtk_label_new(
				"Your settings are now configured.");

	/* Append pages */
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), main_vbox, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), type_vbox, type_label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), done_desc_label, done_label);

	wizard = hildon_wizard_dialog_new(GTK_WINDOW(appdata->mainWindow), "Settings", GTK_NOTEBOOK(notebook));

	gint response = 0;
	g_signal_connect (G_OBJECT (wizard),
		                  "response",
		                  G_CALLBACK (wizard_response),
		                  &response);

	/* Set a function to decide if user can go to next page  */
	hildon_wizard_dialog_set_forward_page_func (HILDON_WIZARD_DIALOG (wizard),
	                                            some_page_func, appdata, NULL);

	g_debug("showing settings...");
	gtk_widget_show_all (wizard);
	gtk_dialog_run (GTK_DIALOG (wizard));
	gtk_widget_destroy(GTK_WIDGET(wizard));
	return response;
}

static void
settings_clicked(
        GtkWidget * widget,
        gpointer user_data)
{
    AppData * appdata = user_data;

    g_debug("Create Settings...");
    create_settings_wizard(appdata);
    g_debug("Refreshing...");
    rtcom_log_model_set_limit(appdata->log_model, get_limit());
    rtcom_log_model_refresh(appdata->log_model);
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
	appdata.filter_by_date = FALSE;
	appdata.start_day = 0;
	appdata.start_month = 0;
	appdata.start_year = 0;
	appdata.end_day = 0;
	appdata.end_month = 0;
	appdata.end_year = 0;



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
    GtkWidget * call_type_button = NULL;
    GtkWidget * date_button = NULL;
    GtkWidget * settings_button = NULL;
    GtkWidget * about_button = NULL;
    HildonAppMenu *menu;

    appdata.scrolled_window = NULL;

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
    appdata.settings.default_type = get_default_type();
    appdata.current_type = appdata.settings.default_type;
    appdata.current_direction = ALL_DIRECTIONS;

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
    /*
    const gchar * services[] = {"RTCOM_EL_SERVICE_CALL", NULL};

    g_debug("Populating calls...");

    rtcom_log_model_populate(appdata.log_model, services);
    */
    GtkWidget *banner = hildon_banner_show_information(GTK_WIDGET(appdata.mainWindow), NULL,
    					"Loading calls may take time. Please be patient");
    populate_calls_default(&appdata);

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
              &appdata);
    hildon_app_menu_append (menu, GTK_BUTTON (refresh_button));
    gtk_widget_show(refresh_button);

    settings_button = gtk_button_new_with_label("Settings");
    g_signal_connect(
             G_OBJECT(settings_button),
              "clicked",
              G_CALLBACK(settings_clicked),
              &appdata);
    hildon_app_menu_append (menu, GTK_BUTTON (settings_button));
    gtk_widget_show(settings_button);

    /*setup the filters for the app menu*/
    all_button = hildon_gtk_radio_button_new (HILDON_SIZE_AUTO, NULL);
    gtk_button_set_label (GTK_BUTTON (all_button), "All");
    g_signal_connect(
              G_OBJECT(all_button),
              "clicked",
              G_CALLBACK(all_call_directions),
              &appdata);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (all_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (all_button), FALSE);

    dialed_button = hildon_gtk_radio_button_new_from_widget (HILDON_SIZE_AUTO, GTK_RADIO_BUTTON(all_button));
    gtk_button_set_label (GTK_BUTTON (dialed_button), "Out");
    g_signal_connect(
              G_OBJECT(dialed_button),
              "clicked",
              G_CALLBACK(dialed_calls),
              &appdata);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (dialed_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (dialed_button), FALSE);

	recieved_button = hildon_gtk_radio_button_new_from_widget (HILDON_SIZE_AUTO, GTK_RADIO_BUTTON(dialed_button));
	gtk_button_set_label (GTK_BUTTON (recieved_button), "In");
    g_signal_connect(
              G_OBJECT(recieved_button),
              "clicked",
              G_CALLBACK(recieved_calls),
              &appdata);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (recieved_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (recieved_button), FALSE);

    missed_button = hildon_gtk_radio_button_new_from_widget (HILDON_SIZE_AUTO, GTK_RADIO_BUTTON(recieved_button));
    gtk_button_set_label (GTK_BUTTON (missed_button), "Missed");
    g_signal_connect(
              G_OBJECT(missed_button),
              "clicked",
              G_CALLBACK(missed_calls),
              &appdata);
    hildon_app_menu_add_filter (menu, GTK_BUTTON (missed_button));
    gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (missed_button), FALSE);


    call_type_button = gtk_button_new_with_label("Call Type");
    g_signal_connect(
              G_OBJECT(call_type_button),
              "clicked",
              G_CALLBACK(call_type_dialog),
              &appdata);
    hildon_app_menu_append (menu, GTK_BUTTON (call_type_button));
    gtk_widget_show(call_type_button);


    date_button = gtk_button_new_with_label("Date");
    g_signal_connect(
              G_OBJECT(date_button),
              "clicked",
              G_CALLBACK(date_dialog),
              &appdata);
    hildon_app_menu_append (menu, GTK_BUTTON (date_button));
    gtk_widget_show(date_button);

    about_button = gtk_button_new_with_label("About");
    g_signal_connect(
             G_OBJECT(about_button),
             "clicked",
              G_CALLBACK(aboutButton_clicked),
              &appdata);
    hildon_app_menu_append (menu, GTK_BUTTON (about_button));
    gtk_widget_show(about_button);

    gtk_widget_show_all (GTK_WIDGET (menu));
    hildon_window_set_app_menu (HILDON_WINDOW (appdata.mainWindow), menu);

    /*gtk_box_pack_start(GTK_BOX(box), buttons_box, FALSE, FALSE, 0);*/

	/* setup the pannable area to show the calls */
    box = gtk_vbox_new(FALSE, 10);
    appdata.scrolled_window = hildon_pannable_area_new();
    hildon_pannable_area_add_with_viewport((HildonPannableArea *)appdata.scrolled_window, appdata.log_view);

    gtk_box_pack_start(GTK_BOX(box), appdata.scrolled_window, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(box), appdata.search_bar, FALSE, FALSE, 0);
    gtk_container_add(
                GTK_CONTAINER(appdata.mainWindow),
                box);

    /*g_signal_connect(G_OBJECT(appdata.log_view), "row-activated",
    		G_CALLBACK(row_activated), &appdata);*/

    g_signal_connect(G_OBJECT(appdata.log_view), "hildon-row-tapped",
       		G_CALLBACK(row_tapped), &appdata);

    g_signal_connect(G_OBJECT(appdata.log_view), "cursor-changed",
       		G_CALLBACK(cursor_changed), &appdata);


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
