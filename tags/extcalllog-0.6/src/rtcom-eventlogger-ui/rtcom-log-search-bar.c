/**
 * Copyright (C) 2008 Nokia Corporation.
 * Contact: Naba Kumar <naba.kumar@nokia.com>
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

#include "rtcom-log-search-bar.h"
#include "rtcom-log-columns.h"
#include "utf8.h"

#include <hildon/hildon.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

G_DEFINE_TYPE(RTComLogSearchBar, rtcom_log_search_bar, GTK_TYPE_TOOLBAR);
#define RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE \
     ((o), RTCOM_LOG_SEARCH_BAR_TYPE, RTComLogSearchBarPrivate))

typedef struct _RTComLogSearchBarPrivate RTComLogSearchBarPrivate;
struct _RTComLogSearchBarPrivate
{
    GtkWidget          * entry;
    RTComLogModel      * model;
    GtkTreeModel       * model_filter;
    GtkWidget          * hook_widget;
    GtkTreeView        * treeview;

    /* Signals */
    gulong               key_press_id,
                         key_release_id,
                         realize_id,
                         unrealize_id,
                         focus_in_id,
                         focus_out_id;

    GtkIMContext       * im_context;
};


/*********************************************************************
 *                                                                   *
 * Private functions.                                                *
 *                                                                   *
 *********************************************************************/
static gchar *utf8_next_token (gchar *txt)
{
    if (txt == NULL)
        return NULL;

    /* find first space character */
    while ((*txt != '\0') && !g_unichar_isspace (g_utf8_get_char (txt)))
        txt = g_utf8_find_next_char (txt, NULL);

    /* skip (possibly several) space character(s) */
    while (g_unichar_isspace (g_utf8_get_char (txt)))
        txt = g_utf8_find_next_char (txt, NULL);

    if (*txt == '\0')
        return NULL;

    return txt;
}

static gssize get_needle_token_size (gchar *needle)
{
    gssize count = 0;

    while ((*needle != '\0') && !g_unichar_isspace (g_utf8_get_char (needle)))
    {
        count++;
        needle = g_utf8_find_next_char (needle, NULL);
    }

    return count;
}

static gboolean utf8_startswith (const gchar *haystack,
    const gchar *needle)
{
    while ((*haystack != '\0') && (*needle != '\0'))
    {
        if (g_utf8_get_char (haystack) != g_utf8_get_char (needle))
            return FALSE;

        haystack = g_utf8_next_char (haystack);
        needle = g_utf8_next_char (needle);
    }

    if (*needle != '\0')
        return FALSE;

    return TRUE;
}

gboolean
_visible_func(
        GtkTreeModel * model,
        GtkTreeIter * iter,
        gpointer data)
{
    RTComLogSearchBarPrivate * priv = data;
    const gchar * text;
    glong len = 0;
    gssize size;
    gchar * displayed_name;
    gboolean result = FALSE;
    gchar * haystack;
    gchar * haypart;
    gchar * needle;
    gchar * needle_part;
    gchar * needle_token;

    text = gtk_entry_get_text(GTK_ENTRY(priv->entry));
    len = g_utf8_strlen(text, -1);

    if(len == 0)
        return TRUE;

    gtk_tree_model_get(
            model, iter,
            RTCOM_LOG_VIEW_COL_REMOTE_NAME, &displayed_name,
            -1);

    if (!displayed_name)
        gtk_tree_model_get (model, iter, RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT,
            &displayed_name, -1);

    needle = g_utf8_strdown(text, -1);
    haystack = g_utf8_strdown(displayed_name, -1);
    g_free (displayed_name);

    /*tokenizing needle*/
    for (needle_part = needle;
        needle_part != NULL;
        needle_part = utf8_next_token (needle_part))
    {
        result = FALSE;
        size = get_needle_token_size (needle_part);
        needle_token = g_utf8_strdown (needle_part, size);

        for (haypart = haystack;
            haypart != NULL;
            haypart = utf8_next_token (haypart))
        {
            if (utf8_startswith (haypart, needle_token))
            {
                result = TRUE;
                break;
            }
        }
        g_free (needle_token);
        if (result == FALSE)
            break;
    }

    g_free (haystack);
    g_free (needle);

    return result;
}

