/* -*- mode: C; c-file-style: "gnu" -*- */
/* config-parser.c  XML-library-agnostic configuration file parser
 *
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * Licensed under the Academic Free License version 1.2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "config-parser.h"
#include "test.h"
#include <dbus/dbus-list.h>
#include <dbus/dbus-internals.h>
#include <string.h>

typedef enum
{
  ELEMENT_NONE,
  ELEMENT_BUSCONFIG,
  ELEMENT_INCLUDE,
  ELEMENT_USER,
  ELEMENT_LISTEN,
  ELEMENT_AUTH,
  ELEMENT_POLICY,
  ELEMENT_LIMIT,
  ELEMENT_ALLOW,
  ELEMENT_DENY
} ElementType;

typedef struct
{
  ElementType type;

  unsigned int had_content : 1;

  union
  {
    struct
    {
      unsigned int ignore_missing : 1;
    } include;

    struct
    {
      char *mechanism;
    } auth;

    struct
    {
      char *context;
      char *user;
      char *group;
      DBusList *rules;
    } policy;

    struct
    {
      int foo;
    } limit;

  } d;

} Element;

struct BusConfigParser
{
  int refcount;

  DBusList *stack;     /**< stack of Element */

  char *user;          /**< user to run as */

  DBusList *listen_on; /**< List of addresses to listen to */
};

static const char*
element_type_to_name (ElementType type)
{
  switch (type)
    {
    case ELEMENT_NONE:
      return NULL;
    case ELEMENT_BUSCONFIG:
      return "busconfig";
    case ELEMENT_INCLUDE:
      return "include";
    case ELEMENT_USER:
      return "user";
    case ELEMENT_LISTEN:
      return "listen";
    case ELEMENT_AUTH:
      return "auth";
    case ELEMENT_POLICY:
      return "policy";
    case ELEMENT_LIMIT:
      return "limit";
    case ELEMENT_ALLOW:
      return "allow";
    case ELEMENT_DENY:
      return "deny";
    }

  _dbus_assert_not_reached ("bad element type");

  return NULL;
}

static Element*
push_element (BusConfigParser *parser,
              ElementType      type)
{
  Element *e;

  _dbus_assert (type != ELEMENT_NONE);
  
  e = dbus_new0 (Element, 1);
  if (e == NULL)
    return NULL;

  if (!_dbus_list_append (&parser->stack, e))
    {
      dbus_free (e);
      return NULL;
    }
  
  e->type = type;

  return e;
}

static void
element_free (Element *e)
{

  dbus_free (e);
}

static void
pop_element (BusConfigParser *parser)
{
  Element *e;

  e = _dbus_list_pop_last (&parser->stack);
  
  element_free (e);
}

static Element*
peek_element (BusConfigParser *parser)
{
  Element *e;

  e = _dbus_list_get_last (&parser->stack);

  return e;
}

static ElementType
top_element_type (BusConfigParser *parser)
{
  Element *e;

  e = _dbus_list_get_last (&parser->stack);

  if (e)
    return e->type;
  else
    return ELEMENT_NONE;
}

static dbus_bool_t
merge_included (BusConfigParser *parser,
                BusConfigParser *included,
                DBusError       *error)
{
  DBusList *link;

  if (included->user != NULL)
    {
      dbus_free (parser->user);
      parser->user = included->user;
      included->user = NULL;
    }

  while ((link = _dbus_list_pop_first_link (&included->listen_on)))
    _dbus_list_append_link (&parser->listen_on, link);

  return TRUE;
}

BusConfigParser*
bus_config_parser_new (void)
{
  BusConfigParser *parser;

  parser = dbus_new0 (BusConfigParser, 1);
  if (parser == NULL)
    return NULL;

  parser->refcount = 1;

  return parser;
}

void
bus_config_parser_ref (BusConfigParser *parser)
{
  _dbus_assert (parser->refcount > 0);

  parser->refcount += 1;
}

void
bus_config_parser_unref (BusConfigParser *parser)
{
  _dbus_assert (parser->refcount > 0);

  parser->refcount -= 1;

  if (parser->refcount == 0)
    {
      while (parser->stack != NULL)
        pop_element (parser);

      dbus_free (parser->user);

      _dbus_list_foreach (&parser->listen_on,
                          (DBusForeachFunction) dbus_free,
                          NULL);

      _dbus_list_clear (&parser->listen_on);

      dbus_free (parser);
    }
}

dbus_bool_t
bus_config_parser_check_doctype (BusConfigParser   *parser,
                                 const char        *doctype,
                                 DBusError         *error)
{
  _DBUS_ASSERT_ERROR_IS_CLEAR (error);

  if (strcmp (doctype, "busconfig") != 0)
    {
      dbus_set_error (error,
                      DBUS_ERROR_FAILED,
                      "Configuration file has the wrong document type %s",
                      doctype);
      return FALSE;
    }
  else
    return TRUE;
}

typedef struct
{
  const char  *name;
  const char **retloc;
} LocateAttr;

static dbus_bool_t
locate_attributes (BusConfigParser  *parser,
                   const char       *element_name,
                   const char      **attribute_names,
                   const char      **attribute_values,
                   DBusError        *error,
                   const char       *first_attribute_name,
                   const char      **first_attribute_retloc,
                   ...)
{
  va_list args;
  const char *name;
  const char **retloc;
  int n_attrs;
#define MAX_ATTRS 24
  LocateAttr attrs[MAX_ATTRS];
  dbus_bool_t retval;
  int i;

  _dbus_assert (first_attribute_name != NULL);
  _dbus_assert (first_attribute_retloc != NULL);

  retval = TRUE;

  n_attrs = 1;
  attrs[0].name = first_attribute_name;
  attrs[0].retloc = first_attribute_retloc;
  *first_attribute_retloc = NULL;

  va_start (args, first_attribute_retloc);

  name = va_arg (args, const char*);
  retloc = va_arg (args, const char**);

  while (name != NULL)
    {
      _dbus_assert (retloc != NULL);
      _dbus_assert (n_attrs < MAX_ATTRS);

      attrs[n_attrs].name = name;
      attrs[n_attrs].retloc = retloc;
      n_attrs += 1;
      *retloc = NULL;

      name = va_arg (args, const char*);
      retloc = va_arg (args, const char**);
    }

  va_end (args);

  if (!retval)
    return retval;

  i = 0;
  while (attribute_names[i])
    {
      int j;
      dbus_bool_t found;
      
      found = FALSE;
      j = 0;
      while (j < n_attrs)
        {
          if (strcmp (attrs[j].name, attribute_names[i]) == 0)
            {
              retloc = attrs[j].retloc;

              if (*retloc != NULL)
                {
                  dbus_set_error (error, DBUS_ERROR_FAILED,
                                  "Attribute \"%s\" repeated twice on the same <%s> element",
                                  attrs[j].name, element_name);
                  retval = FALSE;
                  goto out;
                }

              *retloc = attribute_values[i];
              found = TRUE;
            }

          ++j;
        }

      if (!found)
        {
          dbus_set_error (error, DBUS_ERROR_FAILED,
                          "Attribute \"%s\" is invalid on <%s> element in this context",
                          attribute_names[i], element_name);
          retval = FALSE;
          goto out;
        }

      ++i;
    }

 out:
  return retval;
}

static dbus_bool_t
check_no_attributes (BusConfigParser  *parser,
                     const char       *element_name,
                     const char      **attribute_names,
                     const char      **attribute_values,
                     DBusError        *error)
{
  if (attribute_names[0] != NULL)
    {
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "Attribute \"%s\" is invalid on <%s> element in this context",
                      attribute_names[0], element_name);
      return FALSE;
    }

  return TRUE;
}

