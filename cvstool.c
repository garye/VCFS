#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "cvstool.h"
#include "vcfs.h"

CLIENT *clnt;

char *MOUNT;
char *PROG_NAME;

void cvstool_usage(char *msg);
int cvstool_do_ls(char **argv, int argc);
int cvstool_do_lsver(char **argv, int argc); 
char *cvstool_get_rel(char *path);


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
    
    clnt = clnt_create("localhost", CVSTOOL_PROGRAM, CVSTOOL_VERSION, "udp");
    
    if (clnt == (CLIENT *)NULL) {
        clnt_pcreateerror("localhost error");
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
    else
    {
        cvstool_usage("Invalid command specified");
    }
    
    clnt_destroy(clnt);
    
    exit(status);

}

void cvstool_usage(char *msg) 
{
    printf("%s: %s\n", PROG_NAME, msg);
    
}

char *cvstool_get_rel(char *path)
{
    
    assert(path != NULL);

    if (strlen(path) <= strlen(MOUNT))
    {
        return NULL;
    }
    
    if (strncmp(path, MOUNT, strlen(MOUNT)) == 0)
    {
        return (path + strlen(MOUNT) + 1);
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

    args.path = strdup(rel_path);

    resp = cvstool_ls_1(&args, clnt);
    
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
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
     switch (opt)
        {
        case 'l':
        {
            args.option |= CVSTOOL_LSVER_LONG;
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
