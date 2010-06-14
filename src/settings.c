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
 Name        : settings.c
 Author      : Matrim
 Version     : 0.3
 Description : Settings
 ============================================================================
 */

#include "settings.h"

gboolean set_limit(gint limit)
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

gint get_limit()
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


gboolean set_default_type(gint type)
{
	/* Get the default client */
	GConfClient *client = gconf_client_get_default();

	gconf_client_add_dir (client, "/apps/extcalllog",
	                GCONF_CLIENT_PRELOAD_NONE, NULL);

	if(type < 0 || type >= OTHER)
		type = ALL;

	return gconf_client_set_int (client,
			"/apps/extcalllog/default_type",
            type,
            NULL);
}

gint get_default_type()
{
	gint type;
	/* Get the default client */
	GConfClient *client = gconf_client_get_default();

	/*Add GConf node if absent*/
	/*if(!gconf_client_dir_exists(client, GCONF_NODE, NULL))
	{*/
	gconf_client_add_dir (client, "/apps/extcalllog",
	                GCONF_CLIENT_PRELOAD_NONE, NULL);

	type = gconf_client_get_int(client,"/apps/extcalllog/default_type", NULL);

	if(type < 0 || type >= OTHER)
	{
		type = ALL;
		set_default_type(type);
	}

	return type;

}


