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
 * @file rtcom-log-columns.h
 * @brief Columns used in the widget.
 */

#ifndef __RTCOM_LOG_COLUMNS_H
#define __RTCOM_LOG_COLUMNS_H

#include <glib.h>
#include <gdk/gdk.h>

#include <libosso-abook/osso-abook-presence.h>

enum {
    /* Visible columns */
    RTCOM_LOG_VIEW_COL_ICON,
    RTCOM_LOG_VIEW_COL_TEXT,
    RTCOM_LOG_VIEW_COL_CONTACT, /* Used to retrieve presence and avatar */
    RTCOM_LOG_VIEW_COL_SERVICE_ICON,

    /* Here follow columns with data that
     * the user of the model might wanna access */
    RTCOM_LOG_VIEW_COL_LOCAL_ACCOUNT,
    RTCOM_LOG_VIEW_COL_REMOTE_ACCOUNT,
    RTCOM_LOG_VIEW_COL_REMOTE_NAME,
    RTCOM_LOG_VIEW_COL_ECONTACT_UID,
    RTCOM_LOG_VIEW_COL_EVENT_ID,
    RTCOM_LOG_VIEW_COL_SERVICE,
    RTCOM_LOG_VIEW_COL_GROUP_UID,

    RTCOM_LOG_VIEW_COL_TIMESTAMP,
    RTCOM_LOG_VIEW_COL_END_TIMESTAMP,
    RTCOM_LOG_VIEW_COL_COUNT,
    RTCOM_LOG_VIEW_COL_GROUP_TITLE,
    RTCOM_LOG_VIEW_COL_EVENT_TYPE,

    RTCOM_LOG_VIEW_COL_OUTGOING,
    RTCOM_LOG_VIEW_COL_FLAGS,

    RTCOM_LOG_VIEW_COL_SIZE
};

#define RTCOM_LOG_VIEW_COL_TYPE_1  GDK_TYPE_PIXBUF
#define RTCOM_LOG_VIEW_COL_TYPE_2  G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_3  OSSO_ABOOK_TYPE_CONTACT
#define RTCOM_LOG_VIEW_COL_TYPE_4  GDK_TYPE_PIXBUF

#define RTCOM_LOG_VIEW_COL_TYPE_5  G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_6  G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_7  G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_8  G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_9  G_TYPE_INT
#define RTCOM_LOG_VIEW_COL_TYPE_10 G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_11 G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_12 G_TYPE_INT
#define RTCOM_LOG_VIEW_COL_TYPE_13 G_TYPE_INT
#define RTCOM_LOG_VIEW_COL_TYPE_14 G_TYPE_INT
#define RTCOM_LOG_VIEW_COL_TYPE_15 G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_16 G_TYPE_STRING
#define RTCOM_LOG_VIEW_COL_TYPE_17 G_TYPE_BOOLEAN
#define RTCOM_LOG_VIEW_COL_TYPE_18 G_TYPE_INT

#define RTCOM_LOG_VIEW_COL_ICON_WIDTH 56
#define RTCOM_LOG_VIEW_COL_TEXT_WIDTH 400
#define RTCOM_LOG_VIEW_COL_PRESENCE_WIDTH 24
#define RTCOM_LOG_VIEW_COL_SERVICE_ICON_WIDTH 48
#define RTCOM_LOG_VIEW_COL_AVATAR_WIDTH 56

#define RTCOM_LOG_VIEW_ICON_SIZE   48
#define RTCOM_LOG_VIEW_AVATAR_SIZE 48

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */
