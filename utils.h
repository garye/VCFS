#ifndef _UTILS_H_
#define _UTILS_H_ 1

#include <stdarg.h>
#include "vcfs.h"


/* ENV to use for debugging settings */
#define VCFS_DEBUG_ENV "VCFS_DEBUG"

/* Variable levels of debugging output. The higher the level (number),
 * the more verbose
 */
#define DEBUG_H 3
#define DEBUG_M 2
#define DEBUG_L 1
#define DEBUG_OFF 0

void split_path(vcfs_path path, vcfs_path *parent, vcfs_name *entry);

/*
 * Function to print out debugging info depending on the level set by
 * the user.
 */
/*inline void DEBUG(int level, const char* fmt, ...);*/

#define ASSERT(assertion, s) if (!(assertion)) { \
  fprintf(stderr, "****************************************************************\n"); \
  fprintf(stderr, "ASSERT FAILED: Line %d in %s: %s\n", __LINE__, __FILE__, s); \
  fprintf(stderr, "Please debug or kill the process\n"); \
  fprintf(stderr, "****************************************************************\n"); \
  fflush(stderr); pause(); }\
  
/*
 * Debug function that will print the message (a variable list) if the
 * level of debugging is set at or higher than the parameter 'level'.
 */
void DEBUG(int level, const char *fmt, ... );


#endif
