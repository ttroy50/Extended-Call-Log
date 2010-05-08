/**
 * Copyright (C) 2005-06 Nokia Corporation.
 * Contact: Salvatore Iovene <ext-salvatore.iovene@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "rtcom-eventlogger-ui/rtcom-log-view.h"
#include "rtcom-eventlogger-ui/rtcom-log-columns.h"

#include <hildon/hildon.h>
#include <gtk/gtkcellrenderer.h>

#include <libosso-abook/osso-abook-avatar.h>
#include <libosso-abook/osso-abook-contact.h>

#include <glib/gi18n.h>

#include <gconf/gconf-client.h>
//#include <clockcore-public.h>

#include <time.h>
#include <clockd/libtime.h>
#include <dbus/dbus.h>

#include <rtcom-eventlogger-plugins/chat.h>

#define CLOCK_GCONF_PATH "/apps/clock"
#define CLOCK_GCONF_IS_24H_FORMAT CLOCK_GCONF_PATH  "/time-format"

static void
rtcom_log_view_class_init(
        RTComLogViewClass * klass);

static void
rtcom_log_view_init(
        RTComLogView * log_view);

static void
rtcom_log_view_dispose(
        GObject * obj);

static void
rtcom_log_view_finalize(
        GObject * obj);

G_DEFINE_TYPE(RTComLogView, rtcom_log_view, GTK_TYPE_TREE_VIEW);

#define CELL_HEIGHT 70

#define RTCOM_LOG_VIEW_GET_PRIV(log_view) (G_TYPE_INSTANCE_GET_PRIVATE ((log_view), \
            RTCOM_LOG_VIEW_TYPE, RTComLogViewPrivate))

struct _cell_data
{
    GtkCellRenderer * renderer;
    GtkTreeCellDataFunc func;
};

typedef struct _RTComLogViewPrivate RTComLogViewPrivate;
struct _RTComLogViewPrivate
{
    GtkTreeModel * model;
    GtkTreeViewColumn * left_column;
    GtkTreeViewColumn * right_column;

    gulong before_row_inserted_handler;
    gulong after_row_inserted_handler;
    gulong row_changed_handler;
    gulong row_deleted_handler;
    gulong presence_need_redraw_handler;
    gulong avatar_need_redraw_handler;
    gboolean needs_adjustment;

    struct _cell_data presence_cell;
    struct _cell_data avatar_cell;

    /* For time display settings changed. */
    GConfClient *gconf_client;
    guint time_format_notify_id;
    gboolean use_24h;

    /* For TZ change dbus signals. */
    DBusConnection *dbus;

    GHashTable *text_cell_cache;
    GHashTable *presence_icon_cache;

    gint current_width;
    guint disposed : 1;
    gboolean highlight_new_events;

    /* A flag to control whether to show display names
     * (default) or uids/phone numbers. */
    gboolean show_display_names;

    /* User-requested fixed width or -1 to adjust
     * width dynamically. */
    gint fixed_width;
};

static void
_icon_cell_func
        (GtkTreeViewColumn *tree_column,
        GtkCellRenderer   *cell,
        GtkTreeModel      *tree_model,
        GtkTreeIter       *iter,
        gpointer           data);

static void
_presence_cell_func
        (GtkTreeViewColumn *tree_column,
        GtkCellRenderer   *cell,
        GtkTreeModel      *tree_model,
        GtkTreeIter       *iter,
        gpointer           data);

static void
_avatar_cell_func
        (GtkTreeViewColumn *tree_column,
        GtkCellRenderer   *cell,
        GtkTreeModel      *tree_model,
        GtkTreeIter       *iter,
        gpointer           data);

static void
_text_cell_func
        (GtkTreeViewColumn *tree_column,
        GtkCellRenderer   *cell,
        GtkTreeModel      *tree_model,
        GtkTreeIter       *iter,
        gpointer           data);

static void
_service_cell_func
        (GtkTreeViewColumn *tree_column,
        GtkCellRenderer   *cell,
        GtkTreeModel      *tree_model,
        GtkTreeIter       *iter,
        gpointer           data);

static void
_destroy_old_model(
        RTComLogViewPrivate *priv);

static void
rtcom_log_view_class_init(
        RTComLogViewClass * klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    g_type_class_add_private(object_class, sizeof(RTComLogViewPrivate));
    object_class->dispose = rtcom_log_view_dispose;
    object_class->finalize = rtcom_log_view_finalize;
}

