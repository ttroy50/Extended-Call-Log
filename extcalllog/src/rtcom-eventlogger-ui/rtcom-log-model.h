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
 * @file rtcom-log-model.h
 * @brief Defines an RTComLogModel object.
 *
 * RTComLogModel is a widget that shows events in the log..
 */

#ifndef __RTCOM_LOG_MODEL_H
#define __RTCOM_LOG_MODEL_H

#include <glib.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkwidget.h>
#include <libebook/e-book.h>
#include <rtcom-eventlogger/eventlogger.h>
#include <libosso-abook/osso-abook-aggregator.h>

G_BEGIN_DECLS

#define RTCOM_LOG_MODEL_TYPE            (rtcom_log_model_get_type ())
#define RTCOM_LOG_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RTCOM_LOG_MODEL_TYPE, RTComLogModel))
#define RTCOM_LOG_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RTCOM_LOG_MODEL_TYPE, RtComLogModelClass))
#define RTCOM_IS_LOG_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RTCOM_LOG_MODEL_TYPE))
#define RTCOM_IS_LOG_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RTCOM_LOG_MODEL_TYPE))

typedef struct _RTComLogModel RTComLogModel;
struct _RTComLogModel
{
    GtkListStore parent;
};

typedef struct _RTComLogModelClass RTComLogModelClass;
struct _RTComLogModelClass
{
    GtkListStoreClass parent_class;
};

GType rtcom_log_model_get_type(void) G_GNUC_CONST;

/**
 * Creates a new #RTComLogModel.
 * @return a newly created #RTComLogModel
 */
RTComLogModel *
rtcom_log_model_new();

/**
 * Gets the RTComEl used to query the database.
 * @param model The #RTComLogModel
 * @return a pointer to the internal RTComEl
 */
RTComEl *
rtcom_log_model_get_eventlogger(
        RTComLogModel * model);

/**
 * Populates the model with all the events in the database.
 * @param model The #RTComLogModel
 * @param services A NULL terminated array of strings
 */
void
rtcom_log_model_populate(
        RTComLogModel * model,
        const gchar * services[]);

/**
 * Populates the model with events in the database matching the
 * provided query. The query object is taken by model and shouldn't
 * be manually destroyed. This function will override the default
 * result limit with its own, which will be used for all subsequent
 * queries. If that is not what you want, you should use
 * rtcom_log_model_set_limit() to (re)set the limit value again.
 * @param model The #RTComLogModel
 * @param query The #RTComElQuery selecting the events
 */
void
rtcom_log_model_populate_query(
        RTComLogModel *model,
        RTComElQuery *query);

/*
 * Sets the default limit of results to be returned. This will be
 * used for all subsequent queries.
 * @param model The #RTComLogModel
 * @param limit Result limit or -1 for unlimited number of results
 */
void
rtcom_log_model_set_limit(
        RTComLogModel *model,
        gint limit);

/**
 * Refreshed the model, i.e. reloads the events from the db.
 * @param model The #RTComLogModel
 */
void
rtcom_log_model_refresh(
        RTComLogModel * model);

/**
 * Sets whether the events should be grouped.
 * @param model The #RTComLogModel
 * @param group A boolean value.
 */
void
rtcom_log_model_set_group_by(
        RTComLogModel * model,
        RTComElQueryGroupBy group_by);

/**
 * Sets the OssoABookAggregator used to resolve contacts.
 * Note: this function should be called before populating the model if you
 * want to display presence, service icon and avatars where applicable.
 * If you set this to NULL, the model will create and manage its own
 * aggregator.
 * Otherwise, if you're providing your own aggregator, you are responsible
 * for starting and stopping it.
 * @param model The #RTComLogModel
 * @param aggregator The OssoABookAggregator
 */
void
rtcom_log_model_set_abook_aggregator(
        RTComLogModel * model,
        OssoABookAggregator * aggregator);

void
rtcom_log_model_clear(
        RTComLogModel * model);


/**
 * Model filter function for use with HildonLiveSearch.
 *
 * Example usage:
 *   hildon_live_search_set_visible_func(
 *       HILDON_LIVE_SEARCH(live_search),
 *       rtcom_log_model_filter_visible_func,
 *       NULL, NULL);
 */
gboolean
rtcom_log_model_filter_visible_func (GtkTreeModel *model,
    GtkTreeIter *iter, gchar *text, gpointer data);

/**
 * Sets whether the rows should include group chat messages.
 * Note that disabling group chat messages might show them
 * as normal messages instead of hiding (in cases where hiding
 * them would mean no messages from a known contact would be
 * shown). Default value is FALSE (groupchat messages are not
 * shown).
 */
void
rtcom_log_model_set_show_group_chat (
        RTComLogModel * model,
        gboolean is_shown);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */
