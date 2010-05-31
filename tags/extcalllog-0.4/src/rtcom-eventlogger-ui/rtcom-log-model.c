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

#include "rtcom-log-model.h"
#include "rtcom-log-columns.h"

#include <string.h>
#include <hildon/hildon.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include <libosso-abook/osso-abook-waitable.h>
#include <libosso-abook/osso-abook-aggregator.h>
#include <libosso-abook/osso-abook-contact-filter.h>
#include <libosso-abook/osso-abook-contact-subscriptions.h>
#include <libosso-abook/osso-abook-contact.h>
#include <libosso-abook/osso-abook-presence.h>
#include <libosso-abook/osso-abook-roster.h>
#include <libosso-abook/osso-abook-account-manager.h>
#include <libosso-abook/osso-abook-settings.h>

#include <rtcom-eventlogger-plugins/chat.h>

enum
{
    PROP_0,
    RTCOM_LOG_MODEL_PROP_CREATE_AGGREGATOR
};

static void
rtcom_log_model_class_init(
        RTComLogModelClass * klass);

static void
rtcom_log_model_init(
        RTComLogModel * log_model);

static void
rtcom_log_model_dispose(
      GObject * obj);

static void
rtcom_log_model_finalize(
        GObject * obj);

static guint presence_need_redraw_signal_id = 0;
static guint avatar_need_redraw_signal_id = 0;

G_DEFINE_TYPE(RTComLogModel, rtcom_log_model, GTK_TYPE_LIST_STORE);
#define RTCOM_LOG_MODEL_GET_PRIV(log_model) (G_TYPE_INSTANCE_GET_PRIVATE ((log_model), \
            RTCOM_LOG_MODEL_TYPE, RTComLogModelPrivate))

#define MAX_CACHED_PER_QUERY_FIRST 10
#define MAX_CACHED_PER_QUERY 100

#define AVATAR_IMAGE_BORDER { 0, 0, 0, 0 }

typedef struct _RTComLogModelPrivate RTComLogModelPrivate;
struct _RTComLogModelPrivate
{
    /* Core stuff */
    RTComEl * backend;
    RTComElQuery * current_query;
    RTComElIter * current_iter;
    gchar ** filtered_services;

    /* Receive signals */
    gulong new_event_handler;
    gulong event_updated_handler;
    gulong event_deleted_handler;
    gulong all_deleted_handler;
    gulong refresh_hint_handler;

    /* Caching stuff */
    guint cached_n;
    gboolean done_caching;
    GThread * cache_thread;
    GThread * pixbufs_thread;
    gboolean cancel_threads;
    /* A hash table of <path, pixbuf> */
    GHashTable * cached_icons;
    GHashTable * cached_service_icons;
    GHashTable * cached_account_data;

    RTComElQueryGroupBy group_by;
    gint limit;

    /* whether to special-case groupchats (hide or
     * fake normal message, depending on whether we know
     * or not the contact sending the message, and depending
     * on the protocol) */
    gboolean show_group_chat;

    OssoABookWaitableClosure *aggregator_ready_closure;
    OssoABookAggregator * abook_aggregator;
    OssoABookContactSubscriptions * abook_subs;
    gboolean create_aggregator; /* set the create-aggregator property to FALSE
                                   if you never want RTComLogModel to create an
                                   aggregator of its own (maybe you want to pass
                                   one of your own later on). This defaults to
                                   TRUE */
    gboolean own_aggregator;    /* this variable just tells whether we have
                                   _already_ created our own aggregator. Don't
                                   mix it up with create_aggregator. */
    gboolean abook_aggregator_ready;
    gboolean pixbufs_populated;

    OssoABookAccountManager *account_manager;
    OssoABookWaitableClosure *accman_ready_closure;
    GHashTable *vcard_field_mapping;

    /* Whether the API user has populated the model. If not, don't
     * react to any DBus signals. */
    gboolean in_use;

    /* Idle handler for refreshing the view, used for avoid unneccessary
     * multiple refreshes. */
    guint refresh_id;

    GConfClient *gconf_client;
    guint display_order_notify_id;
};

typedef struct _caching_data caching_data_t;
struct _caching_data
{
    GList * cached_events;
    RTComLogModel * model;
    gboolean prepend;
};

typedef struct _account_data account_data_t;
struct _account_data
{
    RTComLogModel * model;
    OssoABookContact * contact;
    GdkPixbuf * service_icon;
    gulong avatar_notify_id;
    gulong presence_notify_id;
    gulong name_notify_id;
};

static account_data_t *
_account_data_new ()
{
    account_data_t * data = g_slice_new0(account_data_t);

    data->avatar_notify_id = 0;
    data->presence_notify_id = 0;
    data->name_notify_id = 0;
    return data;
}

static void
_account_data_free (account_data_t * data)
{
    if (!data)
        return;

    if (data->contact)
    {
        if (data->avatar_notify_id != 0)
            g_signal_handler_disconnect (data->contact, data->avatar_notify_id);

        if (data->presence_notify_id != 0)
            g_signal_handler_disconnect (data->contact, data->presence_notify_id);

        if (data->name_notify_id != 0)
            g_signal_handler_disconnect (data->contact, data->name_notify_id);

        g_object_unref(data->contact);
    }

    /*
     * The service_icon doesn't need to be unreffed because it will be
     * unreffed by the model.
     */

    g_slice_free (account_data_t, data);
}

static void
_priv_cancel_and_join_threads (RTComLogModelPrivate *priv)
{
    priv->cancel_threads = TRUE;

    if(priv->cache_thread)
    {
        g_debug("%s: waiting for cache_thread to exit gracefully...",
            G_STRFUNC);
        g_thread_join(priv->cache_thread);
        priv->cache_thread = NULL;
    }
    if(priv->pixbufs_thread)
    {
        g_debug("%s: waiting for pixbufs_thread to exit gracefully...",
            G_STRFUNC);
        g_thread_join(priv->pixbufs_thread);
        priv->pixbufs_thread = NULL;
    }

    priv->cancel_threads = FALSE;
}

static gchar *
_account_data_generate_key(
        const gchar * local_uid,
        const gchar * remote_uid)
{
    GString * key = g_string_new("");
    g_string_append_printf(key, "%s/%s", local_uid, remote_uid);
    return g_string_free(key, FALSE);
}

static const GdkPixbuf *_get_service_icon (RTComLogModel *model,
    const gchar *local_uid);

static gboolean
_emit_row_changed_for_contact_slave (GtkTreeModel *model,
    GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    OssoABookContact *contact = data;
    OssoABookContact *current;

    gtk_tree_model_get(model, iter, RTCOM_LOG_VIEW_COL_CONTACT, &current, -1);

    if (contact == current)
        g_signal_emit_by_name (model, "row-changed", path, iter);

    if (current != NULL)
        g_object_unref (current);

    return FALSE;
}

static void
_emit_row_changed_for_contact (RTComLogModel *model,
    OssoABookContact *contact)
{
    gtk_tree_model_foreach (GTK_TREE_MODEL (model),
        _emit_row_changed_for_contact_slave, contact);
}

static void
_presence_notify_status_callback(
        OssoABookPresence * presence,
        GParamSpec * spec,
        account_data_t * data)
{
    if (!OSSO_ABOOK_IS_CONTACT(data->contact))
    {
        g_warning("%s: Contact %p is not valid, ignoring",
          G_STRFUNC, data->contact);
        return;
    }

    if(presence)
    {
        _emit_row_changed_for_contact (data->model, data->contact);
     }
}

static gboolean
_find_row_for_contact (GtkTreeModel *model, OssoABookContact *contact,
    GtkTreeIter *iter)
{
  if (!gtk_tree_model_get_iter_first (model, iter))
      return FALSE;

  do
    {
        OssoABookContact *current;

        gtk_tree_model_get (model, iter,
            RTCOM_LOG_VIEW_COL_CONTACT, &current, -1);

        if (current != NULL)
	  {
	      if (current == contact)
	        {
		  g_object_unref (current);
		  return TRUE;
	        }

	      g_object_unref (current);
	  }
    }
  while (gtk_tree_model_iter_next (model, iter));

  return FALSE;
}


static void
_account_notify_name_callback(
        OssoABookContact * contact,
        GParamSpec * spec,
        account_data_t * data)
{
    GtkTreeIter iter;

    /* Store the new display name in model so that search works
     * correctly. */

    if (_find_row_for_contact (GTK_TREE_MODEL (data->model),
        data->contact, &iter))
      {
        const gchar *name =
            osso_abook_contact_get_display_name (data->contact);

        if (name != NULL)
            gtk_list_store_set (GTK_LIST_STORE (data->model), &iter,
                RTCOM_LOG_VIEW_COL_REMOTE_NAME, name, -1);
      }
}

static void
_account_avatar_notify_image_callback(
        OssoABookAvatar * avatar,
        GParamSpec * spec,
        account_data_t * data)
{
    GdkPixbuf * avatar_pixbuf = NULL;

    if(avatar)
    {
        const guint8 border[4] = AVATAR_IMAGE_BORDER;

        g_debug("Avatar changed.");

        if (!OSSO_ABOOK_IS_CONTACT(data->contact))
        {
            g_warning("%s: Contact %p is not valid, ignoring",
              G_STRFUNC, data->contact);
            return;
        }

        avatar_pixbuf = osso_abook_avatar_get_image_rounded(
                avatar,
                HILDON_ICON_PIXEL_SIZE_FINGER,
                HILDON_ICON_PIXEL_SIZE_FINGER,
                TRUE, -1, border);

        if(avatar_pixbuf)
        {
            g_debug("Setting the avatar-pixbuf data (%p).", avatar_pixbuf);
            g_object_set_data_full(
                    G_OBJECT(data->contact),
                    "avatar-pixbuf",
                    avatar_pixbuf,
                    (GDestroyNotify) g_object_unref);
        }
        else
            g_object_set_data(
                    G_OBJECT(data->contact),
                    "avatar-pixbuf",
                    NULL);

        g_debug("Emit avatar-need-redraw.");
        _emit_row_changed_for_contact (data->model, data->contact);
    }
}

/* Get a VCard field that holds the remote_uid info
 * on a specific account. */
static const gchar *
vcard_field_for_account (RTComLogModel *model, const gchar *local_uid)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    McAccount *acc;
    McProfile *profile;
    const gchar *id;
    const gchar *vcard_field;

    vcard_field = g_hash_table_lookup (priv->vcard_field_mapping, local_uid);
    if (vcard_field)
        return vcard_field;

    if (!osso_abook_waitable_is_ready (
        OSSO_ABOOK_WAITABLE (priv->account_manager), NULL))
      return NULL;

    acc = osso_abook_account_manager_lookup_by_name (priv->account_manager,
        local_uid);
    if (!acc)
        return NULL;

    id = mc_account_compat_get_profile (acc);
    profile = mc_profile_lookup (id);
    if (!profile)
        return NULL;

    vcard_field = mc_profile_get_vcard_field (profile);
    if (!vcard_field)
        return NULL;

    g_hash_table_insert (priv->vcard_field_mapping, g_strdup (local_uid),
        g_strdup (vcard_field));
    return vcard_field;
}

static gchar *
new_discover_abook_contact (RTComLogModel *model, const char *local_uid,
    const gchar *remote_uid)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    const gchar *vcard_field;
    GList *contacts = NULL;
    gchar *remote_ebook_uid = NULL;

    g_return_val_if_fail (osso_abook_waitable_is_ready(
          OSSO_ABOOK_WAITABLE(priv->abook_aggregator), NULL), NULL);

    /* don't even attempt discovering hidden/empty numbers/IDs */
    if ((remote_uid == NULL) || (*remote_uid == '\0'))
        return NULL;

    vcard_field = vcard_field_for_account (model, local_uid);
    if (!vcard_field)
        return NULL;

    if (!strcmp (vcard_field, EVC_TEL))
      {
        contacts = osso_abook_aggregator_find_contacts_for_phone_number
            (OSSO_ABOOK_AGGREGATOR (priv->abook_aggregator), remote_uid, FALSE);
      }
    else
      {
        EBookQuery *query;

        query = e_book_query_vcard_field_test (vcard_field,
                                               E_BOOK_QUERY_IS,
                                               remote_uid);
        contacts = osso_abook_aggregator_find_contacts
            (OSSO_ABOOK_AGGREGATOR (priv->abook_aggregator), query);
        e_book_query_unref (query);
      }

    if (contacts)
    {
        /* only match if there is exactly one matching contact */
        if (contacts->next == NULL)
          {
            g_debug ("%s: discovered remote contact: %s for remote uid %s on account %s",
                G_STRFUNC, remote_ebook_uid, remote_uid, local_uid);

            remote_ebook_uid = g_strdup (
                osso_abook_contact_get_persistent_uid (
                    OSSO_ABOOK_CONTACT (contacts->data)));
          }
        else
          {
            g_debug ("%s: discovered multiple matching contacts, ignoring all",
                G_STRFUNC);
          }

        g_list_free (contacts);
    }

    return remote_ebook_uid;
}

static gchar *
discover_abook_contact(
        RTComLogModel *model,
        const gchar *local_uid,
        const gchar *remote_uid)
{
    gchar *remote_ebook_uid = NULL;
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    GList *li;

    return new_discover_abook_contact (model, local_uid, remote_uid);

    g_return_val_if_fail (osso_abook_waitable_is_ready(
          OSSO_ABOOK_WAITABLE(priv->abook_aggregator), NULL), NULL);

    if (!remote_uid || !remote_uid[0])
        return NULL;

    /* First, try the phone number. */
    li = osso_abook_aggregator_find_contacts_for_phone_number (
        OSSO_ABOOK_AGGREGATOR (priv->abook_aggregator),
        remote_uid, FALSE);

    /* Otherwise, try as an email address. */
    if (li == NULL)
        li = osso_abook_aggregator_find_contacts_for_email_address (
            OSSO_ABOOK_AGGREGATOR (priv->abook_aggregator),
            remote_uid);

    /* Last chance, try IM address. Pick up any contact that has
     * this, no matter what local_uid is (FIXME this not enitrely
     * correct). */
    if (li == NULL)
        li = osso_abook_aggregator_find_contacts_for_im_contact (
            OSSO_ABOOK_AGGREGATOR (priv->abook_aggregator),
            remote_uid, NULL);

    /* If we got it, save the uid. */
    if (li != NULL)
    {
        remote_ebook_uid = g_strdup (
            osso_abook_contact_get_persistent_uid (OSSO_ABOOK_CONTACT (li->data)));
        g_list_free (li);

        g_debug ("%s: discovered remote contact: %s for remote uid %s on account %s",
            G_STRFUNC, remote_ebook_uid, remote_uid, local_uid);
    }

    return remote_ebook_uid;
}

static OssoABookContact *
_get_contact_from_abook_uid (RTComLogModel *model, const gchar *abook_uid)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    OssoABookContact *c;
    GList *l;

    g_return_val_if_fail (osso_abook_waitable_is_ready(
          OSSO_ABOOK_WAITABLE(priv->abook_aggregator), NULL), NULL);

    if (!abook_uid)
        return NULL;

    l = osso_abook_aggregator_lookup(
          OSSO_ABOOK_AGGREGATOR(priv->abook_aggregator),
          abook_uid);

    if (!l)
        return NULL;

    c = OSSO_ABOOK_CONTACT (l->data);
    g_list_free (l);

    return g_object_ref (c);
}

static void
_remote_contact_discovery (RTComLogModel *model)
{
    RTComLogModelPrivate * priv;
    GtkTreeIter iter;
    gboolean valid;

    g_debug("%s: discovering remote contacts if needed", G_STRFUNC);

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
    while(valid)
    {
        gchar * local_uid, * remote_uid, * remote_ebook_uid;
        OssoABookContact *c = NULL;
        const gchar *name = NULL;

        gtk_tree_model_get(
                GTK_TREE_MODEL(model), &iter,
                RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT, &local_uid,
                RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &remote_uid,
                RTCOM_LOG_VIEW_COL_ECONTACT_UID, &remote_ebook_uid,
                RTCOM_LOG_VIEW_COL_CONTACT, &c,
                -1);

        if (c)
        {
            /* Already have it. */
            g_object_unref (c);
            goto cont;
        }

        if (!remote_ebook_uid)
        {
            /* Attempt to guess remote_ebook_uid if possible */
            remote_ebook_uid = discover_abook_contact (model,
                local_uid, remote_uid);

            if (!remote_ebook_uid)
                goto cont;

        }

        c = _get_contact_from_abook_uid(model, remote_ebook_uid);

        if (!c)
            goto cont;

        name = osso_abook_contact_get_display_name (c);

        /* If we've managed to get the contact object, we can
         * safely store the abook id, contact object itself,
         * and current display name. \o/ */
        gtk_list_store_set (GTK_LIST_STORE(model), &iter,
            RTCOM_LOG_VIEW_COL_CONTACT, c,
            RTCOM_LOG_VIEW_COL_REMOTE_NAME, name,
            RTCOM_LOG_VIEW_COL_ECONTACT_UID, remote_ebook_uid,
            -1);

        g_object_unref (c);

cont:
        g_free(local_uid);
        g_free(remote_uid);
        g_free(remote_ebook_uid);

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(model), &iter);
    }
}

static account_data_t *
_populate_pixbufs(
        RTComLogModel * model,
        const gchar * local_uid,
        const gchar * remote_uid,
        OssoABookContact *master_contact)
{
    RTComLogModelPrivate * priv;

    gchar * caching_account_data_key;
    account_data_t * account_data;

    if(!(local_uid && remote_uid))
    {
        g_debug(G_STRLOC ": accounts not defined.");
        return NULL;
    }

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    caching_account_data_key = _account_data_generate_key(
            local_uid, remote_uid);
    account_data = g_hash_table_lookup(
            priv->cached_account_data,
            caching_account_data_key);

    if(!account_data)
    {
        g_debug(G_STRLOC ": account wasn't already there. Creating it.");

        account_data = _account_data_new();
        g_hash_table_insert(
                priv->cached_account_data,
                g_strdup (caching_account_data_key),
                account_data);

        account_data->model = model;
    }

    g_free(caching_account_data_key);

    priv->pixbufs_populated = TRUE;

    /* Using master contact instead the roster one here so
     * we can proprely _get_display_name(contact) later */
    if (account_data->contact == NULL)
    {
        account_data->contact = g_object_ref (master_contact);

        account_data->presence_notify_id = g_signal_connect(
                master_contact, "notify::presence-status",
                G_CALLBACK(_presence_notify_status_callback),
                account_data);
        account_data->avatar_notify_id = g_signal_connect(
                master_contact, "notify::avatar-image",
                G_CALLBACK(_account_avatar_notify_image_callback),
                account_data);
        account_data->name_notify_id = g_signal_connect(
                master_contact, "notify::display-name",
                G_CALLBACK(_account_notify_name_callback),
                account_data);
    }

    _presence_notify_status_callback(
            OSSO_ABOOK_PRESENCE(master_contact), NULL, account_data);
    _account_avatar_notify_image_callback(
            OSSO_ABOOK_AVATAR(master_contact), NULL, account_data);

    return account_data;
}

static void
_abook_aggregator_ready(
      OssoABookWaitable *waitable,
      const GError *error,
      gpointer data)
{
    RTComLogModel *model = RTCOM_LOG_MODEL(data);
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    GtkTreeModel *tm = GTK_TREE_MODEL(model);

    g_debug ("%s: called", G_STRFUNC);

    priv->aggregator_ready_closure = NULL;

    if (error)
        return;

    g_debug ("%s: setting aggregator as READY", G_STRFUNC);
    priv->abook_aggregator_ready = TRUE;

    _remote_contact_discovery (model);

    if (!priv->pixbufs_populated)
    {
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(tm, &iter);

        while(valid)
        {
            OssoABookContact *c = NULL;
            gchar *local_uid, *remote_uid;

            gtk_tree_model_get(tm, &iter,
                    RTCOM_LOG_VIEW_COL_CONTACT, &c,
                    RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT, &local_uid,
                    RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &remote_uid,
                    -1);

            if (c)
            {
                _populate_pixbufs (model, local_uid, remote_uid, c);
                g_object_unref (c);
            }

            g_free(local_uid);
            g_free(remote_uid);

            valid = gtk_tree_model_iter_next(tm, &iter);
        }
    }
}

static gboolean
_stage_cached (gpointer data)
{
    RTComLogModel * model = NULL;
    RTComLogModelPrivate * priv = NULL;
    caching_data_t * caching_data = data;
    GList * events_iter = NULL;

    struct _staging_data
    {
        GHashTable * values;
        GtkTreePath * path;
        GString * caching_account_data_key;
        account_data_t * account_data;

        gint event_id;
        const gchar * service;
        const gchar * group_uid;
        const gchar * local_uid;
        const gchar * remote_uid;
        const gchar * remote_name;
        const gchar * remote_ebook_uid;
        const gchar * text;
        const gchar * icon_name;
        gint timestamp;
        gint count;
        const gchar * group_title;
        const gchar * event_type;
        gboolean outgoing;
        gint flags;
    } staging_data;

    GtkTreeIter iter, deletion_iter;
    gchar * deletion_group_uid = NULL,
          * deletion_ebook_uid = NULL,
          * deletion_local_uid = NULL,
          * deletion_remote_uid = NULL;

    GtkIconTheme * icon_theme;
    icon_theme = gtk_icon_theme_get_default();

    model = caching_data->model;
    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    for(    caching_data->cached_events = g_list_first(caching_data->cached_events),
            events_iter = caching_data->cached_events;
            events_iter;
            events_iter = events_iter->next)
    {
        GdkPixbuf * icon = NULL;
        const GdkPixbuf * service_icon;

        memset(&staging_data, 0, sizeof(struct _staging_data));

        if(priv->cancel_threads)
        {
            g_list_foreach(
                    caching_data->cached_events,
                    (GFunc) g_hash_table_destroy,
                    NULL);
            g_list_free(caching_data->cached_events);
            g_free(caching_data);
            return FALSE;
        }

        staging_data.values = (GHashTable *) events_iter->data;

#define LOOKUP_INT(x) \
    g_value_get_int(g_hash_table_lookup(staging_data.values, x))
#define LOOKUP_BOOL(x) \
    g_value_get_boolean(g_hash_table_lookup(staging_data.values, x))
#define LOOKUP_STR(x) \
    g_value_get_string(g_hash_table_lookup(staging_data.values, x))

        staging_data.event_id = LOOKUP_INT ("id");
        staging_data.service = LOOKUP_STR ("service");
        staging_data.group_uid = LOOKUP_STR ("group-uid");
        staging_data.local_uid = LOOKUP_STR ("local-uid");
        staging_data.remote_uid = LOOKUP_STR ("remote-uid");
        staging_data.remote_name = LOOKUP_STR ("remote-name");
        staging_data.remote_ebook_uid = LOOKUP_STR ("remote-ebook-uid");
        staging_data.text = LOOKUP_STR ("content");
        staging_data.icon_name = LOOKUP_STR ("icon-name");
        staging_data.timestamp = LOOKUP_INT ("start-time");
        staging_data.count = LOOKUP_INT ("event-count");
        staging_data.group_title = LOOKUP_STR ("group-title");
        staging_data.event_type = LOOKUP_STR ("event-type");
        staging_data.outgoing = LOOKUP_BOOL ("outgoing");
        staging_data.flags = LOOKUP_INT ("flags");

        /* if requested, ignore groupchat events */
        if (!priv->show_group_chat &&
            (staging_data.flags & RTCOM_EL_FLAG_CHAT_GROUP) &&
            !strcmp (staging_data.service, "RTCOM_EL_SERVICE_CHAT"))
        {
            /* this is ugly; some protocols use normal remote uids of contacts
             * even for groupchat messages (notably, skype); if we ignore
             * those and grouping is by uids or contact, we won't show *any*
             * messages (incl. non-groupchat-ones) from that contact, because
             * the database gave us only this row; in this
             * case it's prefferable that we treat the groupchat message as
             * a normal one; so, if abook euid of the contact is known, we'll
             * treat the message as a normal one instead. otherwise, we ignore
             * it. */
            if (staging_data.remote_ebook_uid)
              {
                staging_data.flags &= ~RTCOM_EL_FLAG_CHAT_GROUP;
              }
            else
              {
                g_hash_table_destroy(staging_data.values);
                continue;
              }
        }

        g_debug("Staging event:\n\tid: %d\n\tservice: %s\n\tgroup_uid: %s\n\tlocal_uid: %s\n\tremote_uid: %s\n\t"
                "remote_name: %s\n\tremote_ebook_uid: %s\n\ttext: %s\n\ticon_name: %s\n\t"
                "timestamp: %d\n\tevents in group: %d\n\tgroup title: %s\n\tevent type: %s\n\t"
                "outgoing: %s\n\t flags: %d\n",
                staging_data.event_id,
                staging_data.service,
                staging_data.group_uid,
                staging_data.local_uid,
                staging_data.remote_uid,
                staging_data.remote_name,
                staging_data.remote_ebook_uid,
                staging_data.text,
                staging_data.icon_name,
                staging_data.timestamp,
                staging_data.count,
                staging_data.group_title,
                staging_data.event_type,
                staging_data.outgoing ? "yes" : "no",
                staging_data.flags);

        if(staging_data.icon_name)
        {
            icon = g_hash_table_lookup(
                    priv->cached_icons,
                    staging_data.icon_name);
            if(!icon)
            {
                icon = gtk_icon_theme_load_icon(
                        gtk_icon_theme_get_default(),
                        staging_data.icon_name,
                        RTCOM_LOG_VIEW_ICON_SIZE,
                        0, NULL);
                if(icon)
                    g_hash_table_insert(
                            priv->cached_icons,
                            g_strdup(staging_data.icon_name),
                            icon);
            }
        }

        service_icon = _get_service_icon (model, staging_data.local_uid);

        gtk_list_store_insert_with_values(
                GTK_LIST_STORE(model),
                &iter,
                caching_data->prepend ? 0 : GTK_LIST_STORE(model)->length,

                RTCOM_LOG_VIEW_COL_ICON,           icon,
                RTCOM_LOG_VIEW_COL_EVENT_ID,       staging_data.event_id,
                RTCOM_LOG_VIEW_COL_SERVICE,        staging_data.service,
                RTCOM_LOG_VIEW_COL_GROUP_UID,      staging_data.group_uid,

                RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT,  staging_data.local_uid,
                RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, staging_data.remote_uid,
                RTCOM_LOG_VIEW_COL_REMOTE_NAME,    staging_data.remote_name,
                RTCOM_LOG_VIEW_COL_ECONTACT_UID,   staging_data.remote_ebook_uid,

                RTCOM_LOG_VIEW_COL_TEXT,           staging_data.text,
                RTCOM_LOG_VIEW_COL_TIMESTAMP,      staging_data.timestamp,
                RTCOM_LOG_VIEW_COL_COUNT,          staging_data.count,
                RTCOM_LOG_VIEW_COL_GROUP_TITLE,    staging_data.group_title,
                RTCOM_LOG_VIEW_COL_EVENT_TYPE,     staging_data.event_type,
                RTCOM_LOG_VIEW_COL_OUTGOING,       staging_data.outgoing,
                RTCOM_LOG_VIEW_COL_FLAGS,          staging_data.flags,

                /* We shouldn't be setting NULL for a GObject and we don't
                 * want to use separate set (to minimise row redraw), so we're
                 * tricking GtkListStore into thinking that's the terminator.
                 */
                (service_icon != NULL) ? RTCOM_LOG_VIEW_COL_SERVICE_ICON : -1,
                    service_icon,

                -1);

        if(priv->abook_aggregator_ready)
        {
            OssoABookContact *c;

            /* Attempt to guess remote_ebook_uid if possible */
            if (!staging_data.remote_ebook_uid &&
                staging_data.local_uid &&
                staging_data.remote_uid)
            {
                staging_data.remote_ebook_uid = discover_abook_contact (model,
                    staging_data.local_uid,
                    staging_data.remote_uid);

                /* We want to put the discovered uid back to the staging data
                 * values so it gets freed afterwards, with the rest of them. */
                g_value_take_string (g_hash_table_lookup (staging_data.values,
                    "remote-ebook-uid"), (gchar *) staging_data.remote_ebook_uid);
            }

            c = _get_contact_from_abook_uid (model,
                staging_data.remote_ebook_uid);

            /* If we find the contact, store it and its display name
             * immediately, and call populate pixbufs on it.
             *
             * TODO: remove duplicate code from here and
             * _remote_contact_discovery (here we need to discover a
             * single one, while _remote_contact_discovery does it
             * for all undiscovered contacts in the list store. */
            if (c)
            {
                const gchar *name = osso_abook_contact_get_display_name (c);

                gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                    RTCOM_LOG_VIEW_COL_CONTACT, c,
                    RTCOM_LOG_VIEW_COL_ECONTACT_UID,
                        staging_data.remote_ebook_uid,
                    -1);

                if (name)
                    gtk_list_store_set (GTK_LIST_STORE (model),
                        &iter, RTCOM_LOG_VIEW_COL_REMOTE_NAME,
                        name, -1);

                staging_data.account_data = _populate_pixbufs(
                        model,
                        staging_data.local_uid,
                        staging_data.remote_uid,
                        c);

                g_object_unref (c);
            }
        }
        else
        {
            g_debug(G_STRLOC ": couldn't find contact because the aggregator is not ready.");
            priv->pixbufs_populated = FALSE;
        }

        if (staging_data.account_data && staging_data.account_data->contact)
          {
            const gchar *name =
                osso_abook_contact_get_display_name
                    (staging_data.account_data->contact);

            if (name != NULL)
              gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                  RTCOM_LOG_VIEW_COL_REMOTE_NAME, name, -1);
          }

        /**
         * Now let's figure out if this new event that we just added,
         * belongs to an existing group. Of course this can only be if
         * we're adding a single new event, so that caching_data->prepend
         * will be TRUE.
         * If this is the case, we just need to delete the old row
         * representing the group.
         */
        if(priv->group_by == RTCOM_EL_QUERY_GROUP_BY_GROUP)
        {
            deletion_iter = iter;
            while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &deletion_iter))
            {
                deletion_group_uid = NULL;
                gtk_tree_model_get(
                        GTK_TREE_MODEL(model),
                        &deletion_iter,
                        RTCOM_LOG_VIEW_COL_GROUP_UID, &deletion_group_uid,
                        -1);
                if(staging_data.group_uid && deletion_group_uid &&
                   !strcmp(deletion_group_uid, staging_data.group_uid))
                {
                    g_debug(G_STRLOC ": we found the old group, let's delete it.");
                    gtk_list_store_remove(GTK_LIST_STORE(model), &deletion_iter);
                    g_free (deletion_group_uid);
                    break;
                }
                g_free (deletion_group_uid);
            }
        }
        else if(priv->group_by == RTCOM_EL_QUERY_GROUP_BY_UIDS)
        {
            deletion_iter = iter;
            while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &deletion_iter))
            {
                gtk_tree_model_get(
                        GTK_TREE_MODEL(model),
                        &deletion_iter,
                        RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT, &deletion_local_uid,
                        RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &deletion_remote_uid,
                        -1);
                if(staging_data.local_uid && staging_data.remote_uid &&
                   deletion_local_uid && deletion_remote_uid)
                {
                    if(strcmp(deletion_local_uid, staging_data.local_uid) == 0 &&
                       strcmp(deletion_remote_uid, staging_data.remote_uid) == 0)
                    {
                        g_debug(G_STRLOC ": we found the old entry for this contacts pair, let's delete it.");
                        gtk_list_store_remove(GTK_LIST_STORE(model), &deletion_iter);
                        g_free(deletion_local_uid);
                        g_free(deletion_remote_uid);
                        break;
                    }
                }

                g_free(deletion_local_uid);
                g_free(deletion_remote_uid);
            }
        }
        else if(priv->group_by == RTCOM_EL_QUERY_GROUP_BY_CONTACT)
        {
            GtkTreeIter moving_iter;

            deletion_iter = iter;
            while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &deletion_iter))
            {
                deletion_ebook_uid = NULL;
                gtk_tree_model_get(
                        GTK_TREE_MODEL(model),
                        &deletion_iter,
                        RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT, &deletion_local_uid,
                        RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &deletion_remote_uid,
                        RTCOM_LOG_VIEW_COL_ECONTACT_UID, &deletion_ebook_uid,
                        -1);
                if(deletion_ebook_uid && staging_data.remote_ebook_uid &&
                  !strcmp(deletion_ebook_uid, staging_data.remote_ebook_uid))
                {
                    g_debug(G_STRLOC ": we found the old entry for this contact, let's delete it.");
                    gtk_list_store_remove(GTK_LIST_STORE(model), &deletion_iter);
                    g_free (deletion_ebook_uid);
                    g_free(deletion_local_uid);
                    g_free(deletion_remote_uid);
                    break;
                }
                else if(staging_data.local_uid && staging_data.remote_uid &&
                   deletion_local_uid && deletion_remote_uid &&
                  (strcmp(deletion_local_uid, staging_data.local_uid) == 0 &&
                     strcmp(deletion_remote_uid, staging_data.remote_uid) == 0))
                {
                    g_debug(G_STRLOC ": we found the old entry for this contacts pair, let's delete it.");
                    gtk_list_store_remove(GTK_LIST_STORE(model), &deletion_iter);
                    g_free (deletion_ebook_uid);
                    g_free(deletion_local_uid);
                    g_free(deletion_remote_uid);
                    break;
                }
                else
                {
                    g_debug(G_STRLOC ": couldn't match the EBook UIDs nor the (local,remote) UID pair.");
                    g_debug(G_STRLOC ": deletion_ebook_uid = %s, staging_data.remote_ebook_uid = %s",
                            deletion_ebook_uid, staging_data.remote_ebook_uid);
                    g_free (deletion_ebook_uid);
                    g_free(deletion_local_uid);
                    g_free(deletion_remote_uid);
                }
            }

            /**
             * Now let's get back at the inserted iter and figure out if we
             * need to move it down.
             */
            moving_iter = iter;
            while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &moving_iter))
            {
                gint id_moving_iter;

                gtk_tree_model_get(
                        GTK_TREE_MODEL(model), &moving_iter,
                        RTCOM_LOG_VIEW_COL_EVENT_ID, &id_moving_iter,
                        -1);
                g_debug(G_STRLOC ": iterating moving_iter with id %d", id_moving_iter);
                if(id_moving_iter > staging_data.event_id)
                {
                    /**
                     * Found it!
                     */
                    g_debug(G_STRLOC ": move it down!");
                    gtk_list_store_swap(
                            GTK_LIST_STORE(model),
                            &moving_iter,
                            &iter);
                }
                else
                    break;
            }
        }

        /* If we ended up with more events than we should show, trim the last
         * ones (index starts at 0). */
        if (priv->limit != -1)
        {
            while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(model),
                &deletion_iter, NULL, priv->limit))
            {
                gtk_list_store_remove(GTK_LIST_STORE(model), &deletion_iter);
            }
        }

        g_hash_table_destroy(staging_data.values);
    }

    g_list_free(caching_data->cached_events);
    g_free(caching_data);

    return FALSE;
}

