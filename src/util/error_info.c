/* ddc_error.c
 *
 * <copyright>
 * Copyright (C) 2017 Sanford Rockowitz <rockowitz@minsoft.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * </endcopyright>
 */

/** \f
 *  Struct for reporting errors.
 *
 *  #Error_Info provides a pseudo-exception framework that can be integrated
 *  with more traditional status codes.  Instead of returning a status code,
 *  a C function returns a pointer to an #Error_Info instance in the case of
 *  an error, or NULL if there is no error.  Information about the cause of an
 *  error is retained for use by higher levels in the call stack.
 */

/** \cond */

// #define _GNU_SOURCE     // for reallocarray() in stdlib.h

#include <assert.h>
#include <glib-2.0/glib.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */

#include "debug_util.h"
#include "glib_util.h"
#include "report_util.h"
#include "string_util.h"

#include "error_info.h"


// Validates a pointer to an #Error_Info, using asserts
#define VALID_DDC_ERROR_PTR(ptr) \
   assert(ptr); \
   assert(memcmp(ptr->marker, ERROR_INFO_MARKER, 4) == 0);


// Forward references
static char * default_status_code_desc(int rc);

// Constants
// allows cause list to always be null terminated, even when no causes:
static Error_Info * empty_list[] = {NULL};
static const int CAUSE_ALLOC_INCREMENT = 10;

// Globals
// status code to string functions:
static ErrInfo_Status_String errinfo_desc_func = default_status_code_desc;
static ErrInfo_Status_String errinfo_name_func =  NULL;


//
// Initialization
//

/** Initializes the module.
 *
 *   \param   name_func  function returning the name of a status code
 *   \param   desc_func  function returning a description of a status code
 */
void errinfo_init(
      ErrInfo_Status_String name_func,
      ErrInfo_Status_String desc_func)
{
   errinfo_name_func = name_func;
   errinfo_desc_func = desc_func;
}


//
// Instance destruction
//

/** Releases a #Error_Info instance, including all instances it points to.
 *
 *  \param erec pointer to #Error_Info instance,
 *              do nothing if NULL
 */
void errinfo_free(Error_Info * erec){
   bool debug = false;
   if (debug) {
      printf("(%s) Starting. erec=%p\n", __func__, erec);
      show_backtrace(2);
   }
   if (erec) {
      VALID_DDC_ERROR_PTR(erec);

      if (debug) {
         printf("(%s) Freeing exception: \n", __func__);
         errinfo_report(erec, 2);
      }

      if (erec->cause_ct > 0) {
         for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
            errinfo_free(erec->causes[ndx]);
         }
         free(erec->causes);
      }
#ifdef ALT
      if (erec->causes_alt) {
         for (int ndx = 0; ndx < erec->causes_alt->len; ndx++) {
            errinfo_free( g_ptr_array_index(erec->causes_alt, ndx) );
         }
      }
#endif

      free(erec->func);
      erec->marker[3] = 'x';
      free(erec);
   }
}


/** Releases a #Error_Info instance, including all instances it points to.
 *  Optionally reports the instance before freeing it.
 *
 *  \param  erec   pointer to #Error_Info instance,
 *                 do nothing if NULL
 *  \param  report if true, report the intsance
 *  \param  func   name of calling function
 */
void errinfo_free_with_report(Error_Info * erec, bool report, const char * func) {
   if (report) {
      rpt_vstring(0, "(%s) Freeing exception:", func);
         errinfo_report(erec, 1);
   }
   errinfo_free(erec);
}


#ifdef ALT
// signature satisfying GDestroyNotify()

static void ddc_error_free2(void * erec) {
   Error_Info* erec2 = (Error_Info *) erec;
   VALID_DDC_ERROR_PTR(erec2);
   errinfo_free(erec2);
}
#endif


//
// Instance creation
//

/** Sets the status code in a existing #Error_Info instance.
 *
 *  \param  erec   pointer to instance
 *  \param  code   status code
 */
void errinfo_set_status(Error_Info * erec, int code) {
   VALID_DDC_ERROR_PTR(erec);
   erec->status_code = code;
}


/** Adds a cause to an existing #Error_Info instance
 *
 *  \param  parent instance to which cause will be added
 *  \param  cause  instance to add
 */
void errinfo_add_cause(Error_Info * parent, Error_Info * cause) {
   VALID_DDC_ERROR_PTR(parent);
   VALID_DDC_ERROR_PTR(cause);

   // printf("(%s) parent->cause_ct = %d, parent->max_causes = %d\n",
   //         __func__, parent->cause_ct, parent->max_causes);
   if (parent->cause_ct == parent->max_causes) {
      int new_max = parent->max_causes + CAUSE_ALLOC_INCREMENT;
#ifdef ALT
      Error_Info **  new_causes = calloc(new_max+1, sizeof(Error_Info *) );
      memcpy(new_causes, parent->causes, parent->cause_ct * sizeof(Error_Info *) );
      free(parent->causes);
      parent->causes = new_causes;
#endif
      if (parent->causes == empty_list) {
         // printf("(%s) empty_list\n", __func__);
         parent->causes = calloc(new_max+1, sizeof(Error_Info *) );
      }
      else {
         // printf("(%s) realloc\n", __func__);
         // works, but requires _GNU_SOURCE feature test macro:
         // parent->causes = reallocarray(parent->causes, new_max+1, sizeof(Error_Info*) );
         void * new_causes = calloc(new_max+1, sizeof(Error_Info*) );
         memcpy(new_causes, parent->causes, parent->max_causes * sizeof(Error_Info *) );
         free(parent->causes);
         parent->causes = new_causes;
      }
      parent->max_causes = new_max;
   }
   // printf("(%s) parent->causes = %p\n", __func__, parent->causes);
   // printf("(%s) cause_ct=%d\n", __func__, parent->cause_ct);
   // printf("(%s) %p", __func__, &parent->causes[parent->cause_ct]);

   parent->causes[parent->cause_ct++] = cause;

#ifdef ALT
   if (!parent->causes_alt) {
      parent->causes_alt = g_ptr_array_new_with_free_func(ddc_error_free2);
      // parent->causes_alt = g_ptr_array_new();   // *** TRANSITIONAL ***
   }
   g_ptr_array_add(parent->causes_alt, cause);
#endif
}


/** Creates a new #Error_Info instance with the specified status code
 *  and function name.
 *
 *  \param  code  status code
 *  \param  func  name of function generating status code
 *  \return pointer to new instance
 */
Error_Info *  errinfo_new(int code, const char * func) {
   Error_Info * erec = calloc(1, sizeof(Error_Info));
   memcpy(erec->marker, ERROR_INFO_MARKER, 4);
   erec->status_code = code;
   erec->causes = empty_list;
   // printf("(%s) erec->causes = %p\n", __func__, erec->causes);
   // printf("(%s) erec->causes[0] = %p\n", __func__, erec->causes[0]);
   erec->func = strdup(func);   // strdup to avoid constness warning, must free
   return erec;
}


