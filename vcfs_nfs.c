/****************************************************************************
 * Filename: vcfs_nfs.c
 * Our actual implementation of the small subset of the NFS protocol that we
 * currently support. The NFS client and this server communicate via
 * filehandles (vcfs_fhdata) that are opaque to the client and are unique
 * for every file in the filesystem.
 ***************************************************************************/

#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include "nfsproto.h"
#include "vcfs.h"
#include "utils.h"

typedef struct svc_req *SR;

/* Get the attributes of a virtual file */
int get_vattr(vcfs_ventry *v, fattr *f)
{
    nfstime t = {v->ctime, 0};
    
    if (v->type == NFDIR) {
        f->type = NFDIR;
        f->mode = NFSMODE_DIR | 0555;
        f->nlink = 2;
        f->uid = UID;
        f->gid = GID;
        f->size = v->size;
        f->blocksize = 512;
        f->rdev = -1;
        
        f->blocks = (f->size / 512) + 1;
        f->fsid = 101;
        f->fileid = v->id;
        f->atime = t;
        f->mtime = t;
        f->ctime = t;
    }
    else {  
        f->type = NFREG;
        f->mode = (mode_t)33060;
        f->nlink = 1;
        f->uid = UID;
        f->gid = GID;
        f->size = v->size;
        f->blocksize = 512;
        f->blocks = (v->size + 511) / 512;
        f->rdev = -1;
        f->fsid = 101;
        f->fileid = v->id;
        f->atime = t;
        f->mtime = t;
        f->ctime = t;
    }
    return 1;
}

/* Fake the root dir */
void rootgetattr(struct fattr *f)
{
    nfstime roottime = {0,0};
    
    f->type=NFDIR;
    f->mode=NFSMODE_DIR|0555;
    f->nlink = 3;
    f->uid=UID;
    f->gid=GID;
    f->size=8192;
    f->blocksize=512;
    f->rdev = -1;
    f->blocks = 1;
    f->fsid = 101;
    f->fileid = 1;
    bcopy(&roottime,&f->atime,sizeof(nfstime));
    bcopy(&roottime,&f->mtime,sizeof(nfstime));
    bcopy(&roottime,&f->ctime,sizeof(nfstime));
}


void *
nfsproc_null_2(ap,rp)
     void *ap;
     SR rp;
{
	static int ret=0;
	return ((void*) &ret);
}


diropres *rootlookup();
readdirres *rootreaddir();

attrstat *
nfsproc_getattr_2(ap,rp)
     nfs_fh *ap;
     SR rp;
{
    
    static attrstat ret;
    vcfs_fileid *f;
    
    //debug("getattr:\n");
    //dump_fh((vcfs_fhdata *)ap);
    f = get_fh((vcfs_fhdata *)ap);
   
    if (f != NULL)
    {
        if (f->ventry != NULL)
        {
            get_vattr(f->ventry, &ret.attrstat_u.attributes);
            ret.status = NFS_OK;
        }
        else
        {
            rootgetattr(&ret.attrstat_u.attributes);
            ret.status = NFS_OK;
        }
    }
    else 
    {
        ret.status = NFSERR_NOENT;
    }
    
    //printf("   result: %d\n", ret.attrstat_u.attributes.fileid);
    return &ret;
    
}

attrstat *
nfsproc_setattr_2(ap,rp)
     sattrargs *ap;
     SR rp;
{
	static attrstat ret;
    
	ret.status = NFSERR_NOENT;
	return &ret;

}

void *
nfsproc_root_2(ap,rp)
     void *ap;
     SR rp;
{
	static int ret=0;
	
	return ((void*)&ret);
}


diropres *
nfsproc_lookup_2(ap,rp)
     diropargs *ap;
     SR rp;
{
    vcfs_fileid *parent;
    vcfs_fileid *f;
    static diropres ret;
    vcfs_fhdata *hand;
    
    //printf("lookup %s in\n",ap->name);
    //dump_fh((vcfs_fhdata *)(&ap->dir));
    
    parent = get_fh((vcfs_fhdata *)(&ap->dir));
    
    if (parent == NULL)
    {
        ret.status = NFSERR_NOENT;
        return (&ret);
    }
    
    hand = (vcfs_fhdata *)&ret.diropres_u.diropres.file;
    f = (vcfs_fileid *)lookuph(parent, ap->name, hand); 
    
    if (f == NULL)
    {
        ret.status = NFSERR_NOENT;
        //printf("\tCouldn't find %s...\n", ap->name);
        return &ret;
    }
    
    /* We found the file, now get it's attributes and return it */
    assert(f->ventry != NULL);
    get_vattr(f->ventry, &ret.diropres_u.diropres.attributes);
    
    ret.status = NFS_OK;
    return &ret;
    
}

