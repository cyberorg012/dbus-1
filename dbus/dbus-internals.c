/* -*- mode: C; c-file-style: "gnu" -*- */
/* dbus-internals.c  random utility stuff (internal to D-BUS implementation)
 *
 * Copyright (C) 2002, 2003  Red Hat, Inc.
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
#include "dbus-internals.h"
#include "dbus-protocol.h"
#include "dbus-test.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

/**
 * @defgroup DBusInternals D-BUS internal implementation details
 * @brief Documentation useful when developing or debugging D-BUS itself.
 * 
 */

/**
 * @defgroup DBusInternalsUtils Utilities and portability
 * @ingroup DBusInternals
 * @brief Utility functions (_dbus_assert(), _dbus_warn(), etc.)
 * @{
 */

/**
 * @def _dbus_assert
 *
 * Aborts with an error message if the condition is false.
 * 
 * @param condition condition which must be true.
 */

/**
 * @def _dbus_assert_not_reached
 *
 * Aborts with an error message if called.
 * The given explanation will be printed.
 * 
 * @param explanation explanation of what happened if the code was reached.
 */

/**
 * @def _DBUS_N_ELEMENTS
 *
 * Computes the number of elements in a fixed-size array using
 * sizeof().
 *
 * @param array the array to count elements in.
 */

/**
 * @def _DBUS_POINTER_TO_INT
 *
 * Safely casts a void* to an integer; should only be used on void*
 * that actually contain integers, for example one created with
 * _DBUS_INT_TO_POINTER.  Only guaranteed to preserve 32 bits.
 * (i.e. it's used to store 32-bit ints in pointers, but
 * can't be used to store 64-bit pointers in ints.)
 *
 * @param pointer pointer to extract an integer from.
 */
/**
 * @def _DBUS_INT_TO_POINTER
 *
 * Safely stuffs an integer into a pointer, to be extracted later with
 * _DBUS_POINTER_TO_INT. Only guaranteed to preserve 32 bits.
 *
 * @param integer the integer to stuff into a pointer.
 */
/**
 * @def _DBUS_ZERO
 *
 * Sets all bits in an object to zero.
 *
 * @param object the object to be zeroed.
 */
/**
 * @def _DBUS_INT_MIN
 *
 * Minimum value of type "int"
 */
/**
 * @def _DBUS_INT_MAX
 *
 * Maximum value of type "int"
 */
/**
 * @def _DBUS_MAX_SUN_PATH_LENGTH
 *
 * Maximum length of the path to a UNIX domain socket,
 * sockaddr_un::sun_path member. POSIX requires that all systems
 * support at least 100 bytes here, including the nul termination.
 * We use 99 for the max value to allow for the nul.
 *
 * We could probably also do sizeof (addr.sun_path)
 * but this way we are the same on all platforms
 * which is probably a good idea.
 */

/**
 * @typedef DBusForeachFunction
 * 
 * Used to iterate over each item in a collection, such as
 * a DBusList.
 */

/**
 * @def _DBUS_LOCK_NAME
 *
 * Expands to name of a global lock variable.
 */

/**
 * @def _DBUS_DEFINE_GLOBAL_LOCK
 *
 * Defines a global lock variable with the given name.
 * The lock must be added to the list to initialize
 * in dbus_threads_init().
 */

/**
 * @def _DBUS_DECLARE_GLOBAL_LOCK
 *
 * Expands to declaration of a global lock defined
 * with _DBUS_DEFINE_GLOBAL_LOCK.
 * The lock must be added to the list to initialize
 * in dbus_threads_init().
 */

/**
 * @def _DBUS_LOCK
 *
 * Locks a global lock
 */

/**
 * @def _DBUS_UNLOCK
 *
 * Unlocks a global lock
 */

/**
 * Fixed "out of memory" error message, just to avoid
 * making up a different string every time and wasting
 * space.
 */
const char _dbus_no_memory_message[] = "Not enough memory";

/**
 * Prints a warning message to stderr.
 *
 * @param format printf-style format string.
 */
void
_dbus_warn (const char *format,
            ...)
{
  /* FIXME not portable enough? */
  va_list args;

  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
}

/**
 * Prints a warning message to stderr
 * if the user has enabled verbose mode.
 * This is the real function implementation,
 * use _dbus_verbose() macro in code.
 *
 * @param format printf-style format string.
 */
void
_dbus_verbose_real (const char *format,
                    ...)
{
  va_list args;
  static dbus_bool_t verbose = TRUE;
  static dbus_bool_t initted = FALSE;

  /* things are written a bit oddly here so that
   * in the non-verbose case we just have the one
   * conditional and return immediately.
   */
  if (!verbose)
    return;
  
  if (!initted)
    {
      verbose = _dbus_getenv ("DBUS_VERBOSE") != NULL;
      initted = TRUE;
      if (!verbose)
        return;
    }
  
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
}