static dbus_bool_t
start_busconfig_child (BusConfigParser   *parser,
                       const char        *element_name,
                       const char       **attribute_names,
                       const char       **attribute_values,
                       DBusError         *error)
{
  if (strcmp (element_name, "user") == 0)
    {
      if (!check_no_attributes (parser, "user", attribute_names, attribute_values, error))
        return FALSE;

      if (push_element (parser, ELEMENT_USER) == NULL)
        {
          dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
          return FALSE;
        }

      return TRUE;
    }
  else if (strcmp (element_name, "listen") == 0)
    {
      if (!check_no_attributes (parser, "listen", attribute_names, attribute_values, error))
        return FALSE;

      if (push_element (parser, ELEMENT_LISTEN) == NULL)
        {
          dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
          return FALSE;
        }

      return TRUE;
    }
  else if (strcmp (element_name, "include") == 0)
    {
      Element *e;
      const char *ignore_missing;

      if ((e = push_element (parser, ELEMENT_INCLUDE)) == NULL)
        {
          dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
          return FALSE;
        }

      e->d.include.ignore_missing = FALSE;

      if (!locate_attributes (parser, "include",
                              attribute_names,
                              attribute_values,
                              error,
                              "ignore_missing", &ignore_missing,
                              NULL))
        return FALSE;

      if (ignore_missing != NULL)
        {
          if (strcmp (ignore_missing, "yes") == 0)
            e->d.include.ignore_missing = TRUE;
          else if (strcmp (ignore_missing, "no") == 0)
            e->d.include.ignore_missing = FALSE;
          else
            {
              dbus_set_error (error, DBUS_ERROR_FAILED,
                              "ignore_missing attribute must have value \"yes\" or \"no\"");
              return FALSE;
            }
        }

      return TRUE;
    }
  else if (strcmp (element_name, "policy") == 0)
    {
      Element *e;
      const char *context;
      const char *user;
      const char *group;

      if ((e = push_element (parser, ELEMENT_POLICY)) == NULL)
        {
          dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
          return FALSE;
        }

      if (!locate_attributes (parser, "include",
                              attribute_names,
                              attribute_values,
                              error,
                              "context", &context,
                              "user", &user,
                              "group", &group,
                              NULL))
        return FALSE;

      /* FIXME */
      
      return TRUE;
    }
  else
    {
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "Element <%s> not allowed inside <%s> in configuration file",
                      element_name, "busconfig");
      return FALSE;
    }
}

static dbus_bool_t
start_policy_child (BusConfigParser   *parser,
                    const char        *element_name,
                    const char       **attribute_names,
                    const char       **attribute_values,
                    DBusError         *error)
{
  if (strcmp (element_name, "allow") == 0)
    {
      if (push_element (parser, ELEMENT_ALLOW) == NULL)
        {
          dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
          return FALSE;
        }
      
      return TRUE;
    }
  else if (strcmp (element_name, "deny") == 0)
    {
      if (push_element (parser, ELEMENT_DENY) == NULL)
        {
          dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
          return FALSE;
        }
      
      return TRUE;
    }
  else
    {
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "Element <%s> not allowed inside <%s> in configuration file",
                      element_name, "policy");
      return FALSE;
    }
}

dbus_bool_t
bus_config_parser_start_element (BusConfigParser   *parser,
                                 const char        *element_name,
                                 const char       **attribute_names,
                                 const char       **attribute_values,
                                 DBusError         *error)
{
  ElementType t;

  _DBUS_ASSERT_ERROR_IS_CLEAR (error);

  /* printf ("START: %s\n", element_name); */
  
  t = top_element_type (parser);

  if (t == ELEMENT_NONE)
    {
      if (strcmp (element_name, "busconfig") == 0)
        {
          if (!check_no_attributes (parser, "busconfig", attribute_names, attribute_values, error))
            return FALSE;
          
          if (push_element (parser, ELEMENT_BUSCONFIG) == NULL)
            {
              dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
              return FALSE;
            }

          return TRUE;
        }
      else
        {
          dbus_set_error (error, DBUS_ERROR_FAILED,
                          "Unknown element <%s> at root of configuration file",
                          element_name);
          return FALSE;
        }
    }
  else if (t == ELEMENT_BUSCONFIG)
    {
      return start_busconfig_child (parser, element_name,
                                    attribute_names, attribute_values,
                                    error);
    }
  else if (t == ELEMENT_POLICY)
    {
      return start_policy_child (parser, element_name,
                                 attribute_names, attribute_values,
                                 error);
    }
  else
    {
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "Element <%s> is not allowed in this context",
                      element_name);
      return FALSE;
    }  
}