static gpointer
_threaded_cached_load (gpointer data)
{
    RTComLogModel * model = RTCOM_LOG_MODEL(data);
    RTComLogModelPrivate * priv = NULL;
    RTComElIter * it = NULL;
    GHashTable * values = NULL;

    g_return_val_if_fail(RTCOM_IS_LOG_MODEL(model), FALSE);

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    g_return_val_if_fail(RTCOM_IS_EL(priv->backend), FALSE);
    g_return_val_if_fail(RTCOM_IS_EL_QUERY(priv->current_query), FALSE);

    if(priv->done_caching)
    {
        g_debug("All events cached!");
        return NULL;
    }

    for(;;)
    {
        gint limit;

        if(priv->cancel_threads)
            return NULL;

        limit = MAX_CACHED_PER_QUERY;
        if (priv->limit != -1)
          {
            /* Cache up to specified limit */
            if ((priv->cached_n + limit) > priv->limit)
                limit = priv->limit - priv->cached_n;

            /* If priv->limit changed in the meantime, this can happen.
             * Don't do anything else. */
            if (limit < 1)
              {
                priv->done_caching = TRUE;
                break;
              }
          }
        rtcom_el_query_set_limit(priv->current_query, limit);
        rtcom_el_query_set_offset(priv->current_query, priv->cached_n);
        if(!rtcom_el_query_refresh(priv->current_query))
        {
            g_object_unref(priv->current_query);
            priv->current_query = NULL;
            return NULL;
        }

        it = rtcom_el_get_events(priv->backend, priv->current_query);
        if(it && rtcom_el_iter_first(it))
        {
            caching_data_t * d = g_new0(caching_data_t, 1);
            d->model = model;
            d->prepend = FALSE;

            do
            {
                if(priv->cancel_threads)
                {
                    g_object_unref(it);
                    g_list_free(d->cached_events);
                    g_free(d);
                    return NULL;
                }

                values = rtcom_el_iter_get_value_map(
                        it,
                        "id",
                        "service",
                        "group-uid",
                        "local-uid",
                        "remote-uid",
                        "remote-name",
                        "remote-ebook-uid",
                        "content",
                        "icon-name",
                        "start-time",
                        "event-count",
                        "group-title",
                        "event-type",
                        "outgoing",
                        "flags",
                        NULL);
                d->cached_events = g_list_prepend(d->cached_events, values);
            } while(rtcom_el_iter_next(it));

            g_object_unref(it);

            priv->cached_n += MAX_CACHED_PER_QUERY;

            d->cached_events = g_list_reverse(d->cached_events);
            g_idle_add((GSourceFunc) _stage_cached, d);
        }
        else
        {
            priv->done_caching = TRUE;
            break;
        }
    }

    return NULL;
}


static void
_connect_aggregator_signals (RTComLogModel *model)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    priv->aggregator_ready_closure =
        osso_abook_waitable_call_when_ready (OSSO_ABOOK_WAITABLE(priv->abook_aggregator),
            _abook_aggregator_ready, model, NULL);
}

static void
_create_own_aggregator(
        RTComLogModel * model)
{
    GList * distinct_remote_uids;
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    priv->own_aggregator = TRUE;

    distinct_remote_uids = rtcom_el_get_unique_remote_ebook_uids(
            priv->backend);
    if(distinct_remote_uids && g_list_length(distinct_remote_uids) > 0)
    {
        GList * iter;

        priv->abook_aggregator = OSSO_ABOOK_AGGREGATOR(
                osso_abook_aggregator_new(NULL, NULL));
        priv->abook_aggregator_ready = FALSE;
        priv->pixbufs_populated = FALSE;
        _connect_aggregator_signals(model);

        priv->abook_subs = osso_abook_contact_subscriptions_new();
        for(iter = distinct_remote_uids; iter; iter = iter->next)
        {
            osso_abook_contact_subscriptions_add(
                    priv->abook_subs,
                    iter->data);
            g_free(iter->data);
        }

        g_list_free(distinct_remote_uids);

        osso_abook_aggregator_add_filter(
                OSSO_ABOOK_AGGREGATOR(priv->abook_aggregator),
                OSSO_ABOOK_CONTACT_FILTER(priv->abook_subs));

        osso_abook_roster_start(OSSO_ABOOK_ROSTER(priv->abook_aggregator));
    }
    else
    {
        g_debug("Not creating the aggregator because there are no events"
                " in the db with a remote_ebook_uid.");
    }
}


static void
_new_event_callback (
        RTComEl * backend,
        gint event_id,
        const gchar * local_uid,
        const gchar * remote_uid,
        const gchar * remote_ebook_uid,
        const gchar * group_uid,
        const gchar * service,
        RTComLogModel * model)
{
    RTComLogModelPrivate * priv = NULL;
    RTComElQuery * query = NULL;
    RTComElIter * el_iter = NULL;
    GHashTable * values = NULL;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    if(!RTCOM_IS_EL(priv->backend))
    {
        g_warning("Backend is undefined.");
        return;
    }

    if (!priv->in_use)
        return;

    if((service != NULL) && (priv->filtered_services != NULL))
    {
        int i;

        /* Check if we're actually filtering this service */
        for(i = 0; priv->filtered_services[i]; i++)
        {
            if(strcmp(service, priv->filtered_services[i]) == 0)
            {
                /* Ok, here it is, we can continue to staging */
                break;
            }
        }

        if(priv->filtered_services[i] == NULL)
            return;
    }

    query = rtcom_el_query_new(priv->backend);
    rtcom_el_query_set_group_by(query, priv->group_by);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        g_warning("Couldn't prepare query");
        g_object_unref(query);
        return;
    }

    el_iter = rtcom_el_get_events(priv->backend, query);
    g_object_unref(query);

    if(el_iter && rtcom_el_iter_first(el_iter))
    {
        values = rtcom_el_iter_get_value_map(
                el_iter,
                "id",
                "service",
                "group-uid",
                "local-uid",
                "remote-uid",
                "remote-name",
                "remote-ebook-uid",
                "content",
                "icon-name",
                "start-time",
                "event-count",
                "group-title",
                "event-type",
                "outgoing",
                "flags",
                NULL);
        g_object_unref(el_iter);
    }

    if(values)
    {
        caching_data_t * d = NULL;
        const gchar * remote_ebook_uid;
        remote_ebook_uid = g_value_get_string(g_hash_table_lookup(values,
              "remote-ebook-uid"));
        if(remote_ebook_uid)
        {
            g_debug("The new event has a remote_ebook_uid (%s): creating the "
                    "aggregator if we don't have it already", remote_ebook_uid);
            if(priv->abook_aggregator == NULL && priv->create_aggregator)
            {
                _create_own_aggregator(model);
            }
        }

        d = g_new0(caching_data_t, 1);
        d->model = model;
        d->prepend = TRUE;
        d->cached_events = g_list_prepend(d->cached_events, values);

        priv->cached_n += MAX_CACHED_PER_QUERY;

        /* We do it immediately instead of the idle loop so
         * if event gets updated, the update won't come before
         * the event is added. */
        _stage_cached (d);
    }
}

static gboolean
_find_event (
    RTComLogModel *model,
    gint event_id,
    const gchar *local_uid,
    const gchar *remote_uid,
    const gchar *remote_ebook_uid,
    const gchar *group_uid,
    GtkTreeIter *retval,
    gint *event_id_retval)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    GtkTreeIter iter;
    gboolean found = FALSE;

    if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &iter, NULL))
        return FALSE;

    do
    {
        gint id_iter = -1;
        gchar *local_uid_iter = NULL;
        gchar *remote_uid_iter = NULL;
        gchar *group_uid_iter = NULL;
        gchar *remote_ebook_uid_iter = NULL;

        gtk_tree_model_get(
                GTK_TREE_MODEL(model), &iter,
                RTCOM_LOG_VIEW_COL_EVENT_ID, &id_iter,
                RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT, &local_uid_iter,
                RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &remote_uid_iter,
                RTCOM_LOG_VIEW_COL_ECONTACT_UID, &remote_ebook_uid_iter,
                RTCOM_LOG_VIEW_COL_GROUP_UID, &group_uid_iter,
                -1);

        g_debug ("[iterator: id %d, local %s, remote %s, group %s, ebook_uid %s]",
            id_iter, local_uid_iter, remote_uid_iter, group_uid_iter, remote_ebook_uid_iter);

        if(
          (id_iter == event_id) ||

          ((priv->group_by == RTCOM_EL_QUERY_GROUP_BY_GROUP) &&
           (group_uid_iter && group_uid &&
            !strcmp(group_uid_iter, group_uid))) ||

          ((priv->group_by == RTCOM_EL_QUERY_GROUP_BY_CONTACT) &&
           (remote_ebook_uid_iter && remote_ebook_uid &&
            strcmp(remote_ebook_uid_iter, remote_ebook_uid) == 0)) ||

          ((priv->group_by == RTCOM_EL_QUERY_GROUP_BY_UIDS) &&
           (local_uid_iter && local_uid && strcmp(local_uid_iter, local_uid) == 0) &&
           (remote_uid_iter && remote_uid && strcmp(remote_uid_iter, remote_uid) == 0)))
        {
            found = TRUE;
            *retval = iter;
            *event_id_retval = id_iter;
        }

        g_free (local_uid_iter);
        g_free (remote_uid_iter);
        g_free (group_uid_iter);
        g_free (remote_ebook_uid_iter);
    } while (!found && gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));

    return found;
}


static void
_event_updated_callback (
        RTComEl * backend,
        gint event_id,
        const gchar * local_uid,
        const gchar * remote_uid,
        const gchar * remote_ebook_uid,
        const gchar * group_uid,
        const gchar * service,
        RTComLogModel * model)
{
    RTComLogModelPrivate * priv;
    GtkTreeIter iter;
    RTComElQuery * query;
    RTComElIter * el_iter;
    gint id_iter;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    g_debug("%s: event with id=%d, remote_ebook_uid=%s and group_uid=%s "
            "got updated.", G_STRLOC, event_id, remote_ebook_uid, group_uid);

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    if (!priv->in_use)
        return;

    if (!_find_event(model, event_id, local_uid, remote_uid, remote_ebook_uid,
        group_uid, &iter, &id_iter))
      return;

    g_debug(G_STRLOC ": row found. Need to update icon and text.");

    query = rtcom_el_query_new(backend);
    /**
     * Setting the grouping property of the query is necessary
     * because even tho we're just getting a single event, we want
     * the resulting iterator to be aware of the fact that we're
     * dealing with groups. That's because the plugins will use
     * that value to deliver the correct icon.
     */
    rtcom_el_query_set_group_by(query, priv->group_by);
    if(!rtcom_el_query_prepare(
                    query,
                    "id", id_iter, RTCOM_EL_OP_EQUAL,
                    NULL))
    {
        g_warning("Couldn't prepare query");
        g_object_unref(query);
        return;
    }

    el_iter = rtcom_el_get_events(backend, query);
    if(el_iter && rtcom_el_iter_first(el_iter))
    {
        gchar *icon_name, *text, *remote_name, *group_title;
        guint event_count;

        if(rtcom_el_iter_get_values(el_iter,
                "icon-name", &icon_name,
                "content", &text,
                "remote-name", &remote_name,
                "event-count", &event_count,
                "group-title", &group_title,
                NULL))
        {
            GdkPixbuf * icon = NULL;

            g_debug("Got icon_name=\"%s\", text=\"%s\", "
                    "remote_name = \"%s\", event_count = %d, "
                    "group_title = \"%s\"",
                    icon_name, text, remote_name, event_count, group_title);
            if(icon_name)
            {
               icon = g_hash_table_lookup(
                        priv->cached_icons,
                        icon_name);
                if(!icon)
                {
                    icon = gtk_icon_theme_load_icon(
                            gtk_icon_theme_get_default(),
                            icon_name,
                            RTCOM_LOG_VIEW_ICON_SIZE,
                            0, NULL);
                    if(icon)
                        g_hash_table_insert(
                                priv->cached_icons,
                                g_strdup(icon_name),
                                icon);
                }
            }

            g_debug ("%s: setting icon %p (%s) for row",
                G_STRFUNC, icon, icon_name);

            gtk_list_store_set(
                    GTK_LIST_STORE(model),
                    &iter,
                    RTCOM_LOG_VIEW_COL_ICON, icon,
                    RTCOM_LOG_VIEW_COL_TEXT, text,
                    RTCOM_LOG_VIEW_COL_REMOTE_NAME, remote_name,
                    RTCOM_LOG_VIEW_COL_COUNT, event_count,
                    RTCOM_LOG_VIEW_COL_GROUP_TITLE, group_title,
                    -1);

            g_free (icon_name);
            g_free (text);
            g_free (remote_name);
            g_free (group_title);
        }

        g_object_unref(el_iter);
    }

    g_object_unref(query);
}

static void
_event_deleted_callback (
        RTComEl * backend,
        gint event_id,
        const gchar * local_uid,
        const gchar * remote_uid,
        const gchar * remote_ebook_uid,
        const gchar * group_uid,
        const gchar * service,
        RTComLogModel * model)
{
    RTComLogModelPrivate * priv;
    GtkTreeIter iter;
    gint id_iter;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    g_debug("%s: event with id=%d, "
            "local_uid=%s, remote_uid=%s, "
            "remote_ebook_uid=%s and group_uid=%s got deleted.",
            G_STRLOC,
            event_id, local_uid, remote_uid, remote_ebook_uid, group_uid);

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    if (!priv->in_use)
        return;

    if (!_find_event(model, event_id, local_uid, remote_uid, remote_ebook_uid,
        group_uid, &iter, &id_iter))
      return;

    switch (priv->group_by)
    {
        case RTCOM_EL_QUERY_GROUP_BY_NONE:
            /**
             * We're not grouping, so nothing easier than this: just delete
             * the row and forget about it! :)
             */
            g_debug(G_STRLOC ": row found. Need to delete it.");
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            break;

        case RTCOM_EL_QUERY_GROUP_BY_UIDS:
        {
            /**
             * We're grouping bt local and remote uid, and we found a row
             * that matches the local and remote uid of the event we want
             * to delete. There are two cases:
             * 1) the event deleted belongs to a combination of
             * local+remote uid that had only one event in the db. So we
             * can just make sure that the event_id matches and delete the
             * row;
             * 2) the event deleted belongs to a combination of
             * local+remote uid that has several events in the db. In this
             * case we need to redraw the row and possibly move it down.
             */

            gint number_of_events = rtcom_el_get_local_remote_uid_events_n(
                    backend, local_uid, remote_uid);

            g_debug("%s: the pair of local_ui=%s and remote_uid=%s "
                    "has %d associated events.",
                    G_STRLOC, local_uid, remote_uid, number_of_events);

            if(number_of_events == 0)
            {
                /**
                 * There's no more entries with that pair, so we can just
                 * remove the row and forget about it.
                 */

                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            }
            else
            {
                /**
                 * There are more events for that remote-ebook-uid in the
                 * database. We need to update this row and perhaps move it
                 * down. This case is quite different from when we're
                 * grouping by group-uid (see below), as we're not
                 * interested in calculating group flags or anything like
                 * that: we just want to see the most recent event for a
                 * contact. So, here, "updating" becomes more like delete
                 * this entry anyway, then query for the most recent event
                 * for that remote-ebook-uid, insert the event at the
                 * proper position. This last part will be performed in
                 * _stage_cached().
                 */

                RTComElQuery   * query;
                RTComElIter    * el_iter;
                GHashTable    * values;

                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

                query = rtcom_el_query_new(backend);
                rtcom_el_query_set_group_by(query, priv->group_by);
                if(!rtcom_el_query_prepare(
                            query,
                            "local-uid", local_uid, RTCOM_EL_OP_EQUAL,
                            "remote-uid", remote_uid, RTCOM_EL_OP_EQUAL,
                            NULL))
                {
                    g_warning("Couldn't prepare query");
                    g_object_unref(query);
                    return;
                }

                el_iter = rtcom_el_get_events(backend, query);
                if(el_iter && rtcom_el_iter_first(el_iter))
                {
                    caching_data_t * d;
                    RTComLogModelPrivate * priv;

                    values = rtcom_el_iter_get_value_map(
                            el_iter,
                            "id",
                            "service",
                            "group-uid",
                            "local-uid",
                            "remote-uid",
                            "remote-name",
                            "remote-ebook-uid",
                            "content",
                            "icon-name",
                            "start-time",
                            "event-count",
                            "group-title",
                            "event-type",
                            "outgoing",
                            "flags",
                            NULL);

                    d = g_new0(caching_data_t, 1);
                    d->model = model;
                    d->prepend = TRUE;
                    d->cached_events = g_list_prepend(d->cached_events, values);

                    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
                    priv->done_caching = TRUE;
                    priv->cached_n += MAX_CACHED_PER_QUERY;

                    g_idle_add((GSourceFunc) _stage_cached, d);
                }

                if(query)
                    g_object_unref(query);
                if(el_iter)
                    g_object_unref(el_iter);

                /**
                 * We can stop looking for it now.
                 */
                break;
            }
            break;
        }

        case RTCOM_EL_QUERY_GROUP_BY_CONTACT:
        {
            /**
             * We're grouping by contact and we found that this row of the
             * model has the same remote_ebook_uid than the one we want to
             * delete. Since, as just mentioned, we're grouping by contact,
             * there's two cases:
             * 1) the event deleted belongs to a contact that had only one
             * event in the database: in this case we can simply remote the
             * row;
             * 2) the event deleted belongs to a contact that had several
             * events in the database: in this case we have to redraw the
             * row and possibly move it down.
             */

            gint number_of_events = rtcom_el_get_contacts_events_n(
                    backend,
                    remote_ebook_uid);

            g_debug(G_STRLOC ": contact has %d associated events.",
                    number_of_events);

            if(number_of_events == 0)
            {
                /**
                 * There's no more entries with that remote-ebook-uid in
                 * the database. This means that the deleted one was the
                 * last one for that contact. Let's just remove the row and
                 * forget about it.
                 */

                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            }
            else
            {
                /**
                 * There are more events for that remote-ebook-uid in the
                 * database. We need to update this row and perhaps move it
                 * down. This case is quite different from when we're
                 * grouping by group-uid (see below), as we're not
                 * interested in calculating group flags or anything like
                 * that: we just want to see the most recent event for a
                 * contact. So, here, "updating" becomes more like delete
                 * this entry anyway, then query for the most recent event
                 * for that remote-ebook-uid, insert the event at the
                 * proper position. This last part will be performed in
                 * _stage_cached().
                 */

                RTComElQuery   * query;
                RTComElIter    * el_iter;
                GHashTable    * values;

                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

                query = rtcom_el_query_new(backend);
                rtcom_el_query_set_group_by(query, priv->group_by);
                if(!rtcom_el_query_prepare(
                            query,
                            "remote-ebook-uid", remote_ebook_uid, RTCOM_EL_OP_EQUAL,
                            NULL))
                {
                    g_warning("Couldn't prepare query");
                    g_object_unref(query);
                    return;
                }

                el_iter = rtcom_el_get_events(backend, query);
                if(el_iter && rtcom_el_iter_first(el_iter))
                {
                    caching_data_t * d;
                    RTComLogModelPrivate * priv;

                    values = rtcom_el_iter_get_value_map(
                            el_iter,
                            "id",
                            "service",
                            "group-uid",
                            "local-uid",
                            "remote-uid",
                            "remote-name",
                            "remote-ebook-uid",
                            "content",
                            "icon-name",
                            "start-time",
                            "event-count",
                            "group-title",
                            "event-type",
                            "outgoing",
                            "flags",
                            NULL);

                    d = g_new0(caching_data_t, 1);
                    d->model = model;
                    d->prepend = TRUE;
                    d->cached_events = g_list_prepend(d->cached_events, values);

                    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
                    priv->done_caching = TRUE;
                    priv->cached_n += MAX_CACHED_PER_QUERY;

                    g_idle_add((GSourceFunc) _stage_cached, d);
                }

                if(query)
                    g_object_unref(query);
                if(el_iter)
                    g_object_unref(el_iter);

                /**
                 * We can stop looking for it now.
                 */
                break;
            }
            break;
        }

        case RTCOM_EL_QUERY_GROUP_BY_GROUP:
        {
            /**
             * Since we're grouping, we have to compare the group_uids. Now
             * that we got our row, there's two cases we have to consider:
             * 1) the event deleted is somewhere in the middle of the
             * group, i.e. it's not the one representing this row (in other
             * words, it's not the event with the highest id in the group):
             * in this case we just update the icon and the text of the row
             * (because probably the group's flags will have changed, and
             * the icon will have changed)
             * 2) the event deleted is exactly the one that's representing
             * the group in the model: this is more complicated: we need to
             * update the icon and the text starting from the group_uid and
             * not from the id as usual, plus we have to see if the row
             * needs to be moved somehow down the model, because perhaps
             * the group is now "less recent" than some other group that
             * was displayed below it.
             */

            RTComElQuery * query;
            RTComElIter  * el_iter;
            gint           new_id = -1;
            gchar        * icon_name = NULL,
                         * text = NULL,
                         * remote_name = NULL;
            GdkPixbuf    * icon = NULL;
            gchar        * group_uid_iter = NULL;

            if(id_iter != event_id)
            {
                /**
                 * Nice: it seems that we're in the first case of the
                 * previous comment: this is the easy one.
                 * Let's just call the _event_updated_callback() function:
                 * this is not really optimal because it means traversing
                 * the model again, but we're saving duplicating some code
                 * and anyway O(2n) isn't much worse than O(n). Besides,
                 * event deletion doesn't happen in critical loops, so this
                 * is acceptable.
                 */
                _event_updated_callback(
                        backend,
                        rtcom_el_get_group_most_recent_event_id(backend, group_uid),
                        local_uid,
                        remote_uid,
                        remote_ebook_uid,
                        group_uid,
                        service,
                        model);
                break;
            }

            /**
             * Obviously we get here only if we'rein the second case of the
             * big comment block above: the hard one.
             */
            query = rtcom_el_query_new(backend);
            /**
             * Setting the grouping property of the query is necessary
             * because even tho we're just getting a single event, we want
             * the resulting iterator to be aware of the fact that we're
             * dealing with groups. That's because the plugins will use
             * that value to deliver the correct icon.
             */
            rtcom_el_query_set_group_by(query, priv->group_by);
            /**
             * Notice how this time we're querying on the group_uid. We
             * can't query on the id because that id doesn't exist anymore.
             */
            gtk_tree_model_get(
                    GTK_TREE_MODEL(model), &iter,
                    RTCOM_LOG_VIEW_COL_GROUP_UID, &group_uid_iter,
                  -1);

            if(!rtcom_el_query_prepare(
                            query,
                            "group-uid", group_uid_iter, RTCOM_EL_OP_EQUAL,
                            NULL))
            {
                g_warning("Couldn't prepare query");
                g_free(group_uid_iter);
                g_object_unref(query);
                return;
            }
            g_free(group_uid_iter);

            el_iter = rtcom_el_get_events(backend, query);
            /**
             * It's ok to never advance the iterator, because all we care
             * about is the first iterator, or, for the matter, any
             * iterator pointing to that group.
             */
            if(el_iter && rtcom_el_iter_first(el_iter))
            {
                gboolean outgoing;
                const gchar *event_type;
                gint flags;

                /**
                 * Now notice that we're getting the id from the iterator.
                 * Since queries are also performed sorting by descending
                 * id, this id we're getting is gonna be the highest in the
                 * group, hence the id of the event we're setting as
                 * group representative.
                 */

                if(rtcom_el_iter_get_values(
                        el_iter,
                        "id", &new_id,
                        "icon-name", &icon_name,
                        "content", &text,
                        "remote-name", &remote_name,
                        "event-type", &event_type,
                        "outgoing", &outgoing,
                        "flags", &flags,
                        NULL))
                {
                    GtkTreeIter resort_iter;

                    g_debug("Got id=%d, icon_name=\"%s\", text=\"%s\", "
                            "remote_name = \"%s\" and event_type = \"%s\".",
                            new_id, icon_name, text, remote_name, event_type);
                    if(icon_name)
                    {
                       icon = g_hash_table_lookup(
                                priv->cached_icons,
                                icon_name);
                        if(!icon)
                        {
                            icon = gtk_icon_theme_load_icon(
                                    gtk_icon_theme_get_default(),
                                    icon_name,
                                    RTCOM_LOG_VIEW_ICON_SIZE,
                                    0, NULL);
                            if(icon)
                                g_hash_table_insert(
                                        priv->cached_icons,
                                        g_strdup(icon_name),
                                        icon);
                        }
                    }

                    gtk_list_store_set(
                            GTK_LIST_STORE(model),
                            &iter,
                            RTCOM_LOG_VIEW_COL_EVENT_ID, new_id,
                            RTCOM_LOG_VIEW_COL_ICON, icon,
                            RTCOM_LOG_VIEW_COL_TEXT, text,
                            RTCOM_LOG_VIEW_COL_REMOTE_NAME, remote_name,
                            RTCOM_LOG_VIEW_COL_EVENT_TYPE, event_type,
                            RTCOM_LOG_VIEW_COL_OUTGOING, outgoing,
                            RTCOM_LOG_VIEW_COL_FLAGS, flags,
                            -1);

                    /**
                     * Alright, we updated the row, now it's time to see if
                     * we have to move it down.
                     */
                    resort_iter = iter;
                    while(gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &resort_iter))
                    {
                        gint id_resort_iter = -1;

                        gtk_tree_model_get(
                                GTK_TREE_MODEL(model), &resort_iter,
                                RTCOM_LOG_VIEW_COL_EVENT_ID, &id_resort_iter,
                                -1);
                        g_debug(G_STRLOC ": iterating resort_iter with id %d", id_resort_iter);
                        if(id_resort_iter > new_id)
                        {
                            /**
                             * Found it!
                             */
                            g_debug(G_STRLOC ": move it down!");
                            gtk_list_store_swap(
                                    GTK_LIST_STORE(model),
                                    &resort_iter,
                                    &iter);
                        }
                        else
                            break;
                    }

                    g_free (icon_name);
                    g_free (text);
                    g_free (remote_name);
                }

           }
            else
            {
                /**
                 * If we're here it means that the query didn't return any
                 * results, i.e. there's no more events in that group. So
                 * we can just delete the row!
                 */
                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
            }

            if(query)
                g_object_unref(query);
            if(el_iter)
                g_object_unref(el_iter);

            break;
        }
    }
}

static void
_all_deleted_callback(
        RTComEl * backend,
        const gchar * service,
        RTComLogModel * model)
{
    RTComLogModelPrivate * priv;
    guint i;

    /* Thanks, DBus. */
    if((service == NULL) || (*service == '\0'))
    {
        /* All the events in the database have been deleted, so we can
         * safely empty the model, whatever service it was filtering. */
        gtk_list_store_clear(GTK_LIST_STORE(model));
        return;
    }

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    if(priv->filtered_services == NULL)
    {
        /* We're showing all the events, so we gotta refresh */
        rtcom_log_model_refresh(model);
        return;
    }

    for(i = 0; priv->filtered_services[i] != NULL; ++i)
    {
        if(strcmp(service, priv->filtered_services[i]) == 0)
        {
            /* We are displaying events from that service, let's refresh */
            rtcom_log_model_refresh(model);
            break;
        }
    }
}

static gboolean
refresh_idle_cb (gpointer model)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    /* If refresh-hint was a result of modifying contacts, our
     * cache is stale. */
    g_hash_table_remove_all (priv->cached_account_data);

    rtcom_log_model_refresh(model);
    priv->refresh_id = 0;

    return FALSE;
}

static void
_refresh_hint_callback(
        RTComEl * backend,
        RTComLogModel * model)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    g_debug ("%s: called, refreshing the model", G_STRFUNC);

    if (!priv->in_use)
        return;

    /* We provide a 1s delay. If there's lots of updates in bulk,
     * the next refresh request is likely to come before that times out,
     * in which case we'll extend the period. The end effect is, we'll
     * do the update 1s after the last refresh request. */
    if (priv->refresh_id != 0)
    {
        g_source_remove (priv->refresh_id);
    }

    priv->refresh_id = g_timeout_add_seconds (1, refresh_idle_cb, model);
}

static void
rtcom_log_model_set_property(
        GObject * obj,
        guint prop_id,
        const GValue * value,
        GParamSpec * pspec)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_LOG_MODEL_PROP_CREATE_AGGREGATOR:
            priv->create_aggregator = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void rtcom_log_model_get_property(
        GObject * obj,
        guint prop_id,
        GValue * value,
        GParamSpec * pspec)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_LOG_MODEL_PROP_CREATE_AGGREGATOR:
            g_value_set_boolean(value, priv->create_aggregator);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void
rtcom_log_model_class_init(
        RTComLogModelClass * klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    g_type_class_add_private(object_class, sizeof(RTComLogModelPrivate));
    object_class->dispose = rtcom_log_model_dispose;
    object_class->finalize = rtcom_log_model_finalize;
    object_class->get_property = rtcom_log_model_get_property;
    object_class->set_property = rtcom_log_model_set_property;

    g_object_class_install_property(
            object_class,
            RTCOM_LOG_MODEL_PROP_CREATE_AGGREGATOR,
            g_param_spec_boolean(
                "create-aggregator",
                "Create aggregator",
                "Create our own aggregator",
                TRUE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    presence_need_redraw_signal_id = g_signal_new(
            "presence-need-redraw",
            G_TYPE_FROM_CLASS(object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    avatar_need_redraw_signal_id = g_signal_new(
            "avatar-need-redraw",
            G_TYPE_FROM_CLASS(object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
}

static void _create_abook_account_manager (RTComLogModel *model);

static void
_name_order_changed_cb (
    GConfClient *client,
    guint        notify_id,
    GConfEntry  *entry,
    gpointer     data)
{
    RTComLogModel *model = RTCOM_LOG_MODEL (data);
    gboolean valid;
    GtkTreeIter iter;

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);
    while (valid)
    {
        OssoABookContact *contact;

        gtk_tree_model_get(GTK_TREE_MODEL (model), &iter,
            RTCOM_LOG_VIEW_COL_CONTACT, &contact, -1);

        if (contact)
        {
            const gchar *name =
                osso_abook_contact_get_display_name (contact);

            if (name != NULL)
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                    RTCOM_LOG_VIEW_COL_REMOTE_NAME, name, -1);

            g_object_unref (contact);
        }

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter);
    }
}

static void
rtcom_log_model_init(
        RTComLogModel * log_model)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(log_model);

    GType types[] =
    {
        RTCOM_LOG_VIEW_COL_TYPE_1,
        RTCOM_LOG_VIEW_COL_TYPE_2,
        RTCOM_LOG_VIEW_COL_TYPE_3,
        RTCOM_LOG_VIEW_COL_TYPE_4,
        RTCOM_LOG_VIEW_COL_TYPE_5,
        RTCOM_LOG_VIEW_COL_TYPE_6,
        RTCOM_LOG_VIEW_COL_TYPE_7,
        RTCOM_LOG_VIEW_COL_TYPE_8,
        RTCOM_LOG_VIEW_COL_TYPE_9,
        RTCOM_LOG_VIEW_COL_TYPE_10,
        RTCOM_LOG_VIEW_COL_TYPE_11,
        RTCOM_LOG_VIEW_COL_TYPE_12,
        RTCOM_LOG_VIEW_COL_TYPE_13,
        RTCOM_LOG_VIEW_COL_TYPE_14,
        RTCOM_LOG_VIEW_COL_TYPE_15,
        RTCOM_LOG_VIEW_COL_TYPE_16,
        RTCOM_LOG_VIEW_COL_TYPE_17
    };

    gtk_list_store_set_column_types(
            GTK_LIST_STORE(log_model),
            RTCOM_LOG_VIEW_COL_SIZE, types);

    priv->backend = rtcom_el_new();
    priv->current_query = NULL;
    priv->filtered_services = NULL;

    priv->cached_n = 0;
    priv->done_caching = FALSE;

    priv->cache_thread = NULL;
    priv->pixbufs_thread = NULL;
    priv->cancel_threads = FALSE;

    priv->cached_icons = g_hash_table_new_full(
            g_str_hash, g_str_equal,
            (GDestroyNotify) g_free, (GDestroyNotify) g_object_unref);
    priv->cached_service_icons = g_hash_table_new_full(
            g_str_hash, g_str_equal,
            (GDestroyNotify) g_free, (GDestroyNotify) g_object_unref);

    priv->cached_account_data =
        g_hash_table_new_full(
                g_str_hash, g_str_equal,
                (GDestroyNotify) g_free, (GDestroyNotify) _account_data_free);

    priv->group_by = RTCOM_EL_QUERY_GROUP_BY_NONE;
    priv->limit = -1;
    priv->show_group_chat = FALSE;

    priv->aggregator_ready_closure = NULL;
    priv->abook_aggregator = NULL;
    priv->abook_subs = NULL;
    priv->own_aggregator = FALSE;
    priv->abook_aggregator_ready = FALSE;
    priv->pixbufs_populated = FALSE;
    priv->in_use = FALSE;

    priv->vcard_field_mapping = g_hash_table_new_full (g_str_hash,
        g_str_equal, g_free, g_free);

    priv->new_event_handler = g_signal_connect(
            G_OBJECT(priv->backend),
            "new-event",
            (GCallback) _new_event_callback,
            log_model);

    priv->event_updated_handler = g_signal_connect(
            G_OBJECT(priv->backend),
            "event-updated",
            (GCallback) _event_updated_callback,
            log_model);

    priv->event_deleted_handler = g_signal_connect(
            G_OBJECT(priv->backend),
            "event-deleted",
            (GCallback) _event_deleted_callback,
            log_model);

    priv->all_deleted_handler = g_signal_connect(
            G_OBJECT(priv->backend),
            "all-deleted",
            (GCallback) _all_deleted_callback,
            log_model);

    priv->refresh_hint_handler = g_signal_connect(
            G_OBJECT(priv->backend),
            "refresh-hint",
            (GCallback) _refresh_hint_callback,
            log_model);


    if(!g_thread_supported())
        g_thread_init(NULL);

    _create_abook_account_manager (log_model);

    priv->gconf_client = gconf_client_get_default ();
    priv->display_order_notify_id = 0;

    if (priv->gconf_client != NULL)
    {
        priv->display_order_notify_id = gconf_client_notify_add (
            priv->gconf_client, OSSO_ABOOK_SETTINGS_KEY_NAME_ORDER,
            _name_order_changed_cb, log_model, NULL, NULL);
    }

    g_debug("Model initialization completed successfully.");
}

static void
rtcom_log_model_dispose(
        GObject * obj)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(obj);

    _priv_cancel_and_join_threads (priv);

    if (priv->refresh_id)
    {
        g_source_remove (priv->refresh_id);
        priv->refresh_id = 0;
    }

    if(priv->current_query)
    {
        g_debug(G_STRLOC ": unreffing the current query...");
        g_object_unref(priv->current_query);
        priv->current_query = NULL;
    }

    if(priv->backend)
    {
        g_debug(G_STRLOC ": unreffing the backend...");

        g_signal_handler_disconnect(priv->backend, priv->new_event_handler);
        g_signal_handler_disconnect(priv->backend, priv->event_updated_handler);
        g_signal_handler_disconnect(priv->backend, priv->event_deleted_handler);
        g_signal_handler_disconnect(priv->backend, priv->all_deleted_handler);
        g_signal_handler_disconnect(priv->backend, priv->refresh_hint_handler);

        g_object_unref(priv->backend);
        priv->backend = NULL;
    }

    if(priv->abook_subs)
    {
        g_debug(G_STRLOC ": unreffing the abook subscriptions...");
        g_object_unref(priv->abook_subs);
        priv->abook_subs = NULL;
    }

    if(priv->abook_aggregator)
    {
        if (priv->aggregator_ready_closure)
        {
            osso_abook_waitable_cancel (OSSO_ABOOK_WAITABLE(priv->abook_aggregator),
                priv->aggregator_ready_closure);
            priv->aggregator_ready_closure = NULL;
        }

        g_debug(G_STRLOC ": unreffing the aggregator...");
        g_object_unref(priv->abook_aggregator);
        priv->abook_aggregator = NULL;
    }

    if (priv->accman_ready_closure)
    {
        osso_abook_waitable_cancel (OSSO_ABOOK_WAITABLE(priv->account_manager),
            priv->accman_ready_closure);
        priv->accman_ready_closure = NULL;
    }

    g_debug(G_STRLOC ": unreffing the account manager...");
    g_object_unref(priv->account_manager);
    priv->account_manager = NULL;

    if (priv->display_order_notify_id)
        gconf_client_notify_remove (priv->gconf_client,
            priv->display_order_notify_id);
    priv->display_order_notify_id = 0;

    if (priv->gconf_client)
        g_object_unref (priv->gconf_client);
    priv->gconf_client = NULL;

    g_hash_table_destroy (priv->vcard_field_mapping);

    G_OBJECT_CLASS(rtcom_log_model_parent_class)->dispose(obj);
}

static void
rtcom_log_model_finalize(
        GObject * obj)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(obj);

    g_debug("Finalizing the model...");

    if(priv->filtered_services)
    {
        guint i;

        for(i = 0; priv->filtered_services[i]; i++)
        {
            g_free(priv->filtered_services[i]);
        }
        g_slice_free1(
                sizeof(priv->filtered_services),
                priv->filtered_services);
    }

    g_hash_table_destroy(priv->cached_icons);
    g_hash_table_destroy(priv->cached_service_icons);
    g_hash_table_destroy(priv->cached_account_data);

    G_OBJECT_CLASS(rtcom_log_model_parent_class)->finalize(obj);
}

RTComLogModel *
rtcom_log_model_new()
{
    return RTCOM_LOG_MODEL(g_object_new(RTCOM_LOG_MODEL_TYPE, NULL));
}

RTComEl *
rtcom_log_model_get_eventlogger(
        RTComLogModel * model)
{
    RTComLogModelPrivate * priv = NULL;

    g_return_val_if_fail(RTCOM_IS_LOG_MODEL(model), NULL);

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    return priv->backend;
}

void
rtcom_log_model_populate(
        RTComLogModel * model,
        const gchar * services[])
{
    RTComLogModelPrivate * priv = NULL;
    gboolean query_prepared = FALSE;
    RTComElQuery *query = NULL;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    g_return_if_fail(RTCOM_IS_EL(priv->backend));

    priv->in_use = TRUE;

    query = rtcom_el_query_new(priv->backend);
    rtcom_el_query_set_group_by(query, priv->group_by);

    rtcom_el_query_set_limit(query, priv->limit);
    rtcom_el_query_set_offset(query, 0);

    if(services)
    {
        guint i, size;

        if(services != (const gchar **) priv->filtered_services)
        {
            if(priv->filtered_services)
            {
                for(i = 0; priv->filtered_services[i]; i++)
                {
                    g_free(priv->filtered_services[i]);
                }
                g_slice_free1(sizeof(priv->filtered_services),
                              priv->filtered_services);
                priv->filtered_services = NULL;
            }

            for(size= 0; services[size] != NULL; size++);

            priv->filtered_services =
                g_malloc0((size + 1) * sizeof(gchar *));

            for(i = 0; i < size; i++)
            {
                priv->filtered_services[i] = g_strdup(services[i]);
            }
        }

        query_prepared = rtcom_el_query_prepare(
                query,
                "service", priv->filtered_services, RTCOM_EL_OP_IN_STRV,
                NULL);
    }
    else
    {
        if(priv->filtered_services)
        {
            guint i;

            for(i = 0; priv->filtered_services[i]; i++)
            {
                g_free(priv->filtered_services[i]);
            }
            g_free(priv->filtered_services);
            priv->filtered_services = NULL;
        }

        query_prepared = rtcom_el_query_prepare(query, NULL);
    }

    if(!query_prepared)
    {
        g_warning("Couldn't prepare query");
        g_object_unref(query);
        query = NULL;
        return;
    }

    rtcom_log_model_populate_query(model, query);
    g_object_unref (query);
}


void
rtcom_log_model_populate_query(
        RTComLogModel * model,
        RTComElQuery *query)
{
    RTComLogModelPrivate * priv = NULL;
    RTComElIter * it = NULL;
    GHashTable * values = NULL;
    caching_data_t * d = NULL;
    guint event_count = 0;
    gint limit;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    g_return_if_fail(RTCOM_IS_EL(priv->backend));

    priv->in_use = TRUE;

    rtcom_log_model_clear (model);

    if(priv->current_query != NULL)
    {
        g_debug("Unreffing the previous query.");
        g_object_unref(priv->current_query);
    }

    priv->current_query = g_object_ref(query);

    /* We override the current query limit so we can
     * load the data in stages. If the user wants to
     * limit the number of results, they should do so
     * using rtcom_log_model_set_limit() instead. */
    limit = MAX_CACHED_PER_QUERY_FIRST;
    if ((priv->limit != -1) && (priv->limit < limit))
        limit = priv->limit;

    rtcom_el_query_set_limit(query, limit);
    rtcom_el_query_set_offset(query, 0);
    rtcom_el_query_refresh(query);

    it = rtcom_el_get_events(priv->backend, priv->current_query);

    d = g_new0(caching_data_t, 1);
    d->model = model;
    d->prepend = FALSE;

    priv->cached_n = 0;
    if(it && rtcom_el_iter_first(it))
    {
        do
        {
            event_count++;
            values = rtcom_el_iter_get_value_map(
                    it,
                    "id",
                    "service",
                    "group-uid",
                    "local-uid",
                    "remote-uid",
                    "remote-name",
                    "remote-ebook-uid",
                    "content",
                    "icon-name",
                    "start-time",
                    "event-count",
                    "group-title",
                    "event-type",
                    "outgoing",
                    "flags",
                    NULL);
            d->cached_events = g_list_prepend(d->cached_events, values);
        } while(rtcom_el_iter_next(it));

        g_object_unref(it);
    }

    priv->cached_n += event_count;

    /* Create the aggregator now, if it doesn't exist already, before
     * starging to stage the events.
     */
    if(priv->abook_aggregator == NULL && priv->create_aggregator)
    {
        _create_own_aggregator(model);
    }

    d->cached_events = g_list_reverse(d->cached_events);
    _stage_cached(d);

    if ((priv->cached_n == MAX_CACHED_PER_QUERY_FIRST) &&
        (priv->limit > priv->cached_n))
    {
        priv->cancel_threads = FALSE;
        priv->done_caching = FALSE;
        priv->cache_thread = g_thread_create(
                (GThreadFunc) _threaded_cached_load,
                model,
                TRUE, NULL);
    }
}

void
rtcom_log_model_clear(RTComLogModel *model)
{
    RTComLogModelPrivate * priv = NULL;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    _priv_cancel_and_join_threads (priv);

    g_debug("%s: clearing the list store", G_STRFUNC);
    gtk_list_store_clear(GTK_LIST_STORE(model));
}

void
rtcom_log_model_refresh(
        RTComLogModel * model)
{
    RTComLogModelPrivate * priv;
    RTComElQuery *query;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    if (priv->current_query == NULL)
        return;

    g_debug("%s: repopulating the model", G_STRFUNC);

    query = g_object_ref (priv->current_query);
    rtcom_log_model_populate_query (model, query);
    g_object_unref (query);
}

void
rtcom_log_model_set_group_by(
        RTComLogModel * model,
        RTComElQueryGroupBy group_by)
{
    RTComLogModelPrivate * priv = NULL;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));
    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    priv->group_by = group_by;
}

void
rtcom_log_model_set_limit(
      RTComLogModel *model,
      gint limit)
{
    RTComLogModelPrivate * priv = NULL;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));
    priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    priv->limit = limit;
}

void
rtcom_log_model_set_abook_aggregator(
        RTComLogModel * model,
        OssoABookAggregator * aggregator)
{
    RTComLogModelPrivate * priv;

    g_return_if_fail(RTCOM_IS_LOG_MODEL(model));

    priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    /* Some confusion in the API lead to clients setting aggregator
     * without clearing create aggregator flag. It's mutually exclusive
     * anyways. */
    if (aggregator != NULL)
        priv->create_aggregator = FALSE;

    if(priv->create_aggregator)
    {
        _create_own_aggregator(model);
    }
    else if(aggregator != NULL)
    {
        g_object_ref(aggregator);
        priv->abook_aggregator = aggregator;
        priv->own_aggregator = FALSE;

        if (osso_abook_waitable_is_ready(OSSO_ABOOK_WAITABLE(aggregator), NULL))
        {
          g_debug ("%s: aggregator state is ready", G_STRFUNC);
          priv->abook_aggregator_ready = TRUE;
          _abook_aggregator_ready(
                  OSSO_ABOOK_WAITABLE(aggregator),
                  NULL,
                  model);
          _connect_aggregator_signals(model);
        }
        else
        {
          g_debug ("%s: aggregator state is NOT ready", G_STRFUNC);
          priv->abook_aggregator_ready = FALSE;
          _connect_aggregator_signals(model);
        }
    }
}

static const GdkPixbuf *
_get_service_icon (RTComLogModel *model, const gchar *local_uid)
{
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    McAccount *acc;
    McProfile *profile;
    const gchar *id;
    const gchar *icon_name;
    GtkIconTheme * icon_theme;
    GdkPixbuf * service_icon;

    if (!osso_abook_waitable_is_ready(
        OSSO_ABOOK_WAITABLE(priv->account_manager), NULL))
      return NULL;

    acc = osso_abook_account_manager_lookup_by_name (priv->account_manager,
        local_uid);

    if (!acc)
      return NULL;

    id = mc_account_compat_get_profile (acc);

    profile = mc_profile_lookup (id);

    if (!profile)
      return NULL;

    icon_name = mc_profile_get_branding_icon_name (profile);

    if (!icon_name)
        icon_name = mc_profile_get_icon_name (profile);

    g_object_unref (profile);

    if (!icon_name)
      return NULL;

    icon_theme = gtk_icon_theme_get_default();

    service_icon = g_hash_table_lookup(
            priv->cached_service_icons,
            icon_name);

    if(!service_icon)
    {
        GdkPixbuf * tmp;

        tmp = gtk_icon_theme_load_icon(
                icon_theme,
                icon_name,
                HILDON_ICON_PIXEL_SIZE_SMALL,
                0, NULL);

        service_icon = gdk_pixbuf_scale_simple(
                tmp,
                HILDON_ICON_PIXEL_SIZE_SMALL,
                HILDON_ICON_PIXEL_SIZE_SMALL,
                GDK_INTERP_NEAREST);

        g_object_unref(tmp);

        g_hash_table_insert(
                priv->cached_service_icons,
                g_strdup(icon_name),
                service_icon);
    }

    g_debug ("%s: got icon %s for account %s", G_STRFUNC,
        icon_name, local_uid);

    return service_icon;
}

static void
_account_manager_ready(
      OssoABookWaitable *waitable,
      const GError *error,
      gpointer data)
{
    RTComLogModel *model = RTCOM_LOG_MODEL(data);
    RTComLogModelPrivate * priv = RTCOM_LOG_MODEL_GET_PRIV(model);
    GtkTreeIter iter;
    gboolean valid;

    g_debug ("%s: called, filling in service icons", G_STRFUNC);

    priv->accman_ready_closure = NULL;

    if (error)
        return;

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter);
    while(valid)
    {
        gchar *local_uid;
        GdkPixbuf *icon;

        gtk_tree_model_get(
                GTK_TREE_MODEL(model), &iter,
                RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT, &local_uid,
                RTCOM_LOG_VIEW_COL_SERVICE_ICON, &icon,
                -1);

        if (icon == NULL)
        {
            icon = (GdkPixbuf *) _get_service_icon (model, local_uid);

            if (icon != NULL)
            {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                    RTCOM_LOG_VIEW_COL_SERVICE_ICON, icon, -1);
            }
        }
        else
        {
            g_object_unref (icon);
        }

        g_free (local_uid);

        valid = gtk_tree_model_iter_next(
                GTK_TREE_MODEL(model),
                &iter);
    }
}

static void
_create_abook_account_manager (RTComLogModel *model)
{
    RTComLogModelPrivate *priv = RTCOM_LOG_MODEL_GET_PRIV(model);

    g_assert (priv->account_manager == NULL);

    priv->account_manager = g_object_ref
        (osso_abook_account_manager_get_default ());
    priv->accman_ready_closure = NULL;

    if (!osso_abook_waitable_is_ready(
          OSSO_ABOOK_WAITABLE(priv->account_manager), NULL))
    {
      priv->accman_ready_closure =
          osso_abook_waitable_call_when_ready (OSSO_ABOOK_WAITABLE(priv->account_manager),
              _account_manager_ready, model, NULL);

    }
}

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
rtcom_log_model_filter_visible_func (GtkTreeModel *model,
    GtkTreeIter *iter, gchar *text, gpointer data)
{
    glong len = 0;
    gssize size;
    gchar * displayed_name;
    gboolean result = FALSE;
    gchar * haystack;
    gchar * haypart;
    gchar * needle;
    gchar * needle_part;
    gchar * needle_token;

    g_debug ("%s: called for %s", G_STRFUNC, text);

    len = g_utf8_strlen(text, -1);

    if(len == 0)
        return TRUE;

    gtk_tree_model_get(model, iter,
            RTCOM_LOG_VIEW_COL_REMOTE_NAME, &displayed_name,
            -1);

    if (!displayed_name)
        gtk_tree_model_get (model, iter,
            RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT, &displayed_name, -1);

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

void
rtcom_log_model_set_show_group_chat (
        RTComLogModel * model,
        gboolean is_shown)
{
    RTComLogModelPrivate *priv;

    g_return_if_fail (RTCOM_IS_LOG_MODEL (model));
    priv = RTCOM_LOG_MODEL_GET_PRIV (model);

    priv->show_group_chat = is_shown;
    rtcom_log_model_refresh(model);
}


/* vim: set ai et tw=75 ts=4 sw=4: */