static void
_time_format_changed_cb (
    GConfClient *client,
    guint        notify_id,
    GConfEntry  *entry,
    gpointer     data)
{
    RTComLogView * view = RTCOM_LOG_VIEW (data);
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV (view);
    gboolean new_use_24h = gconf_client_get_bool (priv->gconf_client,
        CLOCK_GCONF_IS_24H_FORMAT, NULL);

    if (new_use_24h == priv->use_24h)
        return;

    priv->use_24h = new_use_24h;

    g_debug ("%s: time format changed to %s, updating", G_STRFUNC,
        priv->use_24h ? "24h clock" : "12h clock");

    g_hash_table_remove_all (priv->text_cell_cache);
    gtk_widget_queue_draw (GTK_WIDGET (view));
}

static DBusHandlerResult _dbus_tz_change_cb (
    DBusConnection *conn,
    DBusMessage *msg,
    void *data)
{
    RTComLogView * view = RTCOM_LOG_VIEW (data);
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV (view);
    dbus_int32_t tm;

    /* Check we got the signal we asked for. */
    if (!dbus_message_is_signal (msg, CLOCKD_INTERFACE, CLOCKD_TIME_CHANGED))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_args (msg, NULL, DBUS_TYPE_INT32, &tm,
        DBUS_TYPE_INVALID))
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* Time argument is 0 for TZ changes, we're only interested in that  */
    if (tm != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    g_debug ("%s: called, clearing text cell cache", G_STRFUNC);

    tzset ();
    g_hash_table_remove_all (priv->text_cell_cache);
    gtk_widget_queue_draw (GTK_WIDGET (view));

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
_init_settings(RTComLogView *log_view)
{
    GError *err = NULL;
    DBusError derr;
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV (log_view);

    priv->gconf_client = gconf_client_get_default ();
    priv->time_format_notify_id = 0;
    priv->use_24h = TRUE;

    if (priv->gconf_client)
    {
        gconf_client_add_dir (priv->gconf_client, CLOCK_GCONF_PATH,
            GCONF_CLIENT_PRELOAD_NONE, NULL);

        priv->use_24h = gconf_client_get_bool (priv->gconf_client,
            CLOCK_GCONF_IS_24H_FORMAT, NULL);

        priv->time_format_notify_id = gconf_client_notify_add (priv->gconf_client,
            CLOCK_GCONF_IS_24H_FORMAT,
            _time_format_changed_cb,
            log_view, NULL, &err);

        if (err)
        {
            g_debug ("%s: error while setting 12/24h gconf notification",
                G_STRFUNC);
            priv->time_format_notify_id = 0;
        }
    }

    dbus_error_init (&derr);
    priv->dbus = dbus_bus_get (DBUS_BUS_SYSTEM, &derr);
    if (dbus_error_is_set (&derr))
    {
        g_debug ("%s: can't acquire DBus connection for TZ change listener",
            G_STRFUNC);
        dbus_error_free (&derr);
        priv->dbus = NULL;
    } else {
        dbus_bool_t ret;

        ret = dbus_connection_add_filter (priv->dbus,
            (DBusHandleMessageFunction) _dbus_tz_change_cb,
            (void *) log_view, NULL);

        if (!ret)
        {
            g_debug ("%s: can't add dbus filter", G_STRFUNC);
            dbus_connection_unref (priv->dbus);
            priv->dbus = NULL;
        }
        else
        {
            dbus_bus_add_match (priv->dbus,
                "type='signal',interface='" CLOCKD_INTERFACE
                "',member='" CLOCKD_TIME_CHANGED "'", NULL);
        }
    }
}

static void
_dispose_settings(RTComLogView *log_view)
{
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(log_view);

    if (priv->time_format_notify_id)
        gconf_client_notify_remove (priv->gconf_client,
            priv->time_format_notify_id);
    priv->time_format_notify_id = 0;

    if (priv->gconf_client)
        g_object_unref (priv->gconf_client);
    priv->gconf_client = NULL;

    if (priv->dbus)
    {
        dbus_connection_remove_filter (priv->dbus,
            (DBusHandleMessageFunction) _dbus_tz_change_cb,
            (void *) log_view);

        dbus_connection_unref (priv->dbus);
        priv->dbus = NULL;
    }
}

static void
size_allocate_cb (GtkWidget *view, GtkAllocation *allocation,
    gpointer user_data)
{
  RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(view);

  if ((priv->fixed_width == -1) &&
      (allocation->width != priv->current_width))
  {
      gtk_tree_view_column_set_fixed_width (priv->left_column,
          allocation->width - gtk_tree_view_column_get_fixed_width
             (priv->right_column));
      priv->current_width = allocation->width;
  }
}

static void
rtcom_log_view_init(
        RTComLogView * log_view)
{
    GtkTreeView         * tree_view = GTK_TREE_VIEW(log_view);
    GtkCellRenderer     * text_renderer         = NULL,
                        * icon_renderer         = NULL,
                        * service_icon_renderer = NULL;
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(log_view);

    priv->before_row_inserted_handler  = 0;
    priv->after_row_inserted_handler   = 0;
    priv->presence_need_redraw_handler = 0;
    priv->avatar_need_redraw_handler   = 0;
    priv->needs_adjustment             = FALSE;
    priv->current_width                = 0;
    priv->fixed_width                  = -1;
    priv->highlight_new_events         = TRUE;
    priv->show_display_names           = TRUE;

    priv->text_cell_cache = g_hash_table_new_full
        (NULL, NULL, NULL, g_free);
    priv->presence_icon_cache = g_hash_table_new_full
        (g_str_hash, g_str_equal, g_free, g_object_unref);

    priv->presence_cell.func           = _presence_cell_func;
    priv->avatar_cell.func             = _avatar_cell_func;

    icon_renderer                      = gtk_cell_renderer_pixbuf_new();
    text_renderer                      = gtk_cell_renderer_text_new();
    priv->presence_cell.renderer       = gtk_cell_renderer_pixbuf_new();
    service_icon_renderer              = gtk_cell_renderer_pixbuf_new();
    priv->avatar_cell.renderer         = gtk_cell_renderer_pixbuf_new();

    g_object_set (icon_renderer,
        "stock-size", HILDON_ICON_SIZE_FINGER,
        NULL);
    g_object_set (priv->presence_cell.renderer,
        "stock-size", HILDON_ICON_SIZE_XSMALL,
        NULL);
    g_object_set (service_icon_renderer,
        "stock-size", HILDON_ICON_SIZE_SMALL,
        NULL);

    gtk_tree_view_set_fixed_height_mode (tree_view, TRUE);
    priv->left_column = gtk_tree_view_column_new ();
    priv->right_column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_sizing (priv->left_column,
                                     GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_sizing (priv->right_column,
                                     GTK_TREE_VIEW_COLUMN_FIXED);

    gtk_tree_view_column_set_expand (priv->left_column, FALSE);
    gtk_tree_view_column_set_fixed_width
        (priv->right_column,
         RTCOM_LOG_VIEW_COL_PRESENCE_WIDTH +
         RTCOM_LOG_VIEW_COL_SERVICE_ICON_WIDTH +
         RTCOM_LOG_VIEW_COL_AVATAR_WIDTH +
         HILDON_MARGIN_DEFAULT);

    gtk_tree_view_insert_column (tree_view, priv->left_column, 0);
    gtk_tree_view_insert_column (tree_view, priv->right_column, 1);

    gtk_tree_view_column_pack_start (priv->left_column,
                                     icon_renderer, FALSE);
    gtk_tree_view_column_set_cell_data_func (priv->left_column,
                                             icon_renderer, _icon_cell_func,
                                             log_view, NULL);

    gtk_tree_view_column_pack_start (priv->left_column, text_renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func (priv->left_column,
                                             text_renderer, _text_cell_func,
                                             log_view, NULL);

    gtk_tree_view_column_pack_start (priv->right_column,
                                     priv->presence_cell.renderer, FALSE);
    gtk_tree_view_column_set_cell_data_func (priv->right_column,
                                             priv->presence_cell.renderer,
                                             priv->presence_cell.func,
                                             log_view, NULL);

    gtk_tree_view_column_pack_start (priv->right_column, service_icon_renderer,
                                     FALSE);
    gtk_tree_view_column_set_cell_data_func (priv->right_column,
                                             service_icon_renderer,
                                             _service_cell_func,
                                             log_view, NULL);

    gtk_tree_view_column_pack_start (priv->right_column,
                                     priv->avatar_cell.renderer, FALSE);
    gtk_tree_view_column_set_cell_data_func (priv->right_column,
                                             priv->avatar_cell.renderer,
                                             priv->avatar_cell.func,
                                             log_view, NULL);
    gtk_cell_renderer_set_fixed_size
        (icon_renderer,
         RTCOM_LOG_VIEW_COL_ICON_WIDTH, CELL_HEIGHT);
    gtk_cell_renderer_set_fixed_size
        (text_renderer,
         -1, CELL_HEIGHT);
    gtk_cell_renderer_set_fixed_size
        (priv->presence_cell.renderer,
         RTCOM_LOG_VIEW_COL_PRESENCE_WIDTH, CELL_HEIGHT);
    gtk_cell_renderer_set_fixed_size
        (service_icon_renderer,
         RTCOM_LOG_VIEW_COL_SERVICE_ICON_WIDTH, CELL_HEIGHT);
    gtk_cell_renderer_set_fixed_size
        (priv->avatar_cell.renderer,
         RTCOM_LOG_VIEW_COL_AVATAR_WIDTH, CELL_HEIGHT);

    gtk_tree_view_set_enable_search (GTK_TREE_VIEW (log_view), FALSE);
    gtk_tree_view_set_search_column (GTK_TREE_VIEW (log_view), -1);

    g_signal_connect (G_OBJECT (log_view), "size-allocate",
      (GCallback) size_allocate_cb, NULL);

    _init_settings(log_view);
}

static void
rtcom_log_view_dispose(
        GObject * obj)
{
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(obj);

    if (priv->disposed)
        return;

    priv->disposed = TRUE;

    _destroy_old_model (priv);
    _dispose_settings (RTCOM_LOG_VIEW (obj));

    g_hash_table_destroy (priv->text_cell_cache);
    g_hash_table_destroy (priv->presence_icon_cache);

    G_OBJECT_CLASS(rtcom_log_view_parent_class)->dispose(obj);
}

static void
rtcom_log_view_finalize(
        GObject * obj)
{
    G_OBJECT_CLASS(rtcom_log_view_parent_class)->finalize(obj);
}

GtkWidget *
rtcom_log_view_new(void)
{
    return g_object_new(RTCOM_LOG_VIEW_TYPE, NULL);
}

static void
_row_changed_cb(
        GtkTreeModel * model,
        GtkTreePath  * path,
        GtkTreeIter  * iter,
        gpointer       data)
{
    RTComLogView * view = RTCOM_LOG_VIEW(data);
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(view);

    g_hash_table_remove_all (priv->text_cell_cache);
}

static void
_row_deleted_cb(
        GtkTreeModel * model,
        GtkTreePath  * path,
        gpointer       data)
{
    RTComLogView * view = RTCOM_LOG_VIEW(data);
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(view);

    g_hash_table_remove_all (priv->text_cell_cache);
}

static void
_before_row_inserted(
        GtkTreeModel * model,
        GtkTreePath  * path,
        GtkTreeIter  * iter,
        gpointer       data)
{
    RTComLogView * view = RTCOM_LOG_VIEW(data);
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(view);
    GtkAdjustment * adj =
        gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(view));
    gdouble adj_value = gtk_adjustment_get_value(adj);

    if(adj_value <= 1.0e-6)
    {
        priv->needs_adjustment = TRUE;
    }
}

static void
_after_row_inserted(
        GtkTreeModel * model,
        GtkTreePath  * path,
        GtkTreeIter  * iter,
        gpointer     * data)
{
    RTComLogView * view = RTCOM_LOG_VIEW(data);
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(view);

    if(priv->needs_adjustment)
    {
	GtkAdjustment * adj =
	    gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(view));

	if (adj)
	    gtk_adjustment_set_value (adj, 0.0);

        priv->needs_adjustment = FALSE;
    }
}

static void
_presence_need_redraw(
        GtkTreeModel * model,
        gpointer view)
{
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(RTCOM_LOG_VIEW(view));
    gtk_tree_view_column_set_cell_data_func(
            priv->right_column,
            priv->presence_cell.renderer,
            priv->presence_cell.func,
            GTK_TREE_VIEW(view),
            NULL);
}

static void
_avatar_need_redraw(
        GtkTreeModel * model,
        gpointer view)
{
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(RTCOM_LOG_VIEW(view));
    gtk_tree_view_column_set_cell_data_func(
            priv->right_column,
            priv->avatar_cell.renderer,
            priv->avatar_cell.func,
            GTK_TREE_VIEW(view),
            NULL);
}

GtkTreeModel *
rtcom_log_view_get_model(
        RTComLogView * view)
{
    RTComLogViewPrivate * priv = NULL;

    g_return_val_if_fail(RTCOM_IS_LOG_VIEW(view), NULL);

    priv = RTCOM_LOG_VIEW_GET_PRIV(view);

    return GTK_TREE_MODEL(priv->model);
}

void
rtcom_log_view_set_model(
        RTComLogView * view,
        GtkTreeModel * model)
{
    RTComLogViewPrivate * priv = NULL;
    GtkTreeModel * child_model = NULL;

    g_return_if_fail(RTCOM_IS_LOG_VIEW(view));
    priv = RTCOM_LOG_VIEW_GET_PRIV(view);

    if(model == priv->model)
    {
        g_debug("Setting the same model? Returning.");
        return;
    }

    if(model && GTK_IS_TREE_MODEL(model))
    {
        if(GTK_IS_TREE_MODEL_FILTER(model))
            child_model = gtk_tree_model_filter_get_model(
                    GTK_TREE_MODEL_FILTER(model));

        g_debug("Reffing the model.");
        g_object_ref(model);

        _destroy_old_model (priv);

        if(model)
        {
            if(!g_signal_handler_is_connected(
                        G_OBJECT(model),
                        priv->before_row_inserted_handler))
            {
                g_debug("Connecting row-inserted...");
                priv->before_row_inserted_handler = g_signal_connect(
                        G_OBJECT(model),
                        "row-inserted",
                        (GCallback) _before_row_inserted,
                        view);
            }

            if(!g_signal_handler_is_connected(
                        G_OBJECT(model),
                        priv->after_row_inserted_handler))
            {
                g_debug("Connecting row-inserted after...");
                priv->after_row_inserted_handler = g_signal_connect_after(
                        G_OBJECT(model),
                        "row-inserted",
                        (GCallback) _after_row_inserted,
                        view);
            }
            if(!g_signal_handler_is_connected(
                        G_OBJECT(model),
                        priv->row_changed_handler))
            {
                g_debug("Connecting row-changed and row-deleted...");
                priv->row_changed_handler = g_signal_connect_after(
                        G_OBJECT(model),
                        "row-changed",
                        (GCallback) _row_changed_cb,
                        view);
                priv->row_deleted_handler = g_signal_connect(
                        G_OBJECT(model),
                        "row-deleted",
                        (GCallback) _row_deleted_cb,
                        view);
            }

            if(child_model)
            {
                if(!g_signal_handler_is_connected(
                            G_OBJECT(child_model),
                            priv->presence_need_redraw_handler))
                {
                    g_debug("Connecting presence-need-redraw...");
                    priv->presence_need_redraw_handler = g_signal_connect(
                            G_OBJECT(child_model),
                            "presence-need-redraw",
                            (GCallback) _presence_need_redraw,
                            view);
                }

                if(!g_signal_handler_is_connected(
                            G_OBJECT(child_model),
                            priv->avatar_need_redraw_handler))
                {
                    g_debug("Connecting avatar-need-redraw...");
                    priv->avatar_need_redraw_handler = g_signal_connect(
                            G_OBJECT(child_model),
                            "avatar-need-redraw",
                            (GCallback) _avatar_need_redraw,
                            view);
                }
            }
        }

        priv->model = model;

        gtk_tree_view_set_model(
            GTK_TREE_VIEW(view),
            priv->model);
    }
    else if(priv->model)
    {
        g_debug("Setting the model to NULL. Unreffing previous model.");
        g_object_unref(priv->model);
        priv->model = NULL;
        gtk_tree_view_set_model(
                GTK_TREE_VIEW(view),
                NULL);
    }
}

/* Some private functions */
static gboolean
is_group_chat (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
    gint flags;
    gboolean result;

    gtk_tree_model_get (tree_model, iter,
        RTCOM_LOG_VIEW_COL_FLAGS, &flags, -1);

    if (flags & RTCOM_EL_FLAG_CHAT_GROUP)
      {
        gchar *service;

        gtk_tree_model_get (tree_model, iter,
            RTCOM_LOG_VIEW_COL_SERVICE, &service, -1);

        if (strcmp (service, "RTCOM_EL_SERVICE_CHAT"))
            result = FALSE;
        else
            result = TRUE;

        g_free (service);
      }
    else
      {
        result = FALSE;
      }

    return result;
}

static void
_icon_cell_func(
        GtkTreeViewColumn * tree_column,
        GtkCellRenderer   * cell,
        GtkTreeModel      * tree_model,
        GtkTreeIter       * iter,
        gpointer            data)
{
    GdkPixbuf *icon = NULL;

    gtk_tree_model_get(
            tree_model,
            iter,
            RTCOM_LOG_VIEW_COL_ICON,
            &icon,
            -1);

    g_object_set (cell, "pixbuf", icon, NULL);
    if (icon)
        g_object_unref (icon);
}

static void
_presence_cell_func(
        GtkTreeViewColumn * tree_column,
        GtkCellRenderer   * cell,
        GtkTreeModel      * tree_model,
        GtkTreeIter       * iter,
        gpointer            data)
{
    GdkPixbuf * pixbuf = NULL;
    OssoABookPresence * presence = NULL;
    const gchar * presence_icon = NULL;
    RTComLogViewPrivate * priv;

    priv = RTCOM_LOG_VIEW_GET_PRIV(data);

    if (is_group_chat (tree_model, iter))
    {
        g_object_set(cell, "pixbuf", pixbuf, NULL);
        return;
    }

    gtk_tree_model_get (tree_model, iter,
                        RTCOM_LOG_VIEW_COL_CONTACT, &presence,
                        -1);

    if(presence)
        presence_icon = osso_abook_presence_get_icon_name(presence);

    if (presence_icon)
    {
        pixbuf = g_hash_table_lookup (priv->presence_icon_cache,
                presence_icon);
        if (!pixbuf)
        {
            pixbuf = gtk_icon_theme_load_icon
                (gtk_icon_theme_get_default (),
                 presence_icon,
                 HILDON_ICON_PIXEL_SIZE_XSMALL,
                 0, NULL);

            if (pixbuf)
                g_hash_table_insert (priv->presence_icon_cache,
                        g_strdup (presence_icon), pixbuf);
        }
    }

    g_object_set(
        cell,
        "pixbuf", pixbuf,
        NULL);

    if (presence)
        g_object_unref (presence);
}

static void
get_color_by_name(const gchar *colorname, gchar *buf)
{
    GdkColor color;
    GtkStyle *style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
          NULL, NULL, GTK_TYPE_LABEL);

    if (gtk_style_lookup_color (style, colorname, &color))
    {
        sprintf(buf, "#%02x%02x%02x",
            color.red / 256, color.green / 256, color.blue / 256);
    }
    else
    {
        buf[0] = '\0';
    }
}

static const gchar *
get_secondary_text_color()
{
    static gchar buf[40] = {0};

    if (G_UNLIKELY (buf[0] == '\0'))
        get_color_by_name ("SecondaryTextColor", buf);

    return buf;
}

static const gchar *
get_active_text_color()
{
    static gchar buf[40] = {0};

    if (G_UNLIKELY (buf[0] == '\0'))
        get_color_by_name ("ActiveTextColor", buf);

    return buf;
}

static const gchar *
get_default_text_color()
{
    static gchar buf[40] = {0};

    if (G_UNLIKELY (buf[0] == '\0'))
        get_color_by_name ("DefaultTextColor", buf);

    return buf;
}

static void
get_time_string (
    RTComLogView *log_view,
    gchar     *time_str,
    size_t     length,
    time_t     timestamp)
{
    RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV(log_view);
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

    if (priv->use_24h) {
        time_format = dgettext ("hildon-libs", "wdgt_va_24h_time");
    } else {
        if (loc_time->tm_hour > 11) {
            time_format = dgettext ("hildon-libs", "wdgt_va_12h_time_pm");
        } else {
            time_format = dgettext ("hildon-libs", "wdgt_va_12h_time_am");
        }
    }

    if (date_format) {
        gchar *full_format;

        full_format = g_strdup_printf ("%s | %s", date_format, time_format);

        strftime (time_str, length, full_format, loc_time);
        g_free (full_format);
    } else {
        strftime (time_str, length, time_format, loc_time);
    }
}

