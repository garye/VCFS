/*****************************************************************************
 * File: cvstool_proc.c
 * This file contains the implementation of the cvstool commands. The cvstool
 * protocol is based on RPC, like NFS, and is defined in the file cvstool.x. 
 ****************************************************************************/

#include "cvstool.h"
#include "vcfs.h"
#include "utils.h"
#include "cvs_cmds.h"

#include <assert.h>

int cvstool_get_author_date(vcfs_ventry *v, char **date, char **author);
bool cvstool_validate_tag(vcfs_ventry *v, const char *tag, char **ver);

cvstool_ls_resp *
cvstool_ls_1(cvstool_ls_args *argp, struct svc_req *rqstp)
{
	static cvstool_ls_resp  result;
    vcfs_fileid *id;
    vcfs_ventry *v;
    vcfs_path parent;
    vcfs_name name;

    
    /* Free previous result */
    xdr_free((xdrproc_t)xdr_cvstool_ls_resp, (caddr_t)&result);

    ASSERT(argp->path != NULL, "NULL path");
    
    id = (vcfs_fileid *)lookup_fh_name(argp->path);
    
    if (id == NULL)
    {
        result.status = CVSTOOL_NOENT;
        return &result;
    }
    
    v = id->ventry;
    
    ASSERT(v != NULL, "NULL ventry");
    
    if (strchr(v->name, ','))
    {
        /* Don't do anything for revision extended files */
        result.status = CVSTOOL_NOENT;
        return &result;
    }

    if (v->type == NFREG)
    {
        /* We are listing a single file */
        split_path(argp->path, &parent, &name);
        result.dirents = (cvstool_dirent *)malloc(sizeof(cvstool_dirent));
        result.dirents->name = strdup(name);
        result.dirents->ver_info.ver = strdup(v->ver);
        
        if (strlen(v->tag) > 0)
        {
            result.dirents->ver_info.tag = strdup(v->tag);
        }
        else
        {
            result.dirents->ver_info.tag = strdup("");
        }
        
        if (argp->options & CVSTOOL_LS_LONG)
        {
            /* Get the author and date of this version */
            cvstool_get_author_date(v, 
                                    &(result.dirents->ver_info.date),
                                    &(result.dirents->ver_info.author));
        }
        else
        {
            result.dirents->ver_info.date = strdup("");
            result.dirents->ver_info.author = strdup("");
        }
        
        result.dirents->next = NULL;
        result.dirents->ver_info.next = NULL;

        result.status = CVSTOOL_OK;
        result.num_resp = 1;
        result.eof = TRUE;
    }
    else if (v->type == NFDIR)
    {
        int count = 0;
        vcfs_ventry *entry;
        cvstool_dirent *dirent, **direntp;
        
        direntp = &(result.dirents);
        
        /* List an entire directory */
        for (entry = v->dirent; entry != NULL; entry = entry->next)
        {
            vcfs_path parent;
            vcfs_name name;
            
            if (entry->type != NFREG || strchr(entry->name, ','))
                continue;
            
            split_path(entry->name, &parent, &name);

            dirent = *direntp = 
                (cvstool_dirent *)malloc(sizeof(cvstool_dirent));
            
            dirent->name = strdup(name);
            dirent->ver_info.ver = strdup(entry->ver);
            
            if (argp->options & CVSTOOL_LS_LONG)
            {
                /* Get the author and date of this version */
                cvstool_get_author_date(entry, 
                                        &(dirent->ver_info.date),
                                        &(dirent->ver_info.author));
            }
            else
            {
                dirent->ver_info.date = strdup("");
                dirent->ver_info.author = strdup("");
            }
         
            if (strlen(entry->tag) > 0)
            {
                dirent->ver_info.tag = strdup(entry->tag);
            }
            else
            {
                dirent->ver_info.tag = strdup("");
            }
            
            dirent->ver_info.next = NULL;
            
            count++;
            direntp = &dirent->next;
        }
        
        *direntp = NULL;

        result.status = CVSTOOL_OK;
        result.num_resp = count;
        result.eof = TRUE;
    }
    else
    {
        /* It's not a directory or a file?? Symlinks not supported. */
        ASSERT(1 < 0, "Unsupported filetype");
    }
    
	return &result;
}

