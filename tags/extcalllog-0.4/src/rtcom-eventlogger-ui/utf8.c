/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (C) 2006 Nokia Corporation
 * Author: Tomas Frydrych <tf@openedhand.com>
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "utf8.h"

/*!
   Lowercases the string and strips any marks.

   Caller must g_free() the returned string when no longer
   needed.
*/

#define MY_UC_BUFF_SIZE 20
gunichar *
utf8_strcasestrip (const char *str)
{
  int i = 0;
  const char *p = str;
  GUnicodeType t;
  gunichar c, * str2;
  size_t len;
  gsize result_len, j;


#if HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
  gsize buf_len = MY_UC_BUFF_SIZE;
  gunichar buf_static[MY_UC_BUFF_SIZE];
  gunichar * buf = &buf_static[0];;
#else
  gunichar * buf = NULL;
#endif

  /* sanity check*/
  if(!str || !*str)
    return NULL;

  /*
     Allocate adequate buffer for the output -- since we do not store many of
     these at any given time and the strings are not very long, we simply
     allocate a unichar for each byte; this is guaranteed to be >= of the
     required length (at most 6 * bigger than needed, but in real world
     typically at most 3 * bigger).
   */

  len = strlen(str);

  str2 = (gunichar*) g_malloc(sizeof(gunichar) * (len + 1));

  while(p && p < str + len)
    {
      c = g_utf8_get_char(p);

#if HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
      if(!g_unicode_canonical_decomposition_to_buffer (c,buf,buf_len,
                                                       &result_len)
         && result_len)
        {
          buf = (gunichar*)alloca(result_len*sizeof(gunichar));
          buf_len = result_len;

          if(!g_unicode_canonical_decomposition_to_buffer (c,buf,buf_len,
                                                           &result_len))
            {
              /* something badly wrong here ...*/
              g_free(str2);
              return NULL;
            }
        }
#else
      g_free(buf);
      buf = g_unicode_canonical_decomposition (c, &result_len);
#endif

      for(j = 0; j < result_len; ++j)
        {
          if(i == len)
            {
              /*
                We have run out of our buffer; this should only happen when
                dealing with hangul characters, and even then not that often (we
                allocated 3 unichars per each utf8 char in the hangul range, and
                most hangul points decompose into 3 characters.  We will realloc
                using linear increase)
              */
              len += 10;
              str2 = (gunichar*)g_realloc(str2, sizeof(gunichar) * len);
              if(!str2)
                return NULL;
            }

          t = g_unichar_type(buf[j]);

          switch (t)
            {
            case G_UNICODE_UPPERCASE_LETTER:
            case G_UNICODE_TITLECASE_LETTER:
              str2[i++] = g_unichar_tolower(buf[j]);
              break;

            case G_UNICODE_COMBINING_MARK:
            case G_UNICODE_ENCLOSING_MARK:
            case G_UNICODE_NON_SPACING_MARK:
              /*strip marks -- do nothing */
              break;

            default:
              str2[i++] = buf[j];
            }
        }

      p = g_utf8_next_char(p);
    }

#ifndef HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
  g_free(buf);
#endif

  str2[i] = 0;
  return str2;
}

/*!
   Find needle in the haystack (like strstr())

   needle must be already processed using
   utf8_strcasestrip()

   if return_value != NULL then (return_value + *end ) points at the
   the character in haystack that is immediately after the needle.
*/
const char *
utf8_strstrcasestrip (const char     *haystack,
                      const gunichar *needle,
                      int            *end)
{
  const gunichar * n = needle;
  const char * h = haystack, * n_start = NULL, * n_possible = NULL;
  GUnicodeType t;
  gunichar c;
  gsize result_len, j;

#if HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
  gsize buf_len = MY_UC_BUFF_SIZE;
  gunichar buf_static[MY_UC_BUFF_SIZE];
  gunichar * buf = &buf_static[0];;
#else
  gunichar * buf = NULL;
#endif

  if(!haystack || !*haystack || !needle || !*needle)
    return NULL;

  while(*h && *n)
    {
      c = g_utf8_get_char(h);

#if HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
      if(!g_unicode_canonical_decomposition_to_buffer (c,buf,buf_len,
                                                       &result_len)
         && result_len)
        {
          buf = (gunichar*)alloca(result_len*sizeof(gunichar));
          buf_len = result_len;

          if(!g_unicode_canonical_decomposition_to_buffer (c,buf,buf_len,
                                                           &result_len))
            {
              /* something badly wrong here ...*/
              return NULL;
            }
        }
#else
      g_free(buf);
      buf = g_unicode_canonical_decomposition (c, &result_len);
#endif
      for(j = 0; j < result_len; ++j)
        {
          t = g_unichar_type(buf[j]);

          switch (t)
            {
            case G_UNICODE_COMBINING_MARK:
            case G_UNICODE_ENCLOSING_MARK:
            case G_UNICODE_NON_SPACING_MARK:
              /*strip marks -- do nothing */
              break;

            case G_UNICODE_UPPERCASE_LETTER:
            case G_UNICODE_TITLECASE_LETTER:
              buf[j] = g_unichar_tolower(buf[j]);
              /* fall through */
            default:
              /* just use c as is */
              if(buf[j] == *n)
                {
                  if(n == needle)
                    {
                      /* just starting the comparison -- remember where we are */
                      n_start = h;
                      n_possible = NULL;
                    }
                  else if(!n_possible && buf[j] == *needle)
                    {
                      /*
                        encountered next occurence of the first char of our
                        needle -- remember it, so we can skip the intervening
                        section should the current attempt fail
                      */
                      n_possible = h;
                    }

                  /* use next character in needle in next step */
                  n++;

                  if(!*n)
                    {
                      /* end of needle -- we have a match */
#ifndef HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
                      g_free(buf);
#endif
                      if(end)
                        *end = g_utf8_next_char(h) - haystack;

                      return n_start;
                    }
                }
              else
                {
                  /* reset to start of needle */
                  if(n_possible)
                    {
                      /*
                        was not match but we know where next possible
                        start of needle is
                      */
                      h = n_start = n_possible;
                      n_possible = NULL;

                      /*
                        no need to compare the first char (h will get automatically
                        advanced below at the end of the loop)
                      */
                      n = needle + 1;
                    }
                  else
                    {
                      /* move back to needle start */
                      if(n_start)
                        {
                          h = n_start;
                          n_start = NULL;
                        }

                      n = needle;
                    }
                }
              break;
            }
        }
      h = g_utf8_next_char(h);
    }

#ifndef HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
  g_free(buf);
#endif

  return NULL;
}

