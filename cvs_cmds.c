/*****************************************************************************
 * File: cvs_cmds.c
 * Functions for communicating with the CVS server. Responses from the server
 * are read into cvs_buff structures, and are read one line at a time by
 * cvs_read_line. 
 ****************************************************************************/

#include <sys/poll.h>
#include "cvs_cmds.h"
#include "utils.h"


static unsigned char
shifts[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  114,120, 53, 79, 96,109, 72,108, 70, 64, 76, 67,116, 74, 68, 87,
  111, 52, 75,119, 49, 34, 82, 81, 95, 65,112, 86,118,110,122,105,
   41, 57, 83, 43, 46,102, 40, 89, 38,103, 45, 50, 42,123, 91, 35,
  125, 55, 54, 66,124,126, 59, 47, 92, 71,115, 78, 88,107,106, 56,
   36,121,117,104,101,100, 69, 73, 99, 63, 94, 93, 39, 37, 61, 48,
   58,113, 32, 90, 44, 98, 60, 51, 33, 97, 62, 77, 84, 80, 85,223,
  225,216,187,166,229,189,222,188,141,249,148,200,184,136,248,190,
  199,170,181,204,138,232,218,183,255,234,220,247,213,203,226,193,
  174,172,228,252,217,201,131,230,197,211,145,238,161,179,160,212,
  207,221,254,173,202,146,224,151,140,196,205,130,135,133,143,246,
  192,159,244,239,185,168,215,144,139,165,180,157,147,186,214,176,
  227,231,219,169,175,156,206,198,129,164,150,210,154,177,134,127,
  182,128,158,208,162,132,167,209,149,241,153,251,237,236,171,195,
  243,233,253,240,194,250,191,155,142,137,245,235,163,242,178,152 };

static cvs_args *args;
int DEBUG_RESP = 0;

/* Taken from the cvs client code */
char *scramble (char *str)
{
    int i;
    char *s;
    
    s = (char *) malloc (strlen (str) + 2);
    
    s[0] = 'A';
    strcpy (s + 1, str);
    
    for (i = 1; s[i]; i++)
    {
        s[i] = shifts[(unsigned char)(s[i])];
    }
    
    return s;
}

/* Initialize a socket */
static struct hostent *init_sockaddr(struct sockaddr_in *name, 
                                     char *hostname, unsigned int port)
{
    struct hostent *hostinfo;
    unsigned short shortport = port;
    
    memset (name, 0, sizeof (*name));
    name->sin_family = AF_INET;
    name->sin_port = htons (shortport);
    hostinfo = gethostbyname (hostname);
    
    if (hostinfo == NULL)
	{
	    fprintf (stderr, "Unknown host %s.\n", hostname);
	    exit(0);
	}

    name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
    return hostinfo;
}

/* Set CVS session arguments */
void cvs_init(char *hostname, char *root, char *module, char *user,
              char *password, char *dir)
{
    args = (cvs_args *)malloc(sizeof(cvs_args));

    args->hostname = strdup(hostname);
    args->root = strdup(root);
    args->module = strdup(module);
    args->user = strdup(user);
    args->password = scramble(password);
    args->dir = strdup(dir);
}

/* Returned a malloc'ed cvs_buff.
 * NOTE: Caller must free return value after use using cvs_free_buff.
 */
cvs_buff *cvs_get_buff()
{
    cvs_buff *b = (cvs_buff *)malloc(sizeof(cvs_buff));
    
    b->size = 0;
    b->data = (char *)malloc(CVS_BUFF_SIZE);
    memset(b->data, 0, CVS_BUFF_SIZE);
    b->cookie = 0;
    b->limit = CVS_BUFF_SIZE;
  
    return b;
}
 
/* Free a previously allocated cvs_buff */
void cvs_free_buff(cvs_buff *b)
{
    free(b->data);
    free(b);
}

/* Make sure the buffer has enough for n more bytes, grow if not */
void cvs_ensure_buff(cvs_buff *b, int n)
{
    if (b->size + n >= b->limit)
    {
        b->data = (char *)realloc(b->data, 2 * b->limit);
        b->limit = 2 * b->limit;
    }
}

/* See if the last thing we read is 'ok', meaning the message from the server
 * has completed successfully, there should be no more data.
 */
int cvs_check_ok(char *c, int n)
{
    if (n > 0) {
        return (c[n-3] == 'o' && c[n-2] == 'k' && c[n-1] == '\012');
    }
    else
    {
        return 0;
    }
}

/* Look for an error. Currently unused */
int cvs_check_error(char *c, int n)
{
    /* TBD - Make this SMARTER! We will miss many errors. */
    return (c[n-8] == 'e' && c[n-7] == 'r' && c[n-6] == 'r' 
            && c[n-5] == 'o' && c[n-4] == 'r');
}

/* Read one line from the buffer into a malloc'ed string.
 * NOTE: Caller must free line if not NULL.
 */
int cvs_buff_read_line(cvs_buff *b, char **line)
{
    int len;
    char *p;
    char *l;

    if (b->cookie >= b->size)
    {
        /* We've gone too far */
        return 0;
    }
    
    /* Look for the end of the line */
    p = (char *)memchr(b->data + b->cookie, '\012', b->size - b->cookie);
    
    if (p == NULL)
    {
        return 0;
    }
    
    len = p - (b->data + b->cookie);
    
    if (line != NULL)
    {
        l = (char *)malloc(len + 1);
        memcpy(l, (char *)(b->data + b->cookie), len);
        l[len] = '\0';
        
        *line = l;
    }
    
    b->cookie = b->cookie + len + 1;
    
    return (len + 1);
}

/* Read a response from the server into a cvs_buff */
cvs_buff *cvs_get_resp()
{
    int n = 0;
    cvs_buff *buff;
    char temp[CVS_READ_SIZE];
    int count = 0;
    struct pollfd fds;
    int result;

    fds.fd = args->sock;
    fds.events = POLLIN; /* Check if there is data waiting to be read */
    fds.revents = 0;

    buff = cvs_get_buff();

    do 
    {
        result = poll(&fds, 1, 3000);
        
        if (result == 0)
        {
            /* There is nothing to read */
            if (cvs_check_ok(temp, n))
            {
                /* We finished reading successfully */
                break;
            }
            else
            {
                /* There was an error. TODO: Handle this */
                debug("Error! Client read: \n%s\n", temp);
                cvs_free_buff(buff);
                return NULL;  
            }
        }
        
        memset(&temp, 0, CVS_READ_SIZE);
        n = recv(args->sock, &temp, CVS_READ_SIZE, 0);
        
        if (DEBUG_RESP)
        {
            printf("The data is:\n-------------\n%s\n----------------\n", temp);
        }
        
        cvs_ensure_buff(buff, n);
        
        memcpy(buff->data + buff->size, temp, n);
        buff->size += n;
        count++;
    } while(!cvs_check_ok(temp, n)); /* Read until last thing read is 'ok' */
    
    DEBUG_RESP = 0;
    return buff;
}

