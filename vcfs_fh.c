/****************************************************************************
 * File: vcfs_fh.c
 * This file contains functions for manipulating the in-memory filesystem
 * structures. We keep a hash table of all pathnames in the filesystem, 
 * represented by vcfs_fileid's. Each vcfs_fileid contains a pointer to a
 * vcfs_ventry, which contains file attributes for regular files and
 * a directory listing for directories.
 ***************************************************************************/

#include <dirent.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <rpc/rpc.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "vcfs.h"
#include "cvs_cmds.h"
#include "utils.h"

/* Our hash table of file ids */
vcfs_fileid *file_hash[1024];

/* Our virtual cvs project */
vcfs_ventry *ventry_list;

/* This is the fileid for the root */
vcfs_fileid root_node = {0, 0, {'\0'}, 0, NULL, NULL};
static fh_ut root_handle; 

/* Stuff for the vinode bitmap */
#define NUM_VINODES 256
int vinode_bmap[NUM_VINODES];

/* We cache the most recently read file */
/* TODO: Make this a lot better */
static vcfs_read_cachent *read_cache;

/* Debug a filehandle */
void dump_fh(vcfs_fhdata *f)
{
    
    assert(f != NULL);
	
    printf("\t*** fh dump ***\n");
    printf("\tmagic - 0x%x\n", f->magic);
    printf("\tid - %d\n", f->id);
    printf("\tkey = %d\n", f->key);
    printf("\n");
}

/* Set the first few bits of the bitmap to reserve the first few numbers */
void init_vinode_bmap()
{
    /* Skip the first couple inode numbers */
    vinode_bmap[0] = 7;
}


/* Get the next unique vinode number */
int alloc_vinode()
{
    int i,j;
    int temp;
    int num = 0;
    int new;
    
    for (i = 0; i < NUM_VINODES; i++)
    {
        temp = vinode_bmap[i];
        
        for (j = 0; j < 32; j++)
        {
            if (temp & 1)
            {
                ++num;
            }
            else
            {
                new = 1 << j;
                vinode_bmap[i] ^= new;
                return ((32 * i) + j + 1);
            }
            temp >>= 1;
        }
    } 
    /* Shouldn't happen! */
    assert(1 < 0);
    return 0;
}

/* Simple hash function */
int hash(char *s)
{
    int i;
    int n = 0;
    
    for (i = 0; i < strlen(s); i++) {
        n += s[i];
    }
    if (n % 1024 == 0)
        return 7;
    else
        return n % 1024;
}

/* This function returns a fileid given a vcfs_fhdata */
vcfs_fileid *get_fh(vcfs_fhdata *h)
{
    vcfs_fileid *ret;
    
    if (h->magic != MAGICNUM) {
        /* Update the root handle */
        h->id = 1;
        h->key = 1;
        root_handle.fh.id = 1;
        memcpy((char *)&root_handle, (char *)h, sizeof(root_handle));
        return &root_node;
    }
 
    /* We didn't want the root */
    ret = find_fh("fake string", h->id, h->key);
    
    return ret;
}   

/* Look up a filehandle in the cache */
vcfs_fileid *find_fh(char *name, int id, int key)
{
    int bucket;
    vcfs_fileid *i;
    
    bucket = key;
    
    i = file_hash[bucket];
    while (i != NULL) {
        if (i->id == id) {
            return i;
        }
        i = i->next;
    }
    
    return NULL;
}

/* Create a file id */
vcfs_fileid *create_fh(vcfs_path name, int v, vcfs_ventry *vent)
{
    vcfs_fileid *f;
    
    f = (vcfs_fileid *)malloc(sizeof(vcfs_fileid));
    
    strcpy(f->name, name);
    f->id = vent->id;
    f->hash_key = hash(f->name);
    f->ventry = vent;
    
    insert_fh(f);
    return f;
}

/* Return a fileid for the given pathname */
vcfs_fileid *lookup_fh_name(vcfs_path name)
{
    int h;
    vcfs_fileid *f;
    
    h = hash(name);
    
    for (f = file_hash[h]; f != NULL; f = f->next)
    {
      if (strcmp(f->name, name) == 0)
      {
          return f;
      }
    }
    return NULL;
}

/* TODO - This should return something, it could fail */
void insert_ventry(vcfs_ventry *v)
{
    vcfs_path parent;
    vcfs_name entry;
    vcfs_fileid *f;
    vcfs_ventry *p;
    

    /* First get a pointer to the parent ventry */
    split_path(v->name, &parent, &entry);
    f = lookup_fh_name(parent);
    
    if (f == NULL)
    {
        printf("insert_ventry: Couldn't find parent %s for %s\n", parent,v->name);
        return;
    }
    
    if (f->ventry == NULL)
    {
        fprintf(stderr, "insert_ventry: ventry of parent %s is NULL!!!\n", 
                parent);
        return;
    }
    p = f->ventry;
    
    v->next = p->dirent;
    p->dirent = v;
    
    return;
}
    
/* Create a ventry */
vcfs_ventry *create_ventry(vcfs_path name, int size, ftype type,
                           unsigned int mode, char *ver, time_t t, char *tag)
{
    vcfs_ventry *v;
    
    v = (vcfs_ventry *)malloc(sizeof(vcfs_ventry));
    
    strcpy(v->name, name);
    v->id = alloc_vinode();
    v->size = size;
    v->type = type;
    v->mode = mode;
    if (ver != NULL)
    {
        strncpy(v->ver, ver, VCFS_VER_LEN);
    }
    v->ctime = t;
    v->next = NULL;
    v->dirent = NULL;
    if (tag != NULL)
    {
        strncpy(v->tag, tag, VCFS_TAG_LEN);
    }
    
    insert_ventry(v);
    return v;
    
}

/* Put a file into the cache */
void insert_fh(vcfs_fileid *f)
{
    f->next = file_hash[f->hash_key];
    file_hash[f->hash_key] = f;
}

/* Lookup a file in the cache */
vcfs_fileid *lookuph(vcfs_fileid *d, char *name, vcfs_fhdata *fh)
{
    char path[NFS_MAXPATHLEN];
    vcfs_fileid *f;
    vcfs_path short_name;
    vcfs_ver ver;
    int extended = 0;
    int size;

    fh->magic = MAGICNUM;
    
    if (cvs_ver_extended(name, &short_name, &ver))
    {
        extended = 1;
        sprintf(path, "%s/%s", d->name, short_name);
    }
    else if (strlen(d->name) != 0)
    {
        sprintf(path,"%s/%s", d->name, name);
    }
    else
    {
        sprintf(path, "%s", name);
    }
    
    f = lookup_fh_name(path);
    
    if (f == NULL)
    {
        return NULL;
    }
    
    if (!extended)
    {
        fh->id = f->id;
        fh->key = hash(f->name);
    }
    else
    {
        /* Look for the extended name */
        if (!vcfs_validate_version(f->ventry, ver))
        {
            /* The client asked for a version number that does not exists */
            return NULL;
        }
        
        sprintf(path, "%s/%s", d->name, name);
        
        size = f->ventry->size;
        f = lookup_fh_name(path);
        
        if (f != NULL)
        {
            fh->id = f->id;
            fh->key = hash(f->name);
        }
        else
        {
            vcfs_ventry *v;
            
            /* Construct it */
            v = create_ventry(path, size, NFREG, 0, ver, (time_t)0, NULL);
            f = create_fh(path, 1, v);
            
            fh->id = v->id;
            fh->key = hash(path);
        }
    }
    
    return f;
}

/* Checkout the project from CVS, and build and in-memory representation
 * of it. Currently, this will stay around forever. Soon, there will be an option
 * to dynamically update directory contents based on activity in the repository.
 */