dbus_bool_t
bus_config_parser_end_element (BusConfigParser   *parser,
                               const char        *element_name,
                               DBusError         *error)
{
  ElementType t;
  const char *n;
  Element *e;

  _DBUS_ASSERT_ERROR_IS_CLEAR (error);

  /* printf ("END: %s\n", element_name); */
  
  t = top_element_type (parser);

  if (t == ELEMENT_NONE)
    {
      /* should probably be an assertion failure but
       * being paranoid about XML parsers
       */
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "XML parser ended element with no element on the stack");
      return FALSE;
    }

  n = element_type_to_name (t);
  _dbus_assert (n != NULL);
  if (strcmp (n, element_name) != 0)
    {
      /* should probably be an assertion failure but
       * being paranoid about XML parsers
       */
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "XML element ended which was not the topmost element on the stack");
      return FALSE;
    }

  e = peek_element (parser);
  _dbus_assert (e != NULL);

  switch (e->type)
    {
    case ELEMENT_NONE:
      _dbus_assert_not_reached ("element in stack has no type");
      break;

    case ELEMENT_INCLUDE:
    case ELEMENT_USER:
    case ELEMENT_LISTEN:
    case ELEMENT_AUTH:
      if (!e->had_content)
        {
          dbus_set_error (error, DBUS_ERROR_FAILED,
                          "XML element <%s> was expected to have content inside it",
                          element_type_to_name (e->type));
          return FALSE;
        }
      break;

    case ELEMENT_BUSCONFIG:
    case ELEMENT_POLICY:
    case ELEMENT_LIMIT:
    case ELEMENT_ALLOW:
    case ELEMENT_DENY:
      break;
    }

  pop_element (parser);

  return TRUE;
}

static dbus_bool_t
all_whitespace (const DBusString *str)
{
  int i;

  _dbus_string_skip_white (str, 0, &i);

  return i == _dbus_string_get_length (str);
}

