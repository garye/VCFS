#ifndef _VCFS_H_
#define _VCFS_H_ 1

#include <stdio.h>
#include "nfsproto.h"


#define debug printf

#define MAGICNUM 0xaaaa4444

#define VCFS_ROOT "/tmp" /* TEMP: This is not used right now */
#define VCFS_PORT 3155

enum f_type {
    F_DIR,
    F_FILE,
    F_LINK,
};
typedef enum f_type f_type;

typedef char vcfs_path[NFS_MAXPATHLEN];
typedef char vcfs_name[NFS_MAXNAMLEN];
typedef char vcfs_ver[16];

typedef struct vcfs_fhdata {
    unsigned int magic;
    int id;
    int key;
} vcfs_fhdata;

/* This is a hash table entry */
typedef struct vcfs_fileid {
    int id;
    int hash_key;
    vcfs_path name;
    int virtual;
    struct vcfs_fileid *next;
    struct vcfs_ventry *ventry; /* NULL if not a virtual file */
} vcfs_fileid;

/* Virtual (cvs-controlled) file entry */
typedef struct vcfs_ventry {
    int id;
    vcfs_path name;
    unsigned int size;
    ftype type;
    /* TODO - File stats go here */
    unsigned int mode;
    char ver[16];
    unsigned int ctime;
    struct vcfs_ventry *next; /* Next ventry in the current directory */
    struct vcfs_ventry *dirent; /* A list of dir entries if this is a dir */
} vcfs_ventry;

/* TODO: Move these */
int UID, GID;

/* TBD - Is this necessary? Used for root fh */
typedef union fh_ut {
	unsigned char opaque[NFS_FHSIZE];
	struct vcfs_fhdata fh;
} fh_ut;

/* We want to ask the server for file contents as infrequently as possible.
 * Grab 512K of a file at a time. For bigger files, we have to ask the
 * server for the ENTIRE file for each 512K chunk we grab!
 * TBD: Is this really true? Make cache size bigger?
 */
#define READ_CACHE_SIZE 512 * 1024
typedef struct vcfs_read_cachent {
    char data[READ_CACHE_SIZE];
    int id;
    int start;
    int size;
} vcfs_read_cachent;

int hash(char *s);

int vcfs_build_project();
vcfs_fileid *get_fh(vcfs_fhdata *h);
vcfs_fileid *find_fh(char *name, int id, int key);
void insert_fh(vcfs_fileid *f);
vcfs_fileid *create_fh(vcfs_path name, int v, vcfs_ventry *vent);
void insert_ventry(vcfs_ventry *v);
vcfs_ventry *create_ventry(vcfs_path name, int size, ftype type,
			    unsigned int mode, char *ver, time_t t);
vcfs_fileid *lookuph(vcfs_fileid *d, char *name, vcfs_fhdata *fh);
vcfs_fileid *lookup_fh_name(vcfs_path name);
int vcfs_read(char *buff, vcfs_fhdata *fh, int count, int offset);

    
#endif