int vcfs_build_project()
{
    int r;
    cvs_buff *expand_buff; 
    cvs_buff *co_buff;
    char *beg;
    char *end;
    vcfs_path temp;
    int n;
    char *line;
    int i;
    time_t current_time;
    char ver[16];
    int count = 0;

    time(&current_time);
    
    /* Expand the module */
    r = cvs_expand_modules(&expand_buff);
    
    /* Grab the top dir of the project from the response */
    beg = (char *)memchr(expand_buff->data, ' ', expand_buff->size);
    if (beg == NULL)
    {
        fprintf(stderr, "Cannot understand expand-modules response\n");
        return 0;
    }
    beg++;
    
    end = (char *)memchr(beg, '\012', expand_buff->size);
    if (end == NULL || end < beg)
    {
        fprintf(stderr, "Cannot read name in expand-modules response\n");
        return 0;
    }
    
    memset(temp, 0, sizeof(temp));
    memcpy(temp, beg, (end - beg));
    
    init_vinode_bmap();
    
    /* Prepare the root dir of the project */
    ventry_list = (vcfs_ventry *)malloc(sizeof(vcfs_ventry));

    /*strcpy(ventry_list->name, temp);*/
    ventry_list->id = 3; /* The very top dir is 1 */
    ventry_list->size = 2048;
    ventry_list->type = NFDIR;
    ventry_list->next = NULL;
    ventry_list->dirent = NULL;


    for (i = 0; i < strlen(beg); i++)
    {
        if (beg[i] == '/' || beg[i] == '\012')
        {
            vcfs_path mod_path;
            strncpy(mod_path, beg, i);
            mod_path[i] = '\0';
            
            if (count == 0) 
            {
                /* This is the root dir */
                strcpy(ventry_list->name, mod_path);
                create_fh(ventry_list->name, 1, ventry_list);
            }
            else 
            {
                /* Insert a new directory */
                vcfs_ventry *v;
                
                v = create_ventry(mod_path, 2048, NFDIR, 
                                  0, NULL, current_time, NULL);
                create_fh(mod_path, 1, v);
            }
            
            count++;
        }
        
        
        if (beg[i] == '\012')
        {
            /* All that's left is the 'ok' */
            break;
        }
    }
    
    /* Check out the project */
    r = cvs_co(&co_buff, NULL);
    
    /* Parse the response buffer, build the rest of the ventry tree */
    cvs_buff_read_line(co_buff, &line); /* Swallow first line. TBD - really? */
    while((n = cvs_buff_read_line(co_buff, &line)) > 0)
    {
        int i;
        vcfs_ventry *v;
        vcfs_path path;
        int size;
        int beg_ver;
        int real_size;
        
        memset(path, 0, sizeof(path));
        if (line[0] == 'E')
        {
            /* Looks like a directory. 
             * HACK - Everything from line[23] on is the dir name.
             * TODO - Make extra sure this is a dir and not another message.
             */
            strncpy(path, line + 23, (strlen(line) - 23));
            v = create_ventry(path, 2048, NFDIR, 0, NULL, current_time, NULL);
            create_fh(path, 1, v);
            fprintf(stderr, "create dir %s\n", path);
        }
        else if (line[0] == 'M')
        {
            /* Looks like a file */
            /* TODO - Get the mode */
            /* HACK - Filename is from line[4] on */
            char *tag = NULL;
            
            strncpy(path, line + 4, (strlen(line) - 4));
            free(line);
            
            for (i = 0; i < 2; i++) 
            {
                cvs_buff_read_line(co_buff, NULL);
            }
            
            /* Get the version */
            cvs_buff_read_line(co_buff, &line);
            beg_ver = 0;
            for (i = 1; i < strlen(line); i++)
            { 
                if (line[i] == '/' && beg_ver == 0)
                {
                    beg_ver = i + 1;
                }
                else if (line[i] == '/' && beg_ver > 0)
                {
                    strncpy(&ver[0], (line + beg_ver), (i - beg_ver));
                    ver[i - beg_ver] = '\0';
                    break;
                }
            }

            /* Get the tag if it exists */
            beg = strrchr(line, '/');
            if (beg != NULL && (char)*(beg + 1) == 'T')
            {
                if (beg + 2 < line + strlen(line))
                {
                    tag = beg + 2;
                }
            }
            
            free(line);
            
            cvs_buff_read_line(co_buff, NULL); /* permissions */
            cvs_buff_read_line(co_buff, &line);
            
            if (line[0] == 'z')
            {
                char buff[8192];
                /* We are using compression - this is the compressed size */
                size = atoi(line + 1);
                
                /* Get the uncompressed size */
                real_size = cvs_zlib_inflate_buffer(co_buff, size, 0,
                                                    buff, 8192, FALSE);
            }
            else
            {
                real_size = size = atoi(line);
            }
            
            v = create_ventry(path, real_size, NFREG, 0, ver, current_time, tag);
            create_fh(path, 1, v);
            fprintf(stderr, "  create file %s\n", path);
            
            /* Skip over the file data */
            co_buff->cookie += size;
        }
        
        free(line);
    }
    
    cvs_free_buff(expand_buff);
    cvs_free_buff(co_buff);

    /* Allocate the read cache */
    read_cache = malloc(sizeof(vcfs_read_cachent));
    
    return r;
}