readlinkres *
nfsproc_readlink_2(ap,rp)
     nfs_fh *ap;
     SR rp;
{
    static readlinkres ret;
    ret.status = NFSERR_NOENT;
    return &ret;
}



readres *
nfsproc_read_2(readargs *ap, SR rp)
{
    static readres ret;
    static char buffer[8192];
    int len;
    vcfs_fileid *h;
    
    
    //printf("read: length = %d, offset = %d\n", 
    // ap->count, ap->offset);
    //dump_fh(&ap->file);
    
    len = vcfs_read(buffer, (vcfs_fhdata *)&ap->file, ap->count, ap->offset);
    
    if (len < 0)
    {
        ret.status = NFSERR_NOENT;
        return &ret;
    }
    
    ret.readres_u.reply.data.data_len = len;
    ret.readres_u.reply.data.data_val = buffer;

    h = (vcfs_fileid *)get_fh((vcfs_fhdata *)&ap->file);
    
    if (h != NULL && h->ventry != NULL)
    {
        get_vattr(h->ventry, &ret.readres_u.reply.attributes);
    }
    
    ret.status = NFS_OK;
    return &ret;
    
}


void *
nfsproc_writecache_2(ap,rp)
     void *ap;
     SR rp;
{
	static int ret;

	return (void *)(&ret);
}


attrstat *
nfsproc_write_2(ap,rp)
     writeargs *ap;
     SR rp;
{
	static attrstat ret;

	ret.status = NFSERR_ACCES;
	return &ret;

}


diropres *
nfsproc_create_2(ap,rp)
     createargs *ap;
     SR rp;
{
	static diropres ret;

	ret.status = NFSERR_ACCES;
	return &ret;

}


nfsstat *
nfsproc_remove_2(ap,rp)
     diropargs *ap;
     SR rp;
{
	static nfsstat ret;

	ret = NFSERR_ACCES;
	return &ret;
	
}


nfsstat *
nfsproc_rename_2(ap,rp)
     renameargs *ap;
     SR rp;
{
	static nfsstat ret;

	ret = NFSERR_ACCES;
	return &ret;

}


nfsstat *
nfsproc_link_2(ap,rp)
     linkargs *ap;
     SR rp;
{
	static nfsstat ret;

	ret = NFSERR_ACCES;
	return &ret;

}


nfsstat *
nfsproc_symlink_2(ap,rp)
     symlinkargs *ap;
     SR rp;
{
	static nfsstat ret;

	ret = NFSERR_NOENT;
	return &ret;

}

diropres *
nfsproc_mkdir_2(ap,rp)
     createargs *ap;
     SR rp;
{
	static diropres ret;

	ret.status = NFSERR_ACCES;
	return &ret;

}


nfsstat *
nfsproc_rmdir_2(ap,rp)
     diropargs *ap;
     SR rp;
{
	static nfsstat ret;

	ret = NFSERR_ACCES;
	return &ret;
	
}

/* This is found in cvsfs_fh.c */
extern vcfs_ventry *ventry_list;

void dump_entries(entry *e)
{
    entry *next;
    printf("----- Readdir Dump -----\n");
    
    for (next = e; next != NULL; next = next->nextentry)
    {
        printf("Name is %s, id is %d\n", next->name, next->fileid);
    }
    
    printf("----- Done ------\n\n");
}
    

readdirres *
nfsproc_readdir_2(ap,rp)
     readdirargs *ap;
     SR rp;
{

    static readdirres ret;
    static entry entrytab[256];	/* TODO: Change to dynamic */
    static vcfs_name names[256];
    entry **prev;
    long cookie;
    vcfs_fileid *h;
    vcfs_ventry *temp;
    vcfs_path parent;
    vcfs_name entry;

    strcpy(names[0], ".");
    strcpy(names[1], "..");
    
    h = get_fh((vcfs_fhdata *)&ap->dir);
    ret.readdirres_u.reply.eof = TRUE;

    prev = &ret.readdirres_u.reply.entries;
    *prev = NULL;

    ASSERT(h != NULL, "Trying to read from NULL filehandle");
    
    if (h->id == 0)
    {
        /* Read the root dir */
        entrytab[0].fileid = 1;
        entrytab[0].name = names[0];
        cookie = 1;
        memcpy(entrytab[0].cookie, &cookie, sizeof(long));
        *prev = &entrytab[0];
        prev = &entrytab[0].nextentry;
        entrytab[0].nextentry = NULL;
        
        entrytab[1].fileid = 1;
        cookie = 1;
        memcpy(entrytab[1].cookie, &cookie, sizeof(long));
        entrytab[1].name = names[1];
        *prev = &entrytab[1];
        prev = &entrytab[1].nextentry;
        entrytab[1].nextentry = NULL;
        
        entrytab[2].fileid = ventry_list->id;
        cookie = 2;
        memcpy(entrytab[2].cookie, &cookie, sizeof(long));
        strcpy(names[2], ventry_list->name);
        entrytab[2].name = names[2];
        *prev = &entrytab[2];
        prev = &entrytab[2].nextentry;
        entrytab[2].nextentry = NULL;
        
        ret.readdirres_u.reply.eof = TRUE;
        ret.status = NFS_OK;
        return &ret;
    }
    
    if (*ap->cookie == 0)
    {
        /* Return . (ap->dir) */
        entrytab[0].fileid = h->id;
        entrytab[0].name = names[0];
        cookie = h->id;
        memcpy(entrytab[0].cookie, &cookie, sizeof(nfscookie));
        *prev = &entrytab[0];
        prev = &entrytab[0].nextentry;
        entrytab[0].nextentry = NULL;
        
        /* Return .. */
        if (h->ventry != NULL && h->ventry == ventry_list)
        {
            /* The parent is the root */
            entrytab[1].fileid = 1;
            cookie = 1;
            memcpy(entrytab[1].cookie, &cookie, sizeof(long));
            entrytab[1].name = names[1];
            *prev = &entrytab[1];
            prev = &entrytab[1].nextentry;
            entrytab[1].nextentry = NULL;
        }
        else
        {
            /* Get the parent */
            vcfs_fileid *p;
            
            split_path(h->name, &parent, &entry);
            p = (vcfs_fileid *)lookup_fh_name(parent);
            
            assert(p != NULL);
            
            entrytab[1].fileid = p->id;
            cookie = p->id;
            memcpy(entrytab[1].cookie, &cookie, sizeof(long));
            entrytab[1].name = names[1];
            *prev = &entrytab[1];
            prev = &entrytab[1].nextentry;
            entrytab[1].nextentry = NULL;
        }
    }
    
    ret.readdirres_u.reply.eof = TRUE;
    /* Now get the rest of the files */
    if (h->ventry != NULL)
    {
        int count = 0;
        int j = 2;
        cookie = 2;
        
        if (*ap->cookie == 0)
        {
            count = 2;
        }

        for (temp = h->ventry->dirent; temp != NULL; temp = temp->next)
        {
            split_path(temp->name, &parent, &entry);
            
            /* Skip entries until we reach the cookie */
            if (*(long *)ap->cookie > 0 && (cookie - 1) < *(long *)ap->cookie)
            {
                cookie++;
                j++;
                continue;
            }

            /* Skip filenames containing a comma - ver extended name */
            if (strrchr(entry, ','))
                continue;
            
            entrytab[count].fileid = temp->id;
            cookie = j;
            memcpy(entrytab[count].cookie, &cookie, sizeof(nfscookie));
            strcpy(names[count], entry);
            entrytab[count].name = names[count];
            *prev = &entrytab[count];
            prev = &entrytab[count].nextentry;
            entrytab[count].nextentry = NULL;

            count++;
            j++;
            
            /* Only return 256 entries at a time */
            if (count == 256)
            {
                ret.readdirres_u.reply.eof = FALSE;
                break;
            }
        }
    }
    
    //dump_entries(ret.readdirres_u.reply.entries);
    ret.status = NFS_OK;
    return &ret;
}

statfsres *
nfsproc_statfs_2(ap,rp)
    nfs_fh *ap;
    SR rp;
{
	static statfsres ret;

	ret.status = NFSERR_NOENT;
	return &ret;

}