dbus_bool_t
bus_config_parser_content (BusConfigParser   *parser,
                           const DBusString  *content,
                           DBusError         *error)
{
  Element *e;

  _DBUS_ASSERT_ERROR_IS_CLEAR (error);

#if 0
  {
    const char *c_str;
    
    _dbus_string_get_const_data (content, &c_str);

    printf ("CONTENT %d bytes: %s\n", _dbus_string_get_length (content), c_str);
  }
#endif
  
  e = peek_element (parser);
  if (e == NULL)
    {
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "Text content outside of any XML element in configuration file");
      return FALSE;
    }
  else if (e->had_content)
    {
      _dbus_assert_not_reached ("Element had multiple content blocks");
      return FALSE;
    }

  switch (top_element_type (parser))
    {
    case ELEMENT_NONE:
      _dbus_assert_not_reached ("element at top of stack has no type");
      return FALSE;

    case ELEMENT_BUSCONFIG:
    case ELEMENT_POLICY:
    case ELEMENT_LIMIT:
    case ELEMENT_ALLOW:
    case ELEMENT_DENY:
      if (all_whitespace (content))
        return TRUE;
      else
        {
          dbus_set_error (error, DBUS_ERROR_FAILED,
                          "No text content expected inside XML element %s in configuration file",
                          element_type_to_name (top_element_type (parser)));
          return FALSE;
        }

    case ELEMENT_INCLUDE:
      {
        /* FIXME good test case for this would load each config file in the
         * test suite both alone, and as an include, and check
         * that the result is the same
         */
        BusConfigParser *included;
        DBusError tmp_error;

        e->had_content = TRUE;
        
        dbus_error_init (&tmp_error);
        included = bus_config_load (content, &tmp_error);
        if (included == NULL)
          {
            _DBUS_ASSERT_ERROR_IS_SET (&tmp_error);
            if (dbus_error_has_name (&tmp_error, DBUS_ERROR_FILE_NOT_FOUND) &&
                e->d.include.ignore_missing)
              {
                dbus_error_free (&tmp_error);
                return TRUE;
              }
            else
              {
                dbus_move_error (&tmp_error, error);
                return FALSE;
              }
          }
        else
          {
            _DBUS_ASSERT_ERROR_IS_CLEAR (&tmp_error);

            if (!merge_included (parser, included, error))
              return FALSE;

            bus_config_parser_unref (included);
            return TRUE;
          }
      }
      break;

    case ELEMENT_USER:
      {
        char *s;

        e->had_content = TRUE;
        
        if (!_dbus_string_copy_data (content, &s))
          goto nomem;
          
        dbus_free (parser->user);
        parser->user = s;
      }
      break;

    case ELEMENT_LISTEN:
      {
        char *s;

        e->had_content = TRUE;
        
        if (!_dbus_string_copy_data (content, &s))
          goto nomem;

        if (!_dbus_list_append (&parser->listen_on,
                                s))
          {
            dbus_free (s);
            goto nomem;
          }
      }
      break;

    case ELEMENT_AUTH:
      {
        e->had_content = TRUE;
        /* FIXME */
      }
      break;
    }

  _DBUS_ASSERT_ERROR_IS_CLEAR (error);
  return TRUE;

 nomem:
  dbus_set_error (error, DBUS_ERROR_NO_MEMORY, NULL);
  return FALSE;
}

dbus_bool_t
bus_config_parser_finished (BusConfigParser   *parser,
                            DBusError         *error)
{
  _DBUS_ASSERT_ERROR_IS_CLEAR (error);

  if (parser->stack != NULL)
    {
      dbus_set_error (error, DBUS_ERROR_FAILED,
                      "Element <%s> was not closed in configuration file",
                      element_type_to_name (top_element_type (parser)));

      return FALSE;
    }
  
  return TRUE;
}

const char*
bus_config_parser_get_user (BusConfigParser *parser)
{
  return parser->user;
}

#ifdef DBUS_BUILD_TESTS
#include <stdio.h>

typedef enum
{
  VALID,
  INVALID,
  UNKNOWN
} Validity;

static dbus_bool_t
do_load (const DBusString *full_path,
         Validity          validity,
         dbus_bool_t       oom_possible)
{
  BusConfigParser *parser;
  DBusError error;

  dbus_error_init (&error);

  parser = bus_config_load (full_path, &error);
  if (parser == NULL)
    {
      _DBUS_ASSERT_ERROR_IS_SET (&error);

      if (oom_possible &&
          dbus_error_has_name (&error, DBUS_ERROR_NO_MEMORY))
        {
          _dbus_verbose ("Failed to load valid file due to OOM\n");
          dbus_error_free (&error);
          return TRUE;
        }
      else if (validity == VALID)
        {
          _dbus_warn ("Failed to load valid file but still had memory: %s\n",
                      error.message);

          dbus_error_free (&error);
          return FALSE;
        }
      else
        {
          dbus_error_free (&error);
          return TRUE;
        }
    }
  else
    {
      _DBUS_ASSERT_ERROR_IS_CLEAR (&error);

      bus_config_parser_unref (parser);

      if (validity == INVALID)
        {
          _dbus_warn ("Accepted invalid file\n");
          return FALSE;
        }

      return TRUE;
    }
}

typedef struct
{
  const DBusString *full_path;
  Validity          validity;
} LoaderOomData;