/** Creates a new #Error_Info instance, including a reference to another
 *  instance that is the cause of the current error.
 *
 *  \param  code   status code
 *  \param  cause  pointer to another #Error_Info that is included as a cause
 *  \param  func   name of function creating new instance
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_with_cause(
      int            code,
      Error_Info *   cause,
      const char *   func)
{
   VALID_DDC_ERROR_PTR(cause);
   Error_Info * erec = errinfo_new(code, func);
   errinfo_add_cause(erec, cause);
   return erec;
}


/** Creates a new #Error_Info instance, including a reference to another
 *  instance that is the cause of the current error.  The status code
 *  of the new instance is the same as that of the referenced instance.
 *
 *  \param  cause pointer to another #Error_Info that is included as a cause
 *  \param  func  name of function creating new instance
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_chained(
      Error_Info * cause,
      const char * func)
{
   VALID_DDC_ERROR_PTR(cause);
   Error_Info * erec = errinfo_new_with_cause(cause->status_code, cause, func);
   return erec;
}


/** Creates a new #Error_Info instance with a collection of
 *  instances specified as the causes.
 *
 *  \param  code            status code of the new instance
 *  \param  causes          array of #Error_Info instances
 *  \param  cause_ct        number of causes
 *  \param  func            name of function creating the new #Error_Info
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_with_causes(
      int             code,
      Error_Info **   causes,
      int             cause_ct,
      const char *    func)
{
   Error_Info * result = errinfo_new(code, func);
   for (int ndx = 0; ndx < cause_ct; ndx++) {
      errinfo_add_cause(result, causes[ndx]);
   }
   return result;
}


#ifdef UNUSED

// For creating a new Ddc_Error when the called functions
// return status codes not Ddc_Errors.


/** Creates a new #Error_Info instance, including references to multiple
 *  status codes from called functions that contribute to the current error.
 *  Each of the callee status codes is wrapped in a synthesized #Error_Info
 *  instance that is included as a cause.
 *
 *  \param  status_code
 *  \param  callee_status_codes    array of status codes
 *  \param  callee_status_code_ct  number of status codes in **callee_status_codes**
 *  \param  callee_func            name of function that returned **callee** status codes
 *  \param  func                   name of function generating new #Error_Info
 *  \return pointer to new instance
 */
Error_Info * errinfo_new_with_callee_status_codes(
      int    status_code,
      int *  callee_status_codes,
      int                   callee_status_code_ct,
      const char *          callee_func,
      const char *          func)
{
   Error_Info * result = errinfo_new(status_code, func);
   for (int ndx = 0; ndx < callee_status_code_ct; ndx++) {
      Error_Info * cause = errinfo_new(callee_status_codes[ndx],callee_func);
      errinfo_add_cause(result, cause);
   }
   return result;
}
#endif


//
// Reporting
//

/** Status code description function to be used if none is set
 *  by #errinfo_init()
 *
 *  \param  code  status code
 *  \return description of status code
 *
 *  The value returned is valid until the next call to this function
 *  in the current thread.
 */
static char * default_status_code_desc(int rc) {
   static GPrivate  status_code_key     = G_PRIVATE_INIT(g_free);

   const int default_status_code_buffer_size = 20;

   char * buf = get_thread_fixed_buffer(&status_code_key, default_status_code_buffer_size);
   g_snprintf(buf, default_status_code_buffer_size, "%d",rc);

   return buf;
}


/** Returns a comma separated string of the status code names in the
 *  causes of the specified #Error_Info.
 *  Multiple consecutive identical names are replaced with a
 *  single name and a parenthesized instance count.
 *
 *  \param  erec  pointer to #Error_Info instance
 *  \return comma separated string, caller is responsible for freeing
 */
char * errinfo_causes_string(Error_Info * erec) {
   // bool debug = false;
   // DBGMSF(debug, "history=%p, history->ct=%d", history, history->ct);

   GString * gs = g_string_new(NULL);

   if (erec) {
      assert(memcmp(erec->marker, ERROR_INFO_MARKER, 4) == 0);

      bool first = true;

      int ndx = 0;
      while (ndx < erec->cause_ct) {
         int this_psc = erec->causes[ndx]->status_code;
         int cur_ct = 1;

         for (int i = ndx+1; i < erec->cause_ct; i++) {
            if (erec->causes[i]->status_code != this_psc)
               break;
            cur_ct++;
         }

         if (first)
            first = false;
         else
            g_string_append(gs, ", ");
         if (errinfo_name_func)
            g_string_append(gs, errinfo_name_func(this_psc));
         else {
            char buf[20];
            snprintf(buf, 20, "%d",this_psc);
            buf[19] = '\0';
            g_string_append(gs, buf);
         }
         if (cur_ct > 1)
            g_string_append_printf(gs, "(%d)", cur_ct);
         ndx += cur_ct;
      }
   }

   char * result = gs->str;
   g_string_free(gs, false);

   // DBGMSF(debug, "Done.  Returning: |%s|", result);
   return result;
}


