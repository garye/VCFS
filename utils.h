#include "vcfs.h"

void split_path(vcfs_path path, vcfs_path *parent, vcfs_name *entry);

#define ASSERT(assertion, s) if (!(assertion)) { \
  fprintf(stderr, "****************************************************************\n"); \
  fprintf(stderr, "ASSERT FAILED: Line %d in %s: %s\n", __LINE__, __FILE__, s); \
  fprintf(stderr, "Please debug or kill the process\n"); \
  fprintf(stderr, "****************************************************************\n"); \
  fflush(stderr); pause(); }\
  