cvstool_lsver_resp *
cvstool_lsver_1(cvstool_lsver_args *argp, struct svc_req *rqstp)
{
	static cvstool_lsver_resp  result;
    vcfs_fileid *id;
    vcfs_ventry *v;
    cvs_buff *resp;
    char *ver, *author, *date;
    int count = 0;
    cvstool_ver_info *vers, **versp;
    
    /* Free previous result */
    xdr_free((xdrproc_t)xdr_cvstool_lsver_resp, (caddr_t)&result);

    ASSERT(argp->path != NULL, "NULL path");
    
    id = (vcfs_fileid *)lookup_fh_name(argp->path);
    
    if (id == NULL)
    {
        result.status = CVSTOOL_NOENT;
        return &result;
    }
    
    v = id->ventry;

    ASSERT(v != NULL, "NULL ventry");
    
    if (v->type != NFREG)
    {
        /* We can only list versions of files */
        result.status = CVSTOOL_FTYPE_MISMATCH;
        return &result;
    }
    
    versp = &(result.vers);
    
    cvs_get_log(v->name, &resp);
    
    while(TRUE)
    {
        vers = *versp = (cvstool_ver_info *)malloc(sizeof(cvstool_ver_info));
        
        if (cvs_get_log_info(resp, &vers->ver, &vers->date, 
                             &vers->author, NULL) == 0)
        {
            /* We've read all the versions we can */
            break;
        }
        vers->tag = strdup("");
        vers->next = NULL;
        versp = &vers->next;
        
        if ((argp->options & CVSTOOL_LSVER_PRE) && count == 1)
        {
            /* We just read the predecessor, only return it */
            result.vers = vers;
            break;
        }
   
        count++;
    }
    
    *versp = NULL;
    
    cvs_free_buff(resp);

    result.num_resp = count;
    result.status = CVSTOOL_OK;

	return &result;
}

cvstool_update_resp *
cvstool_update_1(cvstool_update_args *argp, struct svc_req *rqstp)
{
    static cvstool_update_resp  result;
    vcfs_fileid *id;
    vcfs_ventry *v = NULL;
    vcfs_path parent;
    vcfs_name name;

    ASSERT(argp->path != NULL, "NULL path");
    
    if (strcmp(argp->path, ""))
    {
        /* Update a single file */
        /* TODO: Update entire directory */
        id = (vcfs_fileid *)lookup_fh_name(argp->path);
        
        if (id == NULL)
        {
            result.status = CVSTOOL_NOENT;
            return &result;
        }
        
        v = id->ventry;
        
        ASSERT(v != NULL, "NULL ventry");
        ASSERT(v->type == NFREG, "TEMP: Only update files");

        if (argp->options & CVSTOOL_UPDATE_TAG)
        {
            /* We are updating to a tag */
            char *ver;
            
            if (cvstool_validate_tag(v, argp->tag, &ver))
            {
                /* Update the version and tag of this ventry */
                strncpy(v->tag, argp->tag, sizeof(v->tag));
                strncpy(v->ver, ver, sizeof(v->ver));
                free(ver);
                result.status = CVSTOOL_OK;
            }
            else
            {
                /* Not a valid tag for this file */
                result.status = CVSTOOL_NOTAG;
            }
        }
    }
    
    return &result;
}

/* Get the author and date of a file version */
int cvstool_get_author_date(vcfs_ventry *v, char **date, char **author)
{
    int n;
    cvs_buff *resp;
    
    cvs_get_status(v->name, v->ver, &resp);
    
    n = cvs_get_log_info(resp, NULL, date, author, NULL);
    
    cvs_free_buff(resp);

    return n;
}

/* Validate that the given tag is valid for the given ventry. If so, *ver 
 * points to the version number that the tag represents.
 */
bool cvstool_validate_tag(vcfs_ventry *v, const char *tag, char **ver)
{
    cvs_buff *resp;
    char *beg;
    int n;
    char *line;
    bool found_beg = FALSE;

    ASSERT(v != NULL, "Can't validate tag of a NULL ventry");
    
    cvs_get_status_tags(v, &resp);
    
    if (resp == NULL)
    {
        return FALSE;
    }
    
    /* Look for a line beginning with 'Existing Tags', then look at the
     * following lines for the given tag.
     */
    while ((n = cvs_buff_read_line(resp, &line)) > 0)
    {
        if (!found_beg)
        {
            /* No other line in this response should have "Tags:" in it */
            beg = strstr(line, "Tags:");
            if (beg != NULL)
            {
                /* Found the line */
                found_beg = TRUE;
            }
        }
        else
        {
            /* This will work as long as the tag name is not "revision"
             * or "branch"... but who would do that??
             */
            beg = strstr(line, tag);
            if (beg != NULL)
            {
                /* Found it! Now get the related version */
                char *end;
                int len;

                beg = strchr(line, ':');
                ASSERT(beg != NULL, "Weird response from cvs status -v");
                beg += 2;
                
                end = strchr(line, ')');
                ASSERT(end != NULL && beg < end, "Weird response from cvs status -v");
                
                len = end - beg;
                
                *ver = malloc(len + 1);
                strncpy(*ver, beg, len);
                (*ver)[len] = '\0';
                free(line);
                return TRUE;
            }
        }
        
        free(line);
    }
    
    return FALSE;
}

