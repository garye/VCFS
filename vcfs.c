#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>

#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/time.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <pwd.h>
#include "nfsproto.h"
#include "vcfs.h"
#include "cvs_cmds.h"
#include "cvstool.h"

#ifndef CVS_PASSWORD_FILE 
#define CVS_PASSWORD_FILE ".cvspass"
#endif

/* Prints the usage of vcfsd */
void usage(char *msg);

/* Tries to get the password for this pserver from the cvs password
   file (if it exists) */
char* get_cvs_passwd_from_file(char *user, char *hostname);

struct in_addr validhost;

void nfs_program_2();
extern void cvstool_program_1(struct svc_req *rqstp, register SVCXPRT *transp);

char *progname;

int main(int argc, char **argv)
{
    int port;
    struct hostent *hp;
    struct sockaddr_in sin;
    int svrsock;
    SVCXPRT *tp;
    char *pword = NULL;
    register SVCXPRT *transp;
    char *module;
    char *root;
    char *hostname;
    char *user;
    bool use_gzip = TRUE;
    char *tag = NULL;
    int opt;

    progname = argv[0];
    port = VCFS_PORT;

    if (argc < 5) {
        usage(NULL);
        exit(1);
    }
    
    /* Get command options */
    opterr = 0;
    while ((opt = getopt(argc, argv, "nt:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            use_gzip = FALSE;
            break;
            
        case 't':
            tag = optarg;
            break;
        default:
            usage("Invalid option.");
            exit(1);
        }
        
    }
    
    /* Get the four required arguments */
    if (optind + 4 > argc)
    {
        usage(NULL);
        exit(1);
    }
    
    hostname = argv[optind++];
    root = argv[optind++];
    module = argv[optind++];
    user = argv[optind];
    
    /* Try to get the cvs password from the .cvspass file */
    pword = get_cvs_passwd_from_file(user, hostname);

    /* Couldn't find the password in their .cvspass file, so
     * we need to ask the user for it. */
    if (pword == NULL) {

	/* Get the user's cvs password */
	pword = getpass("Enter CVS password:");
    }

    /* Now register our nfs server on the localhost */
    if ((hp = gethostbyname("localhost")) == NULL) {
	    fprintf(stderr, "Cannot resolve localhost.\n");
	    exit(1);
	}
	
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
	memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
    
	validhost.s_addr = sin.sin_addr.s_addr;
	sin.sin_port = htons(port);
    
	if ((svrsock = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
        perror("error creating socket");
        exit(1);
	}
    
	if (bind(svrsock,(struct sockaddr *)&sin,sizeof(sin)) != 0) {
        perror("error binding to socket");
        exit(1);
	}
    
	if ((tp = svcudp_create(svrsock)) == NULL) {
        fprintf(stderr,"Unable to create UDP RPC Service\n");
        exit(1);
	}
	
	/* register the service */
    if (!svc_register(tp, NFS_PROGRAM, NFS_VERSION, nfs_program_2, IPPROTO_UDP)) {
	    fprintf(stderr,"Unable to register vcfsd NFS server\n");
	    exit(1);
	}
    
    pmap_unset(CVSTOOL_PROGRAM, CVSTOOL_VERSION);
    
	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf (stderr, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, CVSTOOL_PROGRAM, CVSTOOL_VERSION, cvstool_program_1, IPPROTO_UDP)) {
		fprintf (stderr, "unable to register (CVSTOOL_PROGRAM, CVSTOOL_VERSION, udp).");
		exit(1);
	}
    
	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf (stderr, "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, CVSTOOL_PROGRAM, CVSTOOL_VERSION, cvstool_program_1, IPPROTO_TCP)) {
		fprintf (stderr, "unable to register (CVSTOOL_PROGRAM, CVSTOOL_VERSION, tcp).");
		exit(1);
	}
    
    UID = getuid();
    GID = getgid();
    
    cvs_init_session(hostname, root, module, user, 
                     pword, VCFS_ROOT, use_gzip, tag);
    
    if (cvs_pserver_connect() < 0) {
        fprintf(stderr, "Authentication on CVS server failed\n");
        exit(1);
    }
    
    if (!vcfs_build_project())
    {
        /* Error building the project */
        fprintf(stderr, "Error mounting repository\n");
        exit(1);
    }

    printf("CVS project %s successfully mounted\n", argv[3]);
	svc_run();
    exit(1);
}

/* Returns the password for this pserver from the cvs password
   file (if it exists), NULL otherwise */
char* get_cvs_passwd_from_file(char *user, char *hostname) 
{
    
    char* home = NULL; /* path to home dir*/
    char* home_env = NULL; /* contents of home env */
    struct passwd *pw; /* passwd struct - see man getpwuid for details*/
    char* passfile = NULL; /* path to the password file */
    FILE *fp; /* ptr to open cvspass file */
    char *user_at_host; /* of form username@host */
    int line_length;
    long line = 0;
    char *linebuf = NULL;
    size_t linebuf_len;
    char *passwd_chunk;
    char *password_tmp;
    char *password;
    int i = 0;

    /* First find their home directory */
    if ((home_env = getenv("HOME")) != NULL) {
	/* They had HOME set. */
	home = home_env;
    }
    else if ((pw = (struct passwd *) getpwuid (getuid()))
	     && pw->pw_dir) {

	/* We got it from the passwd file */
	home = strdup(pw->pw_dir);
    }
    else {
	/* Couldn't find their home dir.
	 * We are outta luck */
	return NULL;
    }
	
    /* Make space for filename to password file - TODO: what is the +3? */
    passfile = malloc( strlen(home) + strlen(CVS_PASSWORD_FILE) + 3);
    
    strcpy(passfile, home); /* Copy the path to user's home */

    strcat(passfile, "/"); /* append slash to home dir*/

    strcat(passfile, CVS_PASSWORD_FILE); /* append cvs passwd file name*/

    fp = fopen(passfile, "r"); /* open that file for reading */

    if (fp == NULL) {
	return NULL; /* Couldn't open the file */
    }

    /* We have their CVS passwd file. Let's check each line for an
       appropriate entry */
    while ((line_length = getline( &linebuf, &linebuf_len, fp)) >= 0) {

	line++;

	/* Get mem for full name */
	user_at_host = malloc( strlen(user) + strlen(hostname) + 3);	

	/* Form into 'user@hostname' */	
	strcat(user_at_host, user);
	strcat(user_at_host, "@");
	strcat(user_at_host, hostname);

	/* Search for the username and server in the line */	
	if ( (strstr(linebuf, user_at_host) != NULL) &&
	     /* Search backword for first space char. */
	     ((passwd_chunk = rindex(linebuf, ' ')) != NULL) &&
	     /* Then for the char A. (ex. 'Aencryptedpasswd') */
	     ((passwd_chunk = rindex(linebuf, 'A')) != NULL) ) {	    

		/* Copy the chunk we found and trailing newline */
		password_tmp = malloc( strlen(passwd_chunk) );
		
		/* skip initial space char vvv and skip trailing \n vvvvv */
		strncpy(password_tmp, passwd_chunk + 1, strlen(passwd_chunk)-2);
		
		/* Unscrable passwd - the method is symetric */
		password = scramble( password_tmp );
		
		/* Shift the whole string one char to the left,
		   pushing the unwanted 'A' off the left end.
		   Safe, because s is null-terminated. Taken from
		   CVS client code */
		for (i = 0; password[i]; i++) 
		    password[i] = password[i + 1];
		
	    }
	    else {
		/* Couldn't find or parse the passwd */
		password = NULL; 
	    }
	}

    /* Close the passwd file */
    if ( fclose(fp) < 0 ) {
	/* TODO: Should we warn the user we couldn't close their
           passwd file?? */
    }

    /* release string memory */
    free(home);
    free(passfile); 
    free(user_at_host);
    free(password_tmp);

    return password;
}

/* Prints the usage of vcfsd, with given message (if
   provided). Otherwise prints the default message. */
void usage(char *msg)
{
    if (msg != NULL) {
        fprintf(stderr, "%s: %s\n", progname, msg);
    }
    
    fprintf(stderr, "Usage: %s [OPTION] HOSTNAME CVSROOT PROJECT USERNAME\n\n",
            progname);
    fprintf(stderr, "-n\tDon't gzip file contents\n");
    fprintf(stderr, "-t TAG\tLoad the version of the repository specified by TAG, which is either a branch or tag name\n");
}