static void
_text_cell_func(
        GtkTreeViewColumn * tree_column,
        GtkCellRenderer   * cell,
        GtkTreeModel      * tree_model,
        GtkTreeIter       * iter,
        gpointer            data)
{
  OssoABookContact *contact;
  guint event_id;
  gchar *remote_name;
  gchar *remote_uid;
  gchar *text;
  gint timestamp;
  gint count;
  gchar *group_title;
  gchar *markup;
  gchar *count_str = NULL;
  const gchar *title_col = NULL;
  const gchar *name_str = NULL;
  gchar time_str[256];
  RTComLogViewPrivate * priv = RTCOM_LOG_VIEW_GET_PRIV (data);

  const gchar *pango_template_no_content = "<span foreground=\"%s\">%s%s</span> <span foreground=\"%s\" size=\"x-small\"><sup>(%s)</sup></span>\n"
      "<span foreground=\"%s\" size=\"x-small\"><sup>%s</sup></span>";
  /* sms layout */
  const gchar *pango_template_with_content = "<span foreground=\"%s\">%s%s</span>"
      " <span foreground=\"%s\" size=\"x-small\"><sup>%s</sup></span>\n"
      "<span foreground=\"%s\" size=\"x-small\">%s</span>";

  gtk_tree_model_get (tree_model, iter,
          RTCOM_LOG_VIEW_COL_EVENT_ID, &event_id,
          -1);

  markup = g_hash_table_lookup (priv->text_cell_cache, GUINT_TO_POINTER
      (event_id));
  if (markup)
  {
      g_object_set(cell, "markup", markup, NULL);
      return;
  }

  gtk_tree_model_get (tree_model, iter,
          RTCOM_LOG_VIEW_COL_TEXT, &text,
          RTCOM_LOG_VIEW_COL_CONTACT, &contact,
          RTCOM_LOG_VIEW_COL_REMOTE_NAME, &remote_name,
          RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &remote_uid,
          RTCOM_LOG_VIEW_COL_TIMESTAMP, &timestamp,
          RTCOM_LOG_VIEW_COL_COUNT, &count,
          RTCOM_LOG_VIEW_COL_GROUP_TITLE, &group_title,
          -1);

  if (count > 1)
      count_str = g_strdup_printf (" (%d)", count);

  if (is_group_chat (tree_model, iter))
      name_str = group_title;

  if (priv->show_display_names &&
      ((name_str == NULL) || (*name_str == '\0')))
    name_str = remote_name;

  if ((name_str == NULL) || (*name_str == '\0'))
      name_str = remote_uid;

  if ((name_str == NULL) || (*name_str == '\0'))
  {
      /* this is only needed in call-ui, when there's no text.
       * not a nice hack :( */
      if ((text == NULL) || (*text == '\0'))
          name_str = (const gchar *) dgettext("rtcom-call-ui",
              "voip_fi_caller_information_unknown_caller");
  }

  get_time_string (RTCOM_LOG_VIEW (data), time_str, 256, timestamp);

  if (priv->highlight_new_events && (count > 0))
      title_col = get_active_text_color ();
  else
      title_col = get_default_text_color ();

  if ((text == NULL) || (*text == '\0'))
  {
      markup = g_markup_printf_escaped (pango_template_no_content,
          title_col, name_str,
          (count_str != NULL) ? count_str : "",
          get_secondary_text_color (), remote_uid,
          get_secondary_text_color (), time_str);
  }
  else
  {
      gchar *tmp = g_strdup (text);
      gchar *x;

      for (x = tmp; *x; x++)
        if ((*x == '\r') || (*x == '\n')) *x = ' ';

      markup = g_markup_printf_escaped (pango_template_with_content,
          title_col, name_str,
          (count_str != NULL) ? count_str : "",
          get_secondary_text_color (), time_str,
          get_secondary_text_color (), tmp);

      g_free (tmp);
  }

  g_free (text);
  g_free (remote_name);
  g_free (remote_uid);
  g_free (group_title);
  g_free (count_str);

  if (contact != NULL)
      g_object_unref (contact);

  g_hash_table_insert (priv->text_cell_cache, GUINT_TO_POINTER (event_id),
      markup);
  g_object_set(cell, "markup", markup, NULL);
}

