/*  file_util.c
 *
 *  Created on: Dec 6, 2015
 *      Author: rock
 */

#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util/file_util.h"

int file_getlines(const char * fn,  GPtrArray* line_array) {
   bool debug = false;
   int rc = 0;
   if (debug)
      printf("(%s) Starting. fn=%s  \n", __func__, fn );
   FILE * fp = fopen(fn, "r");
   if (!fp) {
      int errsv = errno;
      rc = -errno;
      fprintf(stderr, "Error opening file %s: %s\n", fn, strerror(errsv));
   }
   else {
      char * line = NULL;
      size_t len = 0;
      ssize_t read;
      // int     ct;
      // char    s0[32], s1[257], s2[16];
      // char *  head;
      // char *  rest;
      int     linectr = 0;

      while ((read = getline(&line, &len, fp)) != -1) {
         linectr++;
         char * s = strdup(line);
         g_ptr_array_add(line_array, s);
         // printf("Retrieved line of length %zu :\n", read);
         // printf("%s", line);
      }
      rc = linectr;
   }
   if (debug)
      printf("(%s) Done. returning: %d\n", __func__, rc);
   return rc;
}

