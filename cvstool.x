/***********************************************
 * General cvstool defines
 ***********************************************/
const CVSTOOL_PATHLEN =   1024;
const CVSTOOL_NAMELEN =   255;
const CVSTOOL_MAXENT  =   256;
const CVSTOOL_MAXVER  =   64;
const CVSTOOL_VERLEN  =   16;
const CVSTOOL_DATELEN =   32;
const CVSTOOL_TAGLEN  =   32;

/***********************************************
 * Status codes for responses
 ***********************************************/
enum cvstool_status {
    CVSTOOL_OK = 0,
    CVSTOOL_NOENT = 1,
    CVSTOOL_NOEOF = 2,
    CVSTOOL_CVSERR = 3,
    CVSTOOL_OPT_MISMATCH = 4,
    CVSTOOL_FTYPE_MISMATCH = 5,
    CVSTOOL_NOVER = 6,
    CVSTOOL_NOTAG = 7
};
    
/***********************************************
 * Basic datatypes
 ***********************************************/
typedef string cvstool_path<CVSTOOL_PATHLEN>;
typedef string cvstool_name<CVSTOOL_NAMELEN>;
typedef string cvstool_ver<CVSTOOL_VERLEN>;
typedef string cvstool_date<CVSTOOL_DATELEN>;
typedef string cvstool_tag<CVSTOOL_TAGLEN>;

/*
 * Options for the 'ls' command
 */
const CVSTOOL_LS_LONG      =   0x00000001;
const CVSTOOL_LS_DIRECTORY =   0x00000002;

struct cvstool_ls_args {
    cvstool_path path;
    unsigned int options;
    int num_resp;
};

struct cvstool_ver_info {
    cvstool_ver ver;
    cvstool_name author;
    cvstool_date date;
    cvstool_tag tag;
    cvstool_ver_info *next;
};
    
struct cvstool_dirent {
    cvstool_name name;
    cvstool_ver_info ver_info;
    cvstool_dirent *next;
};

struct cvstool_ls_resp {
    int num_resp;
    cvstool_dirent *dirents;
    cvstool_status status;
    int eof;
};

/*
 * Options for the 'lsver' command
 */
const CVSTOOL_LSVER_LONG = 0x00000001;
const CVSTOOL_LSVER_FROM = 0x00000002;
const CVSTOOL_LSVER_PRE  = 0x00000004;

struct cvstool_lsver_args {
    cvstool_path path;
    cvstool_ver from_ver;
    unsigned int options;
    int num_resp;
};

struct cvstool_lsver_resp {
    int num_resp;
    cvstool_ver_info *vers;
    cvstool_status status;
};

/* 
 * Options for the 'update command 
 */
const CVSTOOL_UPDATE_DYNAMIC = 0x00000001;
const CVSTOOL_UPDATE_VER     = 0x00000002;
const CVSTOOL_UPDATE_TAG     = 0x00000004;

struct cvstool_update_args {
    cvstool_path path;
    cvstool_ver ver;
    cvstool_tag tag;
    unsigned int options;
}; 

struct cvstool_update_resp {
    cvstool_status status;
};

program CVSTOOL_PROGRAM {
    version CVSTOOL_VERSION {
        cvstool_ls_resp     CVSTOOL_LS(cvstool_ls_args) = 1;
        cvstool_lsver_resp  CVSTOOL_LSVER(cvstool_lsver_args) = 2;
        cvstool_update_resp CVSTOOL_UPDATE(cvstool_update_args) = 3;
    } = 1;
} = 0x3315563;