static void
_avatar_cell_func(
        GtkTreeViewColumn * tree_column,
        GtkCellRenderer   * cell,
        GtkTreeModel      * tree_model,
        GtkTreeIter       * iter,
        gpointer            data)
{
    OssoABookAvatar * avatar = NULL;
    GdkPixbuf * avatar_pixbuf = NULL;

    gtk_tree_model_get(
            tree_model,
            iter,
            RTCOM_LOG_VIEW_COL_CONTACT, &avatar,
            -1);

    if(is_group_chat(tree_model, iter))
    {
        static GdkPixbuf *muc_pixbuf = NULL;

        if (G_UNLIKELY (!muc_pixbuf))
            muc_pixbuf = gtk_icon_theme_load_icon
                (gtk_icon_theme_get_default (),
                 "general_conference_avatar",
                 HILDON_ICON_PIXEL_SIZE_FINGER,
                 0, NULL);

        avatar_pixbuf = muc_pixbuf;
    }
    else if(avatar)
    {
        avatar_pixbuf = g_object_get_data(
                G_OBJECT(avatar),
                "avatar-pixbuf");

        g_object_unref (avatar);
    }

    if (!avatar_pixbuf)
    {
        static GdkPixbuf *fallback = NULL;

        if (G_UNLIKELY (!fallback))
            fallback = gtk_icon_theme_load_icon
                (gtk_icon_theme_get_default (),
                 "general_default_avatar",
                 HILDON_ICON_PIXEL_SIZE_FINGER,
                 0, NULL);

        avatar_pixbuf = fallback;
    }

    g_object_set(cell, "pixbuf", avatar_pixbuf, NULL);
}