#ifdef ALT
 char * errinfo_causes_string_alt(Error_Info * erec) {
    // bool debug = false;
    // DBGMSF(debug, "history=%p, history->ct=%d", history, history->ct);

    GString * gs = g_string_new(NULL);

    if (erec) {
       assert(memcmp(erec->marker, ERROR_INFO_MARKER, 4) == 0);

       if (erec->causes_alt) {
          bool first = true;

          int ndx = 0;
          int cause_ct = erec->causes_alt->len;
          while (ndx < cause_ct) {
             Error_Info * this_cause = g_ptr_array_index( erec->causes_alt, ndx);
             int this_psc = this_cause->status_code;
             int cur_ct = 1;

             for (int i = ndx+1; i < cause_ct; i++) {
                Error_Info * next_cause = g_ptr_array_index( erec->causes_alt, i);
                if (next_cause->status_code != this_psc)
                   break;
                cur_ct++;
             }

             if (first)
                first = false;
             else
                g_string_append(gs, ", ");
             if (errinfo_name_func)
                g_string_append(gs, errinfo_name_func(this_psc));
             else {
                char buf[20];
                snprintf(buf, 20, "%d",this_psc);
                buf[19] = '\0';
                g_string_append(gs, buf);
             }
             if (cur_ct > 1)
                g_string_append_printf(gs, "(x%d)", cur_ct);
             ndx += cur_ct;
          }
       }
    }

   char * result = gs->str;
   g_string_free(gs, false);

   // DBGMSF(debug, "Done.  Returning: |%s|", result);
   return result;
}
#endif


/** Emits a full report of the contents of the specified #Error_Info,
 *  using report functions.
 *
 *  \param  erec   pointer to #Error_Info
 *  \param  depth  logical indentation depth
 */
void errinfo_report(Error_Info * erec, int depth) {
   int d1 = depth+1;

   // rpt_vstring(depth, "Status code: %s", psc_desc(erec->psc));
   // rpt_vstring(depth, "Location: %s", (erec->func) ? erec->func : "not set");
   rpt_vstring(depth, "Exception in function %s: status=%s",
         (erec->func) ? erec->func : "not set", errinfo_desc_func(erec->status_code) );

   if (erec->cause_ct > 0) {
      rpt_vstring(depth, "Caused by: ");
      for (int ndx = 0; ndx < erec->cause_ct; ndx++) {
         errinfo_report(erec->causes[ndx], d1);
      }
   }

#ifdef ALT
   if (erec->causes_alt && erec->causes_alt->len > 0) {
      rpt_vstring(depth, "Caused by: ");
      for (int ndx = 0; ndx < erec->causes_alt->len; ndx++) {
         errinfo_report( g_ptr_array_index(erec->causes_alt,ndx), d1);
      }
   }
#endif
}


/** Returns a string summary of the specified #Error_Info.
 *  The returned value is valid until the next call to this function in the
 *  current thread, and should not be freed by the caller.
 *
 *  \param erec  pointer to #Error_Info instance
 *  \return string summmay of error
 */
char * errinfo_summary(Error_Info * erec) {
   if (!erec)
      return "NULL";
   VALID_DDC_ERROR_PTR(erec);

   static GPrivate  esumm_key     = G_PRIVATE_INIT(g_free);
   static GPrivate  esumm_len_key = G_PRIVATE_INIT(g_free);

   char * desc = errinfo_desc_func(erec->status_code);  // thread safe buffer owned by psc_desc(), do not free()

   gchar * buf1 = NULL;
   if (erec->cause_ct == 0) {
#ifdef ALT
   if (erec->causes_alt || erec->causes_alt->len == 0) {
#endif
      buf1 = gaux_asprintf("Ddc_Error[%s in %s]", desc, erec->func);
   }
   else {
      char * causes   = errinfo_causes_string(erec);
      buf1 = gaux_asprintf("Ddc_Error[%s in %s, causes: %s]", desc, erec->func, causes);
      free(causes);
   }
   int required_size = strlen(buf1) + 1;

   char * buf = get_thread_dynamic_buffer(&esumm_key, &esumm_len_key, required_size);
   g_strlcpy(buf, buf1, required_size);
   free(buf1);
   return buf;
}
