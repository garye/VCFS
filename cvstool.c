/*****************************************************************************
 * File: cvstool.c
 * Command-line client for the cvstool utility. Parse options, and call the
 * appropriate client stub.
 ****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "cvstool.h"
#include "vcfs.h"

CLIENT *clnt;
bool   client_setup = FALSE;

char *MOUNT;
char *PROG_NAME;

void cvstool_usage(char *msg);
void cvstool_do_help();
int cvstool_do_ls(char **argv, int argc);
int cvstool_do_lsver(char **argv, int argc); 
char *cvstool_get_rel(char *path);
void cvstool_setup_client(void);

int main(int argc, char **argv)
{
    int status;
    
    PROG_NAME = "cvstool";

    MOUNT = getenv("VCFS_MOUNT");
    
    if (MOUNT == NULL)
    {
        fprintf(stderr, "Please set the VCFS_MOUNT environment variable\n");
        exit(1);
    }

    /* Need at least the command argument */
    if (argc < 2) {
        cvstool_usage("No command specified");
        exit(1);
    }
    
    /* Get the command */
    if (!strcmp(argv[1], "ls"))
    {
        status = cvstool_do_ls(argv, argc);
    }
    else if (!strcmp(argv[1], "lsver"))
    {
        status = cvstool_do_lsver(argv, argc);
    }
    else if (!strcmp(argv[1], "help") || !strcmp(argv[1], "-h")) 
    {
        cvstool_do_help();
    }
    else
    {
        cvstool_usage("Invalid command specified");
    }
    
    if (client_setup)
    {
        clnt_destroy(clnt);
    }
    
    exit(status);

}

void cvstool_setup_client(void)
{
    clnt = clnt_create("localhost", CVSTOOL_PROGRAM, CVSTOOL_VERSION, "udp");
    
    if (clnt == (CLIENT *)NULL) {
        clnt_pcreateerror("localhost error");
        exit(1);
    }
    
    client_setup = TRUE;

    return;
}

void cvstool_usage(char *msg) 
{
    printf("%s: %s\n", PROG_NAME, msg);
    printf("%s: Type \'%s help\' for more information.\n", PROG_NAME, PROG_NAME);
}

void cvstool_do_help() 
{
    printf("%s: Usage: %s COMMAND [FILENAME] \n", PROG_NAME, PROG_NAME);
    printf("%s: Perform COMMAND on FILENAME or current directory\n", PROG_NAME);
    printf("%s: Valid commands are:\n", PROG_NAME);
    printf("%s:   ls              List the current version of the file or directory\n", PROG_NAME);
    printf("%s:   lsver [-l] [-p] List all of the versions of this file\n", PROG_NAME);
    printf("%s:   lsver -l        List all of the versions of this file with more information\n", PROG_NAME);
    printf("%s:   lsver -p        List the previous version of this file\n", PROG_NAME);

    /* This should be last */
    printf("%s:   help            Prints out this help message\n", PROG_NAME);
    printf("%s: Report bugs through http://sourceforge.net/projects/vcfs \n", PROG_NAME);
}

/*
 * Translates path from a fully qualified path, that may contain
 * relative references to a path relative to the VCFS_MOUNT env w/o
 * relative references. example: 
 * input:  /vcfs/vcfs/doc/../utils.h
 * output: vcfs/utils.h
 */
char *cvstool_get_rel(char *path)
{    
    /* TODO: Is this big enough? This was just a guess... */
    char resolved_path [255];
    char *temp_path;
   
    assert(path != NULL);

    if (strlen(path) <= strlen(MOUNT))
    {
        return NULL;
    }
    
    if (strncmp(path, MOUNT, strlen(MOUNT)) == 0)
    {

	/* Remove relative references in path */
	path = realpath(path, resolved_path);

	/* Couldn't resolve them - doh */
	if (path == NULL) 
	    return NULL;
	
	temp_path = strdup(path);
        return (temp_path + strlen(MOUNT) + 1);
    }
    else
    {
        return NULL;
    }
    
}

