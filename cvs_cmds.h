#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "vcfs.h"

#define CVSPORT 2401
#define CVS_BUFF_SIZE 32768

typedef struct cvs_buff {
    char *data;
    int size;
    int cookie;
    int limit;
} cvs_buff;

typedef struct cvs_args {
    char *module;
    char *hostname;
    char *root;
    char *user;
    char *password;
    char *dir;
    int sock;
} cvs_args;

#define CVS_READ_SIZE 8192

/* Cvs buffer management */
cvs_buff *cvs_get_buff();
void cvs_free_buff(cvs_buff *b);
void cvs_ensure_buff(cvs_buff *b, int n);

void cvs_init(char *hostname, char *root, char *module, char *user,
              char *password, char *dir);
int cvs_pserver_connect();
int cvs_send(int sock, char *msg);
int cvs_expand_modules(cvs_buff **resp);
int cvs_co(cvs_buff **resp);
int cvs_buff_read_line(cvs_buff *b, char **line);
int cvs_ver_extended(char *name, vcfs_path *short_name, vcfs_ver *ver);
int cvs_get_file(vcfs_path name, char *ver, cvs_buff **resp);
int cvs_get_status(vcfs_path name, char *ver, cvs_buff **resp);
int cvs_get_log(vcfs_path name, cvs_buff **resp);
int cvs_get_log_info(cvs_buff *log_buff, char **ver,
                     char **date, char **author, char **msg);