/* Connect to the CVS pserver and authenticate ourselves */
int cvs_pserver_connect() 
{
    int sock;
    struct sockaddr_in client_sock;
    struct hostent *hostinfo;
    char *begin = "BEGIN AUTH REQUEST\012";
    char *end = "END AUTH REQUEST\012";
    char resp[512];
    int n;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) 
    {
        fprintf(stderr, "Cannot create socket\n");
        return -1;
    }
    
    hostinfo = init_sockaddr (&client_sock, args->hostname, CVSPORT);
    
    if (connect(sock, (struct sockaddr *) &client_sock, 
                sizeof(client_sock)) < 0)
	{
	    fprintf(stderr, "Could not connect to server\n");
	    return -1;
	}
    
    /* Authenticate ourselves with the server */
    if (send (sock, begin, strlen (begin), 0) < 0)
        fprintf(stderr, "Can't send\n");
    if (send (sock, args->root, strlen(args->root), 0) < 0)
        fprintf(stderr, "Can't send\n");
    if (send (sock, "\012", 1, 0) < 0)
        fprintf(stderr, "Can't send\n");
    if (send (sock, args->user, strlen(args->user), 0) < 0)
        fprintf(stderr, "Can't send\n");
    if (send (sock, "\012", 1, 0) < 0)
        fprintf(stderr, "Can't send\n");
    if (send (sock, args->password, strlen(args->password), 0) < 0)
        fprintf(stderr, "Can't send\n");
    if (send (sock, "\012", 1, 0) < 0)
        fprintf(stderr, "Can't send\n");
    if (send (sock, end, strlen(end), 0) < 0)
        fprintf(stderr, "Can't send\n");
    
    /* Get the response */
    n = recv(sock, &resp, 512, 0);
    
    if (n < 0 || resp[2] != 'L')
    {
        /* The server didn't say "I Love you", so we weren't authenticated */
        return -1;
    }
    
    args->sock = sock;
    return sock;
}

/* Send data to the CVS server */
int cvs_send(int sock, char *msg)
{
    int n;

    if (msg == NULL)
    {
        fprintf(stderr, "Cannot send NULL message!\n");
        return -1;
    }
    
    n = send(sock, msg, strlen(msg), 0);
    if (n < 0)
    {
        fprintf(stderr, "Cannot send message: %s\n", msg);
        return -1;
    }
    else {
        /* Debug sends */
        debug("C (%d bytes): %s\n", n, msg);
    }

    return 1;
}

/* Send "expand-modules" CVS request */
int cvs_expand_modules(cvs_buff **resp)
{
    char cmd[1024];
    char buff[1024];
    
    memset(buff, 0, 1024);

    /*
    if (chdir(args->dir) != 0) {
        fprintf(stderr, "Could not chdir to directory %s\n", args->dir);
        return 0;
    }
    */

    sprintf(&cmd[0], "Root %s\012", args->root);
    cvs_send(args->sock, cmd);

    sprintf(&cmd[0], "Argument %s\012", args->module);
    cvs_send(args->sock, cmd);

    cvs_send(args->sock, "Directory .\012");
    
    sprintf(&cmd[0], "%s\012", args->root); /* Unecessary?? */
    cvs_send(args->sock, cmd);
    
    cvs_send(args->sock, "expand-modules\012");
    
    *resp = cvs_get_resp();
    
    return 1;
}

/* Checkout the module and get a complete dir listing. */
int cvs_co(cvs_buff **resp)
{
    char cmd[1024];
    time_t before;

    //cvs_send(args->sock, "gzip-file-contents 3\012");
    
    /* TBD - rewrite with sprintf!! */
    strcpy(cmd, "Argument ");
    strcat(cmd, args->module);
    strcat(cmd, "\012");
    cvs_send(args->sock, cmd);
    
    sprintf(cmd, "Directory .\012");
    cvs_send(args->sock, cmd);
    
    strcpy(cmd, args->root);
    strcat(cmd, "\012");
    cvs_send(args->sock, cmd);

    cvs_send(args->sock, "co\012");

    before = time(NULL);

    *resp = cvs_get_resp();
    printf("total time = %ld\n", time(NULL) - before);
    
    return 1;
}

/* Send an "update" CVS request */
int cvs_get_file(vcfs_path name, char *ver, cvs_buff **resp)
{
    char cmd[1024];
    vcfs_path parent;
    vcfs_name entry;
    
    split_path(name, &parent, &entry);
    
    sprintf(cmd, "Argument -r\012Argument %s\012", ver);
    cvs_send(args->sock, cmd);

    sprintf(cmd, "Argument -u\012Directory .\012%s/%s\012", args->root, parent);
    cvs_send(args->sock, cmd);
    
    sprintf(cmd, "Entry /%s/%s///\012", entry, ver);
    cvs_send(args->sock, cmd);
    
    sprintf(cmd, "Argument %s\012update\012", entry);
    cvs_send(args->sock, cmd);
    
    *resp = cvs_get_resp();
    
    return 1;
    
}

