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
    
    assert(argp->path != NULL);

    id = (vcfs_fileid *)lookup_fh_name(argp->path);
    
    if (id == NULL)
    {
        result.status = CVSTOOL_NOENT;
        return &result;
    }
    
    v = id->ventry;

    assert(v != NULL);
 
    if (v->type == NFREG)
    {
        /* We are listing a single file */
        split_path(argp->path, &parent, &name);
        result.dirents = (cvstool_dirent *)malloc(sizeof(cvstool_dirent));
        result.dirents->name = strdup(name);
        result.dirents->ver_info.ver = strdup(v->ver);
        
        if (argp->option & CVSTOOL_LS_LONG)
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
            
            if (entry->type != NFREG)
                continue;
            
            split_path(entry->name, &parent, &name);

            dirent = *direntp = 
                (cvstool_dirent *)malloc(sizeof(cvstool_dirent));
            
            dirent->name = strdup(name);
            dirent->ver_info.ver = strdup(entry->ver);
            
            if (argp->option & CVSTOOL_LS_LONG)
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
            
            dirent->ver_info.next = NULL;
            
            count++;
            direntp = &dirent->next;
        }
        
        *direntp = NULL;

        result.status = CVSTOOL_OK;
        result.num_resp = count;

    }
    else
    {
        /* It's not a directory or a file?? Symlinks not supported. */
        assert(1 < 0);
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
    
    id = (vcfs_fileid *)lookup_fh_name(argp->path);
    
    if (id == NULL)
    {
        result.status = CVSTOOL_NOENT;
        return &result;
    }
    
    v = id->ventry;
    
    assert(v != NULL);
    
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

        vers->next = NULL;
        versp = &vers->next;
        
        if ((argp->option & CVSTOOL_LSVER_PRE) && count == 1)
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

int cvstool_get_author_date(vcfs_ventry *v, char **date, char **author)
{
    int n;
    cvs_buff *resp;
    
    cvs_get_status(v->name, v->ver, &resp);
    
    n = cvs_get_log_info(resp, NULL, date, author, NULL);
    
    cvs_free_buff(resp);

    return n;
}