static gboolean
_key_press_event_cb(
        GtkWidget * widget,
        GdkEventKey * key,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = NULL;
    GtkEditable * edit = NULL;
    gint start, end, pos;

    g_return_val_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb), FALSE);
    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);

    /* Ignore CTRL presses, except Ctrl+Space which is hw kb switch. */
    if ((key->state & GDK_CONTROL_MASK) &&
        (key->keyval != GDK_space))
      return FALSE;

    if(GTK_WIDGET_MAPPED(sb))
    {
        if(key->keyval == GDK_Escape)
            return TRUE;

        /* Take care to pass Ctrl+Space on. */
        if (key->keyval == GDK_Return ||
            (key->keyval == GDK_space &&
             (key->state & GDK_CONTROL_MASK)))
          return FALSE;

        if(key->keyval == GDK_BackSpace)
        {
            edit = GTK_EDITABLE(priv->entry);
            if(gtk_editable_get_selection_bounds(edit, &start, &end))
                gtk_editable_delete_text(edit, start, end);
            else
            {
                pos = gtk_editable_get_position(edit);
                if(pos < 1)
                    return TRUE;
                gtk_editable_delete_text(edit, pos - 1, pos);
            }

            return TRUE;
        }
    }

    /* Take care to pass Ctrl+Space on. */
    if(GTK_WIDGET_HAS_FOCUS(priv->entry) &&
        (key->keyval == GDK_space &&
         (key->state & GDK_CONTROL_MASK)))
      return FALSE;

    return gtk_im_context_filter_keypress(priv->im_context, key);;
}

GtkTreeModel *
rtcom_log_search_bar_get_model_filter(
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = NULL;

    g_return_val_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb), NULL);

    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    return priv->model_filter;
}

static gboolean
_key_release_event_cb(
        GtkWidget * widget,
        GdkEventKey * key,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = NULL;
    GtkEditable * edit = NULL;
    gchar * txt = NULL;

    g_return_val_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb), FALSE);
    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    edit = GTK_EDITABLE(priv->entry);

    if(key->state & GDK_CONTROL_MASK)
        return FALSE;

    if(GTK_WIDGET_MAPPED(sb))
    {
        if(key->keyval == GDK_Escape)
        {
            g_debug(G_STRLOC ": going to hide search bar...");
            rtcom_log_search_bar_hide(sb);
            return TRUE;
        }
        if(key->keyval == GDK_BackSpace)
        {
            txt = gtk_editable_get_chars(edit, 0, -1);
            if(!(txt && strlen(txt)))
            {
                rtcom_log_search_bar_hide(sb);
                return TRUE;
            }
        }
        /* removed for Bug 12102
        if(GTK_WIDGET_HAS_FOCUS(priv->entry))
            return FALSE;*/

        return gtk_im_context_filter_keypress(priv->im_context, key);
    }

    if(!GTK_WIDGET_MAPPED(priv->treeview))
        return FALSE;

    if(key->keyval == GDK_Return || key->keyval == GDK_space)
        return FALSE;

    gtk_im_context_filter_keypress(priv->im_context, key);

    return FALSE;
}

static void
_realize_cb(
        GtkWidget * hook_widget,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    gtk_im_context_set_client_window(
            priv->im_context,
            hook_widget->window);
}

static void
_unrealize_cb(
        GtkWidget * hook_widget,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    gtk_im_context_set_client_window(
            priv->im_context,
            NULL);
}

static gboolean
_focus_in_out_event_cb(
        GtkWidget * widget,
        GdkEventFocus * focus,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    if(focus->in)
        gtk_im_context_focus_in(priv->im_context);
    else
        gtk_im_context_focus_out(priv->im_context);

    return FALSE;
}

static void
_im_context_commit_cb(
        GtkIMContext * context,
        gchar * utf8,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = NULL;
    GtkEditable * edit = NULL;
    gint start, end, pos;
    gunichar c;

    g_return_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb));

    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    if(!GTK_WIDGET_MAPPED(sb))
    {
        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL (priv->model),
                NULL) == 0)
            return;

        c = g_utf8_get_char_validated(utf8, strlen(utf8));
        if(!g_unichar_isgraph(c))
            return;

        rtcom_log_search_bar_show(sb);
    }

    gtk_widget_grab_focus(priv->entry);

    edit = GTK_EDITABLE(priv->entry);
    if(gtk_editable_get_selection_bounds(edit, &start, &end))
        gtk_editable_delete_text(edit, start, end);

    pos = gtk_editable_get_position(edit);
    gtk_editable_insert_text(edit, utf8, strlen(utf8), &pos);
    gtk_editable_set_position(edit, pos);
}

static void
_unmap_cb(
        GtkWidget * widget,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    hildon_gtk_im_context_hide(priv->im_context);
}

static void
_hide_cb(
        GtkWidget * widget,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    gtk_editable_delete_text(GTK_EDITABLE(priv->entry), 0, -1);
}

static void
_entry_changed_cb(
        GtkEntry * entry,
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = NULL;

    g_return_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb));

    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    gtk_tree_model_filter_refilter(
            GTK_TREE_MODEL_FILTER(priv->model_filter));
}

/*********************************************************************
 *                                                                   *
 * GObject boilerplate.                                              *
 *                                                                   *
 *********************************************************************/
static void
rtcom_log_search_bar_dispose(
        GObject * obj)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(obj);

    if(priv->model)
    {
        g_debug(G_STRLOC ": unreffing the model...");
        g_object_unref(priv->model);
        priv->model = NULL;
    }

    if(priv->model_filter)
    {
        g_debug(G_STRLOC ": unreffing the model filter...");
        g_object_unref(priv->model_filter);
        priv->model_filter = NULL;
    }

    if(priv->im_context)
    {
        g_debug(G_STRLOC ": unreffing the IM context...");
        g_object_unref(priv->im_context);
        priv->im_context = NULL;
    }

    if (priv->treeview)
    {
        g_object_unref (priv->treeview);
        priv->treeview = NULL;
    }

    rtcom_log_search_bar_widget_unhook (RTCOM_LOG_SEARCH_BAR (obj));

    G_OBJECT_CLASS(rtcom_log_search_bar_parent_class)->dispose(obj);
}

static void
rtcom_log_search_bar_class_init(
        RTComLogSearchBarClass * klass)
{
    GObjectClass * object_class = G_OBJECT_CLASS(klass);
    g_type_class_add_private (klass, sizeof(RTComLogSearchBarPrivate));
    object_class->dispose = rtcom_log_search_bar_dispose;
}