/**
 * Duplicates a string. Result must be freed with
 * dbus_free(). Returns #NULL if memory allocation fails.
 * If the string to be duplicated is #NULL, returns #NULL.
 * 
 * @param str string to duplicate.
 * @returns newly-allocated copy.
 */
char*
_dbus_strdup (const char *str)
{
  int len;
  char *copy;
  
  if (str == NULL)
    return NULL;
  
  len = strlen (str);

  copy = dbus_malloc (len + 1);
  if (copy == NULL)
    return NULL;

  memcpy (copy, str, len + 1);
  
  return copy;
}

/**
 * Returns a string describing the given type.
 *
 * @param type the type to describe
 * @returns a constant string describing the type
 */
const char *
_dbus_type_to_string (int type)
{
  switch (type)
    {
    case DBUS_TYPE_INVALID:
      return "invalid";
    case DBUS_TYPE_NIL:
      return "nil";
    case DBUS_TYPE_BOOLEAN:
      return "boolean";
    case DBUS_TYPE_INT32:
      return "int32";
    case DBUS_TYPE_UINT32:
      return "uint32";
    case DBUS_TYPE_DOUBLE:
      return "double";
    case DBUS_TYPE_STRING:
      return "string";
    case DBUS_TYPE_BOOLEAN_ARRAY:
      return "boolean array";
    case DBUS_TYPE_INT32_ARRAY:
      return "int32 array";
    case DBUS_TYPE_UINT32_ARRAY:
      return "uint32 array";
    case DBUS_TYPE_DOUBLE_ARRAY:
      return "double array";
    case DBUS_TYPE_BYTE_ARRAY:
      return "byte array";
    case DBUS_TYPE_STRING_ARRAY:
      return "string array";
    default:
      return "unknown";
    }
}

static dbus_bool_t
run_failing_each_malloc (int                    n_mallocs,
                         const char            *description,
                         DBusTestMemoryFunction func,
                         void                  *data)
{
  n_mallocs += 10; /* fudge factor to ensure reallocs etc. are covered */
  
  while (n_mallocs >= 0)
    {      
      _dbus_set_fail_alloc_counter (n_mallocs);

      _dbus_verbose ("\n===\n%s: (will fail malloc %d with %d failures)\n===\n",
                     description, n_mallocs,
                     _dbus_get_fail_alloc_failures ());

      if (!(* func) (data))
        return FALSE;
      
      n_mallocs -= 1;
    }

  _dbus_set_fail_alloc_counter (_DBUS_INT_MAX);

  return TRUE;
}                        

/**
 * Tests how well the given function responds to out-of-memory
 * situations. Calls the function repeatedly, failing a different
 * call to malloc() each time. If the function ever returns #FALSE,
 * the test fails. The function should return #TRUE whenever something
 * valid (such as returning an error, or succeeding) occurs, and #FALSE
 * if it gets confused in some way.
 *
 * @param description description of the test used in verbose output
 * @param func function to call
 * @param data data to pass to function
 * @returns #TRUE if the function never returns FALSE
 */
dbus_bool_t
_dbus_test_oom_handling (const char             *description,
                         DBusTestMemoryFunction  func,
                         void                   *data)
{
  int approx_mallocs;

  /* Run once to see about how many mallocs are involved */
  
  _dbus_set_fail_alloc_counter (_DBUS_INT_MAX);

  if (!(* func) (data))
    return FALSE;
  
  approx_mallocs = _DBUS_INT_MAX - _dbus_get_fail_alloc_counter ();

  _dbus_verbose ("=================\n%s: about %d mallocs total\n=================\n",
                 description, approx_mallocs);

  _dbus_set_fail_alloc_failures (1);
  if (!run_failing_each_malloc (approx_mallocs, description, func, data))
    return FALSE;

  _dbus_set_fail_alloc_failures (2);
  if (!run_failing_each_malloc (approx_mallocs, description, func, data))
    return FALSE;
  
  _dbus_set_fail_alloc_failures (3);
  if (!run_failing_each_malloc (approx_mallocs, description, func, data))
    return FALSE;

  _dbus_set_fail_alloc_failures (4);
  if (!run_failing_each_malloc (approx_mallocs, description, func, data))
    return FALSE;
  
  _dbus_verbose ("=================\n%s: all iterations passed\n=================\n",
                 description);

  return TRUE;
}

/** @} */