/* Perform a read. The very simplistic "cache" used here simply
 * slurps up files in large chunks and stores the current chunk for the most
 * recently requested file.
 */
int vcfs_read(char *buff, vcfs_fhdata *fh, int count, int offset)
{
    cvs_buff *resp;
    vcfs_fileid *f;
    int i;
    char *line;
    int size;
    int len;
    int cache_end;
    int partial_read = 0;
    int cached_read = FALSE;
    vcfs_path filename;
    vcfs_ver ver;
    int compressed_offset;
    bool compressed = FALSE;
    
    f = get_fh(fh);
    
    assert(f != NULL);
    
    /* Adjust the name if it is version extended */
    if (!cvs_ver_extended(f->name, &filename, &ver))
    {
        strcpy(filename, f->name);
    }
    
    cache_end = read_cache->start + read_cache->size;
    
    /* Are we reading from our cache? */
    if (read_cache->id == f->id)
    {
        cached_read = TRUE;

        if (read_cache->size == READ_CACHE_SIZE && offset + count > cache_end)
        {
            /* We are reading beyond our current cache */
            if (offset < cache_end)
            {
                /* Copy what we have into the buffer, go get the rest */
                partial_read = cache_end - offset;
                memcpy(buff, read_cache->data + offset, partial_read);
                count -= partial_read;
            }
        }
        else
        {
            /* We have everything we need in cache, return it */
            
            memcpy(buff, read_cache->data + offset - read_cache->start, count);
            len = count;
            return len;
        }
    }
    
    /* Populate the cache for the next read to this file */
    read_cache->id = f->id;
    
    cvs_get_file(filename, f->ventry->ver, &resp);
    
    for (i = 0; i < 5; i++)
    {
        cvs_buff_read_line(resp, NULL);
    }
    
    cvs_buff_read_line(resp, &line);
    
    if (line[0] == 'z')
    {
        /* This file is compressed */
        compressed = TRUE;
        size = atoi(line + 1);
        
        /* Get the uncompressed chunk of the file we need to fill the cache */
        if (cached_read)
        {
            compressed_offset = read_cache->start + read_cache->size;
        }
        else
        {
            compressed_offset = 0;
        }
        
        size = cvs_zlib_inflate_buffer(resp, size, compressed_offset, read_cache->data,
                                       READ_CACHE_SIZE, TRUE);
    }
    else
    {
        size = atoi(line);
    }
    free(line);
    
    f->ventry->size = size;
    
    if (cached_read)
    {
        /* The file has already been cached, but we need to read more */
        if (size - read_cache->size > READ_CACHE_SIZE)
        {
            size = READ_CACHE_SIZE;
        }
        read_cache->start += read_cache->size;
    
        if (!compressed)
        {
            memcpy(read_cache->data, resp->data + resp->cookie + read_cache->start,
                   size);
        }
        read_cache->size = size;
        offset -= read_cache->start;
    }
    else
    {
        if (size > READ_CACHE_SIZE)
        {
            size = READ_CACHE_SIZE;
        }
        
        if (!compressed)
        {
            memcpy(read_cache->data, resp->data + resp->cookie, size);
        }
        
        read_cache->size = size;
        read_cache->start = 0;
        
    }
    
    if (offset + count > read_cache->start + read_cache->size)
    {
        memcpy(buff + partial_read, (read_cache->data + offset),
               read_cache->size - offset);
        len = size - offset;
    }
    else
    {
        memcpy(buff + partial_read, (read_cache->data + offset), count);
        len = count + partial_read;
    }
    
    //printf("The data we read is %s\n", buff);
    
    cvs_free_buff(resp);
    
    return len;
}


/* Make sure the given version is actually a version of the specified file */
int vcfs_validate_version(vcfs_ventry *v, char *ver)
{
    cvs_buff *resp;
    int n;
    char *next_ver;
    int valid = 0;
    
    cvs_get_log(v->name, &resp);
    
    while(cvs_get_log_info(resp, &next_ver, NULL, NULL, NULL) > 0)
    {
        if (strcmp(ver, next_ver) == 0)
        {
            /* It's a valid version */
            valid = 1;
        }
        
        free(next_ver);
    }
    
    cvs_free_buff(resp);
    return valid;
    
}