/*! like !strncmp (a, b, strlen (a))

   if return_value != NULL then (return_value + *end ) points at the
   the character in haystack that is immediately after the needle.
*/
gboolean
utf8_strstartswithcasestrip (const char     *a,
                             const gunichar *b,
                             int            *end)
{
  const char * h = a;
  const gunichar * n = b;
  GUnicodeType t;
  gunichar c;
  gsize result_len, j;

#if HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
  gsize buf_len = MY_UC_BUFF_SIZE;
  gunichar buf_static[MY_UC_BUFF_SIZE];
  gunichar * buf = &buf_static[0];;
#else
  gunichar * buf = NULL;
#endif

  if(!a || !*a || !b || !*b)
    return FALSE;

  while(*h && *n)
    {
      c = g_utf8_get_char(h);

#if HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
      if(!g_unicode_canonical_decomposition_to_buffer (c,buf,buf_len,
                                                       &result_len)
         && result_len)
        {
          buf = (gunichar*)alloca(result_len*sizeof(gunichar));
          buf_len = result_len;

          if(!g_unicode_canonical_decomposition_to_buffer (c,buf,buf_len,
                                                           &result_len))
            {
              /* something badly wrong here ...*/
              return FALSE;
            }
        }
#else
      g_free(buf);
      buf = g_unicode_canonical_decomposition (c, &result_len);
#endif

      for(j = 0; j < result_len; ++j)
        {
          t = g_unichar_type(buf[j]);

          switch (t)
            {
            case G_UNICODE_COMBINING_MARK:
            case G_UNICODE_ENCLOSING_MARK:
            case G_UNICODE_NON_SPACING_MARK:
              /*strip marks -- do nothing */
              break;

            case G_UNICODE_UPPERCASE_LETTER:
            case G_UNICODE_TITLECASE_LETTER:
              buf[j] = g_unichar_tolower(buf[j]);
              /* fall through */
            default:
              /* just use c as is */
              if(buf[j] == *n)
                {
                  /* so far so good -- use next character in needle in next
                     step */
                  n++;

                  if(!*n)
                    {
                      /* end of needle -- we have a match */
#ifndef HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
                      g_free(buf);
#endif
                      if(end)
                        *end = g_utf8_next_char(h) - a;

                      return TRUE;
                    }
                }
              else
                {
                  /* bad luck */
#ifndef HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
                  g_free(buf);
#endif
                  return FALSE;
                }
              break;
            }
        }
      h = g_utf8_next_char(h);
    }

#ifndef HAVE_DECL_G_UNICODE_CANONICAL_DECOMPOSITION_TO_BUFFER
  g_free(buf);
#endif

  /* run out of haystack before the end of needle */
  return FALSE;
}

#if 0
#include <stdio.h>

int
main (int argc, char *argv[])
{
  char haystack[] = "ToTomášova kupKa seNa";
  char n1[] = "tO";
  char n2[] = "PK";
  char n3[] = "tomas";
  char * pNormalised = &haystack[0];
  const char * p;
  gunichar * pU1, * pU2, *pU3;
  int end;

  pU1 = utf8_strcasestrip(n1);
  pU2 = utf8_strcasestrip(n2);
  pU3 = utf8_strcasestrip(n3);

  printf("needle 3: 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
         pU3[0], pU3[1], pU3[2], pU3[3], pU3[4]);

  printf("haystack starts with needle (1): %s (end %d)\n",
         utf8_strstartswithcasestrip(pNormalised, pU1, &end) ? "yes" : "no",
         end);
  printf("haystack starts with needle (2): %s (end %d)\n",
         utf8_strstartswithcasestrip(pNormalised, pU2, &end) ? "yes" : "no",
         end);

  p = utf8_strstrcasestrip (pNormalised, pU1, &end);
  if(p)
    {
      printf("found needle 1 at offset %d (end %d)\n", p - pNormalised, end);
    }
  else
    {
      printf("needle 1 not found\n");
    }

  p = utf8_strstrcasestrip (pNormalised, pU2, &end);
  if(p)
    {
      printf("found needle 2 at offset %d (end %d)\n", p - pNormalised, end);
    }
  else
    {
      printf("needle 2 not found\n");
    }

  p = utf8_strstrcasestrip (pNormalised, pU3, &end);
  if(p)
    {
      printf("found needle 3 at offset %d (end %d)\n", p - pNormalised, end);
    }
  else
    {
      printf("needle 3 not found\n");
    }

  g_free(pU1);
  g_free(pU2);
  g_free(pU3);
}
#endif