static void
_service_cell_func(
        GtkTreeViewColumn * tree_column,
        GtkCellRenderer   * cell,
        GtkTreeModel      * tree_model,
        GtkTreeIter       * iter,
        gpointer            data)
{
    GdkPixbuf *icon = NULL;

    gtk_tree_model_get(
            tree_model,
            iter,
            RTCOM_LOG_VIEW_COL_SERVICE_ICON,
            &icon,
            -1);

    g_object_set (cell, "pixbuf", icon, NULL);
    if (icon)
        g_object_unref (icon);
}

static void
_destroy_old_model (
        RTComLogViewPrivate *priv)
{
    if(priv->model)
    {
        GtkTreeModel * filter_model = NULL;

        if(GTK_IS_TREE_MODEL_FILTER(priv->model))
            filter_model = gtk_tree_model_filter_get_model(
                GTK_TREE_MODEL_FILTER(priv->model));

        if (priv->before_row_inserted_handler != 0)
        {
            g_signal_handler_disconnect (
                priv->model, priv->before_row_inserted_handler);
            priv->before_row_inserted_handler = 0;
        }
        if (priv->after_row_inserted_handler != 0)
        {
            g_signal_handler_disconnect (
                priv->model, priv->after_row_inserted_handler);
            priv->after_row_inserted_handler = 0;
        }
        if (priv->row_changed_handler != 0)
        {
            g_signal_handler_disconnect (
                priv->model, priv->row_changed_handler);
            priv->row_changed_handler = 0;
        }
        if (priv->row_deleted_handler != 0)
        {
            g_signal_handler_disconnect (
                priv->model, priv->row_deleted_handler);
            priv->row_deleted_handler = 0;
        }

        if (filter_model != NULL &&
            priv->presence_need_redraw_handler != 0)
        {
            g_signal_handler_disconnect (
                filter_model, priv->presence_need_redraw_handler);
            priv->presence_need_redraw_handler = 0;
        }

        if (filter_model != NULL &&
            priv->avatar_need_redraw_handler != 0)
        {
            g_signal_handler_disconnect (
                filter_model, priv->avatar_need_redraw_handler);
            priv->avatar_need_redraw_handler = 0;
        }

        g_debug("Unreffing the previous model..");
        g_object_unref(priv->model);

        /* Nullify it as well so the check in the beginning is valid */
        priv->model = NULL;
    }
}

