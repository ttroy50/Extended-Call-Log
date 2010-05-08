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

/**
 * @file rtcom-log-view.h
 * @brief Defines an RTComLogView object.
 *
 * RTComLogView is a widget that shows events in the log..
 */

#ifndef __RTCOM_LOG_VIEW_H
#define __RTCOM_LOG_VIEW_H

#include <glib.h>
#include <gtk/gtk.h>

#include "rtcom-eventlogger-ui/rtcom-log-model.h"

G_BEGIN_DECLS

#define RTCOM_LOG_VIEW_TYPE            (rtcom_log_view_get_type ())
#define RTCOM_LOG_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RTCOM_LOG_VIEW_TYPE, RTComLogView))
#define RTCOM_LOG_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RTCOM_LOG_VIEW_TYPE, RTComLogViewClass))
#define RTCOM_IS_LOG_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RTCOM_LOG_VIEW_TYPE))
#define RTCOM_IS_LOG_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RTCOM_LOG_VIEW_TYPE))

typedef struct _RTComLogView RTComLogView;
struct _RTComLogView
{
    GtkTreeView parent;
};

typedef struct _RTComLogViewClass RTComLogViewClass;
struct _RTComLogViewClass
{
    GtkTreeViewClass parent_class;
};

GType rtcom_log_view_get_type(void) G_GNUC_CONST;

/**
 * Creates a new #RTComLogView.
 * @return a a newly allocated RTComLogView
 */
GtkWidget *
rtcom_log_view_new(void);

/**
 * Gets the internal #RTComLogModel
 * @param view The #RTComLogView
 * @return the internal #RTComLogModel
 */
GtkTreeModel *
rtcom_log_view_get_model(
        RTComLogView * view);

/**
 * Sets the model for the view.
 * @param The #RTComLogView
 * @param The #RTComLogModel
 */
void
rtcom_log_view_set_model(
        RTComLogView * view,
        GtkTreeModel * model);

/**
 * Sets whether the rows for new (unread/missed)
 * events should be highlighted (default TRUE).
 */
void
rtcom_log_view_set_highlight_new_events(
        RTComLogView * view,
        gboolean higlight);

/**
 * Sets whether the rows should show group chat
 * specific information (avatar, group_title)
 *
 * It should be called before model is added to the view
 */
void
rtcom_log_view_set_show_group_chat (
        RTComLogView * view,
        gboolean is_shown);

/**
 * Sets the width of the view widget to exactly
 * the specified width, ignoring screen size.
 *
 * This should be set as early as possible to avoid
 * flicker on the resize.
 */
void
rtcom_log_view_set_fixed_width (
        RTComLogView *view,
        gint width);

/**
 * Sets whether the rows should show contact display name
 * (the default) or remote uid / phone number.
 *
 * It should be called before model is added to the view
 */
void
rtcom_log_view_set_show_display_names (
        RTComLogView * view,
        gboolean show_display_names);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */
