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

#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <libosso.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>


G_BEGIN_DECLS


typedef enum {
  ALL,
  VOIP,
  GSM,
  OTHER
} call_types;

#define DEFAULT_LIMIT 30

typedef struct
{
	gint limit;
	gint default_type;

} AppSettings;

gboolean get_settings(AppSettings *settings);


gboolean set_limit(gint limit);
gint get_limit();

gboolean set_default_type(gint default_type);
gint get_default_type();

G_END_DECLS

#endif