static void
rtcom_log_search_bar_init(
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);
    const HildonGtkInputMode imode = HILDON_GTK_INPUT_MODE_FULL;
    GtkWidget *close_button_alignment;
    GtkToolItem *close_button_container;
    GtkWidget *close;
    GtkToolItem *close_button;
    GtkToolItem *entry_container;
    GtkWidget *entry_hbox;

    gtk_toolbar_set_style (GTK_TOOLBAR (sb), GTK_TOOLBAR_ICONS);
    gtk_container_set_border_width (GTK_CONTAINER (sb), 0);

    entry_container = gtk_tool_item_new ();
    gtk_tool_item_set_expand (entry_container, TRUE);

    entry_hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (entry_container), entry_hbox);

    priv->entry = hildon_entry_new (HILDON_SIZE_FINGER_HEIGHT);
    /* clear autocap and dictionary flags */
    hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry), imode);
    gtk_widget_set_name (GTK_WIDGET (priv->entry),
                         "RTComLogSearchEntry");

    gtk_box_pack_start (GTK_BOX (entry_hbox), priv->entry, TRUE, TRUE,
                        HILDON_MARGIN_DEFAULT);

    gtk_toolbar_insert (GTK_TOOLBAR (sb), entry_container, 0);

    close = gtk_image_new_from_icon_name ("general_close",
                                          HILDON_ICON_SIZE_FINGER);
    gtk_misc_set_padding (GTK_MISC (close), 0, 0);
    close_button = gtk_tool_button_new (close, NULL);
    GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_FOCUS);

    close_button_alignment = gtk_alignment_new (0.0f, 0.0f, 1.0f, 1.0f);
    gtk_alignment_set_padding (GTK_ALIGNMENT (close_button_alignment),
                               0, 0,
                               0, HILDON_MARGIN_DEFAULT);
    gtk_container_add (GTK_CONTAINER (close_button_alignment),
                       GTK_WIDGET (close_button));

    close_button_container = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (close_button_container),
                       close_button_alignment);

    gtk_toolbar_insert (GTK_TOOLBAR (sb), close_button_container, -1);

    g_signal_connect (G_OBJECT (priv->entry), "changed",
                      G_CALLBACK (_entry_changed_cb), sb);

    priv->im_context = gtk_im_multicontext_new();
    g_object_set (priv->im_context, "hildon-input-mode", imode, NULL);
    g_signal_connect (G_OBJECT (priv->im_context), "commit",
                      G_CALLBACK (_im_context_commit_cb), sb);


    g_signal_connect (G_OBJECT (sb), "unmap",
                      G_CALLBACK (_unmap_cb), sb);

    g_signal_connect (G_OBJECT (sb), "hide",
                      G_CALLBACK (_hide_cb), sb);

    g_signal_connect_swapped(G_OBJECT (close_button), "clicked",
        G_CALLBACK(rtcom_log_search_bar_hide), sb);
}

/*********************************************************************
 *                                                                   *
 * Public functions.                                                 *
 *                                                                   *
 *********************************************************************/
GtkWidget *
rtcom_log_search_bar_new(void)
{
    return g_object_new(RTCOM_LOG_SEARCH_BAR_TYPE, NULL);
}

void
rtcom_log_search_bar_set_model(
        RTComLogSearchBar * sb,
        RTComLogModel * model)
{
    RTComLogSearchBarPrivate * priv = NULL;

    g_return_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb));
    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);

    if (priv->model_filter)
    {
        g_object_unref (priv->model_filter);
        priv->model_filter = NULL;
    }

    if (priv->model)
    {
        g_object_unref (priv->model);
        priv->model = NULL;
    }

    if (model)
    {
        priv->model = g_object_ref (model);

        priv->model_filter = gtk_tree_model_filter_new(
                GTK_TREE_MODEL(priv->model), NULL);

        gtk_tree_model_filter_set_visible_func(
                GTK_TREE_MODEL_FILTER(priv->model_filter),
                _visible_func,
                priv,
                NULL);
    }
}

