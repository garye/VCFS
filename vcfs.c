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
#include "nfsproto.h"
#include "vcfs.h"
#include "cvs_cmds.h"
#include "cvstool.h"

void usage(char *msg);

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
    char *pword;
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
    

    /* Get the user's cvs password */
    pword = getpass("Enter CVS password:");
    
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
    
    vcfs_build_project();
    printf("CVS project %s successfully mounted\n", argv[3]);
	svc_run();
    exit(1);
}

void usage(char *msg)
{
    if (msg != NULL) {
        fprintf(stderr, "%s: %s\n", progname, msg);
    }
    
    fprintf(stderr, "Usage: %s [OPTION] HOSTNAME CVSROOT PROJECT USERNAME\n\n",
            progname);
    fprintf(stderr, "-n\tDon't gzip file contents\n");
    fprintf(stderr, "-t TAG\tLoad the contents of the repository specified by TAG\n");
}

