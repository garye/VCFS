#include "utils.h"

char PATHSEP = '/';
/* Split a pathname into the parent directory and entry */ 
void split_path(vcfs_path path, vcfs_path *parent, vcfs_name *entry)
{
    char *p;
    
    memset(parent, 0, sizeof(parent));
    memset(entry, 0, sizeof(entry));
    
    /* Find the last '/' in the path */
    p = (char *)strrchr(path, PATHSEP);
    if (p == NULL) {
        /* TBD - ??? This shouldn't happen */
        return;
    }
    
    strncpy(*parent, path, (p - path));
    (*parent)[p - path] = '\0';
    
    strncpy(*entry, p + 1, (strlen(path) - (p - path) - 1));
    (*entry)[(strlen(path) - (p - path)) - 1] = '\0';
    
    return;
    
}