void
rtcom_log_search_bar_widget_hook(
        RTComLogSearchBar * sb,
        GtkWidget * hook_widget,
        GtkTreeView * treeview)
{
    RTComLogSearchBarPrivate * priv = NULL;

    g_return_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb));
    g_return_if_fail(GTK_IS_WIDGET(hook_widget));
    g_return_if_fail(GTK_IS_TREE_VIEW(treeview));

    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);

    if (priv->treeview != NULL)
    {
        g_object_unref (priv->treeview);
        priv->treeview = NULL;
    }

    priv->hook_widget = g_object_ref (hook_widget);
    priv->treeview    = g_object_ref (treeview);

    priv->key_press_id = g_signal_connect(
            priv->hook_widget, "key-press-event",
            G_CALLBACK(_key_press_event_cb), sb);

    priv->key_release_id = g_signal_connect(
            priv->hook_widget, "key-release-event",
            G_CALLBACK(_key_release_event_cb), sb);

    if(GTK_WIDGET_REALIZED(priv->hook_widget))
    {
        gtk_im_context_set_client_window(
                priv->im_context,
                priv->hook_widget->window);
        priv->realize_id = 0;
    }
    else
    {
        priv->realize_id = g_signal_connect(
                G_OBJECT(priv->hook_widget), "realize",
                G_CALLBACK(_realize_cb), sb);
    }

    priv->unrealize_id = g_signal_connect(
            G_OBJECT(priv->hook_widget), "unrealize",
            G_CALLBACK(_unrealize_cb), sb);

    priv->focus_in_id = g_signal_connect(
            G_OBJECT(priv->hook_widget), "focus-in-event",
            G_CALLBACK(_focus_in_out_event_cb), sb);

    g_debug(G_STRLOC ": widget hooked.");
}

void
rtcom_log_search_bar_widget_unhook(
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = NULL;

    g_return_if_fail(RTCOM_IS_LOG_SEARCH_BAR(sb));
    priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);

    g_debug ("%s: called", G_STRFUNC);

    if (priv->hook_widget == NULL)
        return;

    if(priv->key_press_id)
    {
        g_signal_handler_disconnect(priv->hook_widget, priv->key_press_id);
        priv->key_press_id = 0;
    }

    if(priv->key_release_id)
    {
        g_signal_handler_disconnect(priv->hook_widget, priv->key_release_id);
        priv->key_release_id = 0;
    }

    if(priv->realize_id)
    {
        g_signal_handler_disconnect(priv->hook_widget, priv->realize_id);
        priv->realize_id = 0;
    }

    if(priv->unrealize_id)
    {
        g_signal_handler_disconnect(priv->hook_widget, priv->unrealize_id);
        priv->unrealize_id = 0;
    }

    if(priv->focus_in_id)
    {
        g_signal_handler_disconnect(priv->hook_widget, priv->focus_in_id);
        priv->focus_in_id = 0;
    }

    if(priv->focus_out_id)
    {
        g_signal_handler_disconnect(priv->hook_widget, priv->focus_out_id);
        priv->focus_out_id = 0;
    }

    g_object_unref (priv->hook_widget);
    priv->hook_widget = NULL;

    g_debug(G_STRLOC ": widget unhooked.");
}


void
rtcom_log_search_bar_suspend(
        RTComLogSearchBar * sb)
{
}

void
rtcom_log_search_bar_resume(
        RTComLogSearchBar * sb)
{
}

void
rtcom_log_search_bar_show(
        RTComLogSearchBar * sb)
{
    RTComLogSearchBarPrivate * priv = RTCOM_LOG_SEARCH_BAR_GET_PRIVATE(sb);

    if(!GTK_WIDGET_MAPPED(priv->treeview))
            return;

    if(!GTK_WIDGET_MAPPED(sb))
    {
        gboolean no_show_all;

        g_object_get(sb, "no-show-all", &no_show_all, NULL);
        g_object_set(sb, "no-show-all", FALSE, NULL);

        gtk_widget_show_all(GTK_WIDGET(sb));
        gtk_widget_grab_focus(priv->entry);

        g_object_set(sb, "no-show-all", no_show_all, NULL);
    }
}

void
rtcom_log_search_bar_hide(
        RTComLogSearchBar * sb)
{
    gboolean no_show_all;

    if(!GTK_WIDGET_VISIBLE(sb))
        return;

    g_object_get(sb, "no-show-all", &no_show_all, NULL);
    g_object_set(sb, "no-show-all", FALSE, NULL);

    gtk_widget_hide_all(GTK_WIDGET(sb));

    g_object_set(sb, "no-show-all", no_show_all, NULL);
}


/* vim: set ai et tw=75 ts=4 sw=4: */
