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

#ifndef __RTCOM_LOG_SEARCH_BAR_H
#define __RTCOM_LOG_SEARCH_BAR_H

#include <gtk/gtk.h>

#include "rtcom-eventlogger-ui/rtcom-log-model.h"

G_BEGIN_DECLS

#define RTCOM_LOG_SEARCH_BAR_TYPE \
    (rtcom_log_search_bar_get_type ())

#define RTCOM_LOG_SEARCH_BAR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST \
     ((obj), RTCOM_LOG_SEARCH_BAR_TYPE, RTComLogSearchBar))

#define RTCOM_LOG_SEARCH_BAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST \
     ((klass), RTCOM_LOG_SEARCH_BAR_TYPE, RTComLogSearchBarClass))

#define RTCOM_IS_LOG_SEARCH_BAR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RTCOM_LOG_SEARCH_BAR_TYPE))

#define RTCOM_IS_LOG_SEARCH_BAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), RTCOM_LOG_SEARCH_BAR_TYPE))

typedef struct _RTComLogSearchBar RTComLogSearchBar;
struct _RTComLogSearchBar
{
    GtkToolbar parent;
};

typedef struct _RTComLogSearchBarClass RTComLogSearchBarClass;
struct _RTComLogSearchBarClass
{
    GtkToolbarClass parent_class;
};

GType
rtcom_log_search_bar_get_type(void) G_GNUC_CONST;

/**
 * Creates a new #RTComLogSearchBar.
 * @returns a new #RTComLogSearchBar
 */
GtkWidget *
rtcom_log_search_bar_new(void);

/**
 * Connects this search bar to a model.
 * @param sb The #RTComLogSearchBar
 * @param model The #model that will be filtered
 */
void
rtcom_log_search_bar_set_model(
        RTComLogSearchBar * sb,
        RTComLogModel * model);

/**
 * Gets the ModelFilter used internally.
 * @param sb The #RTComLogSearchBar object
 * @return The ModelFilter used internally
 */
GtkTreeModel *
rtcom_log_search_bar_get_model_filter(
        RTComLogSearchBar * sb);

/**
 * Hooks a widget to the search bar. The widget should be a GtkWindow
 * and the ket-press events will be captured.
 * @param sb The #RTComLogSearchBar object
 * @param hook_windget The widget to hook to
 * @param treeview The TreeView that the search bar will control (for
 * adjusting focused row)
 */
void
rtcom_log_search_bar_widget_hook(
        RTComLogSearchBar * sb,
        GtkWidget * hook_widget,
        GtkTreeView * treeview);


/**
 * Unhooks the widget from the search bar.
 */
void
rtcom_log_search_bar_widget_unhook(
        RTComLogSearchBar * sb);

/**
 * Prevent the widget from handling the key-press events.
 * @param sb The #RTComLogSearchBar object
 */
void
rtcom_log_search_bar_suspend(
        RTComLogSearchBar * sb);

/**
 * Resumes the widget handling the key-press events.
 * @param sb The #RTComLogSearchBar object
 */
void
rtcom_log_search_bar_resume(
        RTComLogSearchBar * sb);

/**
 * Shows the search bar.
 * @param sb The #RTComLogSearchBar object
 */
void
rtcom_log_search_bar_show(
        RTComLogSearchBar * sb);

/**
 * Hides the search bar.
 * @param sb The #RTComLogSearchBar object
 */
void
rtcom_log_search_bar_hide(
        RTComLogSearchBar * sb);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */
