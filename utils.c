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

/*
 * Debug function that will print the message (a variable list) if the
 * level of debugging is set at or higher than the parameter 'level'.
 * Depends on the VCFS_DEBUG being set.
 */
void DEBUG(int level, const char *fmt, ... ) 
{
    /* see 'man va_start' and 'man vfprintf' for dealing w/ variable
       arg lists */
    va_list argp; /* variable arg list */
    int curr_level;
    char *char_level;
    static int num = 0;
    va_start( argp, fmt); /* initialize the va list */
    
    /* Check to see if the env is set */
    if ( (char_level = getenv( VCFS_DEBUG_ENV )) != NULL) {
        
        curr_level = atoi( char_level );
        
        /* If it set at or above the level used, print the debug message */
        if (level <= curr_level) {
            fprintf(stderr, "[VCFS DEBUG] %d: ", num++);
            vfprintf(stderr, fmt, argp); /* print the variable list */
            fprintf(stderr, "\n");
            fflush(stderr);
        }
    }
    va_end( argp ); /* end the var arg list */
}