void
rtcom_log_view_set_highlight_new_events (RTComLogView *view,
    gboolean highlight)
{
  RTComLogViewPrivate *priv;

  g_return_if_fail (RTCOM_IS_LOG_VIEW (view));
  priv = RTCOM_LOG_VIEW_GET_PRIV (view);

  priv->highlight_new_events = highlight;
  g_hash_table_remove_all (priv->text_cell_cache);
}

void
rtcom_log_view_set_show_group_chat (
        RTComLogView * view,
        gboolean is_shown)
{
    RTComLogViewPrivate *priv;

    g_return_if_fail (RTCOM_IS_LOG_VIEW (view));
    priv = RTCOM_LOG_VIEW_GET_PRIV (view);

    if (priv->model != NULL)
    {
        rtcom_log_model_set_show_group_chat (RTCOM_LOG_MODEL (priv->model),
            is_shown);
        g_hash_table_remove_all (priv->text_cell_cache);
    }
}

void
rtcom_log_view_set_fixed_width (
        RTComLogView *view,
        gint width)
{
    RTComLogViewPrivate *priv;

    g_return_if_fail (RTCOM_IS_LOG_VIEW (view));
    priv = RTCOM_LOG_VIEW_GET_PRIV (view);

    priv->fixed_width = width;

    if (priv->fixed_width != -1)
    {
        gtk_tree_view_column_set_fixed_width (priv->left_column,
           priv->fixed_width - gtk_tree_view_column_get_fixed_width
               (priv->right_column));
    }
}

void
rtcom_log_view_set_show_display_names (
        RTComLogView *view,
        gboolean show_display_names)
{
    RTComLogViewPrivate *priv;

    g_return_if_fail (RTCOM_IS_LOG_VIEW (view));
    priv = RTCOM_LOG_VIEW_GET_PRIV (view);

    priv->show_display_names = show_display_names;
    g_hash_table_remove_all (priv->text_cell_cache);
}

/* vim: set ai et tw=75 ts=4 sw=4: */