/* Send a "log" CVS request */
int cvs_get_status(vcfs_path name, char *ver, cvs_buff **resp)
{

    char cmd[1024];
    vcfs_path parent;
    vcfs_name entry;
  
    split_path(name, &parent, &entry);

    sprintf(cmd, "Argument -r%s\012", ver);
    cvs_send(args->sock, cmd);

    sprintf(cmd, "Directory .\012%s/%s\012", args->root, parent);
    cvs_send(args->sock, cmd);
    
    sprintf(cmd, "Entry /%s/%s///\012", entry, ver);
    cvs_send(args->sock, cmd);
    
    sprintf(cmd, "Argument %s\012log\012", entry);
    cvs_send(args->sock, cmd);
    
    *resp = cvs_get_resp();
    
    return 1;

}

int cvs_get_log(vcfs_path name, cvs_buff **resp)
{
    char cmd[1024];
    vcfs_path parent;
    vcfs_name entry;

    split_path(name, &parent, &entry);
    
    sprintf(cmd, "Directory .\012%s/%s\012", args->root, parent);
    cvs_send(args->sock, cmd);

    //sprintf(cmd, "Entry /%s/%s///\012", entry, ver);
    //cvs_send(args->sock, cmd);

    sprintf(cmd, "Argument %s\012log\012", entry);
    cvs_send(args->sock, cmd);
    
    *resp = cvs_get_resp();
    
    return 1;
}


/* TBD - Combine with split_path. Same functionality. */
int cvs_ver_extended(char *name, vcfs_path *short_name, vcfs_ver *ver)
{
    char *p;
    
    memset(short_name, 0, sizeof(short_name));
    memset(ver, 0, sizeof(ver));
    
    /* Look for a comma */
    p = (char *)strrchr(name, ',');
    
    if (p == NULL)
    {
        return 0;
    }
    
    strncpy(*short_name, name, (p - name));
    (*short_name)[p - name] = '\0';
    
    strncpy(*ver, p + 1, (strlen(name) - (p - name) - 1));
    (*ver)[(strlen(name) - (p - name) - 1)] = '\0';

    return 1;
    
}

/* Note: log message currently unsupported */
int cvs_get_log_info(cvs_buff *log_buff, char **ver,
                         char **date, char **author, char **msg)
{
    char *line;
    char *p, *a;
    int len;
    int n;

    while((n = cvs_buff_read_line(log_buff, &line)) > 0) 
    {
        if (!strncmp(line, "M revision", 10))
        {
            /* This line lists a version number */
            if (ver != NULL)
            {
                /* The version number is from the 2nd space to the end of the line */
                p = (char *)strchr(line, ' ');
                p = (char *)strchr(p + 1, ' ');
                a = (char *)strchr(line, '\n');
                
                /* Skip past the space */
                p++;
                
                /* Copy up to sizeof(vcfs_ver) bytes of the version */
                len = a - p;
                len = (len <= sizeof(vcfs_ver) ? len : sizeof(vcfs_ver)); 
                *ver = malloc(len);
                strncpy(*ver, p, len);
                (*ver)[len] = '\0';
            }
            
            free(line);
            
            /* The next line contains date and author information */
            cvs_buff_read_line(log_buff, &line);
            
            if (!strncmp(line, "M date:", 7))
            {
                p = (char *)strchr(line, ';');
                
                if (p != NULL && date != NULL)
                {
                    len = p - line - 8;  /* Hack to copy correct chunk */
                    *date = malloc(len + 1);
                    strncpy(*date, line + 8, len);
                    (*date)[len] = '\0';
                }
                
                a = (char *)strchr(p + 1, ';');
                
                if (a != NULL && author != NULL)
                {
                    len = a - p - 11;
                    *author = malloc(len + 1);
                    strncpy(*author, p + 11, len);
                    (*author)[len] = '\0';
                }
            }
            
            /* We found one a log entry, so return */
            free(line);
            return 1;
        }
        
    }
    
    return 0;
    
}