int cvstool_do_ls(char **argv, int argc)
{
    cvstool_ls_args args;
    cvstool_ls_resp *resp;
    int count = 0;
    cvstool_dirent *dirent;
    int opt;
    vcfs_path temp_path;
    char *rel_path;

    /* Start looking for options at argv[2], suppress errors from getopt */
    optind = 2;
    opterr = 0;

    args.option = 0;

    /* Get all options for the 'ls' command */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
        {
            args.option |= CVSTOOL_LS_LONG;
            break;
        }   
        default:
            cvstool_usage("Invalid option specified");
            exit(1);
        }
    }
    
    getcwd(temp_path, sizeof(temp_path));

    if (optind < argc)
    {
        /* The user specified a file or directory */
        strcat(temp_path, "/");
        strcat(temp_path, argv[optind]);
    }
    
    rel_path = cvstool_get_rel(temp_path);
    
    if (rel_path == NULL)
    {
        fprintf(stderr, "%s: Not a valid vcfs path: %s\n",
                PROG_NAME, temp_path);
        exit(1);
    }

    args.path = rel_path;

    cvstool_setup_client();

    /* Make the RPC call */
    resp = cvstool_ls_1(&args, clnt);
    
    if (resp == NULL) {
        clnt_perror(clnt, "error receiving response");
        exit(1);
    }

    /* Check for errors */
    if (resp->status != CVSTOOL_OK)
    {
	printf("status was NOT ok\n");
        /* TODO: Display appropriate message */
        return resp->status;
    }
    
    dirent = resp->dirents;
    
    /* Format and print the results */
    while (dirent != NULL && count < resp->num_resp)
    {
        vcfs_path path;
        
        strcpy(path, dirent->name);
        strcat(path, ",");
        strcat(path, dirent->ver_info.ver);

        /* 55 Characters for the name... random */
        printf("%-55s", path);
        
        if (args.option & CVSTOOL_LS_LONG)
        {
            printf("   %s   %s", dirent->ver_info.author,
                   dirent->ver_info.date);
        }

        printf("\n");

        dirent = dirent->next;
        count++;
    }
    
    return resp->status; 
    
}

int cvstool_do_lsver(char **argv, int argc)
{
    cvstool_lsver_args args;
    cvstool_lsver_resp *resp;
    int count = 0;
    cvstool_ver_info *ver_info;
    int opt;
    vcfs_path temp_path;
    char *rel_path;
    
    
    /* Start looking for options at argv[2], suppress errors from getopt */
    optind = 2;
    opterr = 0;
    
    args.option = 0;
    
    /* TEMP */
    args.from_ver = strdup("");

    /* Get all options for the 'lsver' command */
    /* TODO: Implement other options */
    while ((opt = getopt(argc, argv, "lp")) != -1)
    {
        switch (opt)
        {
        case 'l':
        {
            args.option |= CVSTOOL_LSVER_LONG;
            break;
        }
        case 'p':
        {
            args.option |= CVSTOOL_LSVER_PRE;
	    break;
        }
        default:
            cvstool_usage("Invalid option specified");
            exit(1);
        }
    }
    
    getcwd(temp_path, sizeof(temp_path));
    
    if (optind < argc)
    {
        /* The user specified a file or directory */
        strcat(temp_path, "/");
        strcat(temp_path, argv[optind]);
    }
    else
    {
        cvstool_usage("No filename specified");
        exit(1);
    }
    
    rel_path = cvstool_get_rel(temp_path);
    
    if (rel_path == NULL)
    {
        fprintf(stderr, "%s: Not a valid vcfs path: %s\n",
                PROG_NAME, temp_path);
        exit(1);
    }
    
    args.path = strdup(rel_path);
    
    cvstool_setup_client();
    
    /* Make the RPC call */
    resp = cvstool_lsver_1(&args, clnt);
    
    if (resp == NULL) {
        clnt_perror(clnt, "error receiving response");
        exit(1);
    }
    
    /* Check for errors */
    if (resp->status != CVSTOOL_OK)
    {
        /* TODO: Display appropriate message */
        return resp->status;
    }

    ver_info = resp->vers;
    
    while (ver_info != NULL && count < resp->num_resp)
    {
        printf("%-16s", ver_info->ver);
        
        if (args.option & CVSTOOL_LSVER_LONG)
        {
            printf("%-24s %-32s", ver_info->author, ver_info->date);
        }
        
        printf("\n");
        
        count++;
        ver_info = ver_info->next;
    }
    
    return resp->status;
}