static dbus_bool_t
check_loader_oom_func (void *data)
{
  LoaderOomData *d = data;

  return do_load (d->full_path, d->validity, TRUE);
}

static dbus_bool_t
process_test_subdir (const DBusString *test_base_dir,
                     const char       *subdir,
                     Validity          validity)
{
  DBusString test_directory;
  DBusString filename;
  DBusDirIter *dir;
  dbus_bool_t retval;
  DBusError error;

  retval = FALSE;
  dir = NULL;

  if (!_dbus_string_init (&test_directory, _DBUS_INT_MAX))
    _dbus_assert_not_reached ("didn't allocate test_directory\n");

  _dbus_string_init_const (&filename, subdir);

  if (!_dbus_string_copy (test_base_dir, 0,
                          &test_directory, 0))
    _dbus_assert_not_reached ("couldn't copy test_base_dir to test_directory");

  if (!_dbus_concat_dir_and_file (&test_directory, &filename))
    _dbus_assert_not_reached ("couldn't allocate full path");

  _dbus_string_free (&filename);
  if (!_dbus_string_init (&filename, _DBUS_INT_MAX))
    _dbus_assert_not_reached ("didn't allocate filename string\n");

  dbus_error_init (&error);
  dir = _dbus_directory_open (&test_directory, &error);
  if (dir == NULL)
    {
      const char *s;
      _dbus_string_get_const_data (&test_directory, &s);
      _dbus_warn ("Could not open %s: %s\n", s,
                  error.message);
      dbus_error_free (&error);
      goto failed;
    }

  printf ("Testing:\n");

 next:
  while (_dbus_directory_get_next_file (dir, &filename, &error))
    {
      DBusString full_path;
      LoaderOomData d;

      if (!_dbus_string_init (&full_path, _DBUS_INT_MAX))
        _dbus_assert_not_reached ("couldn't init string");

      if (!_dbus_string_copy (&test_directory, 0, &full_path, 0))
        _dbus_assert_not_reached ("couldn't copy dir to full_path");

      if (!_dbus_concat_dir_and_file (&full_path, &filename))
        _dbus_assert_not_reached ("couldn't concat file to dir");

      if (!_dbus_string_ends_with_c_str (&full_path, ".conf"))
        {
          const char *filename_c;
          _dbus_string_get_const_data (&filename, &filename_c);
          _dbus_verbose ("Skipping non-.conf file %s\n",
                         filename_c);
	  _dbus_string_free (&full_path);
          goto next;
        }

      {
        const char *s;
        _dbus_string_get_const_data (&filename, &s);
        printf ("    %s\n", s);
      }

      _dbus_verbose (" expecting %s\n",
                     validity == VALID ? "valid" :
                     (validity == INVALID ? "invalid" :
                      (validity == UNKNOWN ? "unknown" : "???")));

      d.full_path = &full_path;
      d.validity = validity;
      if (!_dbus_test_oom_handling ("config-loader", check_loader_oom_func, &d))
        _dbus_assert_not_reached ("test failed");

      _dbus_string_free (&full_path);
    }

  if (dbus_error_is_set (&error))
    {
      const char *s;
      _dbus_string_get_const_data (&test_directory, &s);
      _dbus_warn ("Could not get next file in %s: %s\n",
                  s, error.message);
      dbus_error_free (&error);
      goto failed;
    }

  retval = TRUE;

 failed:

  if (dir)
    _dbus_directory_close (dir);
  _dbus_string_free (&test_directory);
  _dbus_string_free (&filename);

  return retval;
}

dbus_bool_t
bus_config_parser_test (const DBusString *test_data_dir)
{
  if (test_data_dir == NULL ||
      _dbus_string_get_length (test_data_dir) == 0)
    {
      printf ("No test data\n");
      return TRUE;
    }

  if (!process_test_subdir (test_data_dir, "valid-config-files", VALID))
    return FALSE;

  return TRUE;
}

#endif /* DBUS_BUILD_TESTS */

