/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006 Nokia Corporation
 * Contact: Onne Gorter <onne.gorter@nokia.com>
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

#ifndef __UTF8_H__
#define __UTF8_H__

#include <glib.h>

G_BEGIN_DECLS

gunichar *
utf8_strcasestrip           (const char     *str);

const char *
utf8_strstrcasestrip        (const char     *haystack,
                             const gunichar *needle,
                             int            *end);
gboolean
utf8_strstartswithcasestrip (const char     *a,
                             const gunichar *b,
                             int            *end);

G_END_DECLS

#endif /* __UTF8_H__ */
