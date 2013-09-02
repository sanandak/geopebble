#define DATADIR "."
#define LSBUFSIZE 1024*1024 /* FIXME - result of ls */
#include "geopebble.h"

#include <dirent.h>
#include <ftw.h>
#include "cJSON.h"

extern int debug;

/* free it in the calling program */
char *ans; /* malloc, populate and return this */
int  ansind; /* index into the buffer that handle_nftw uses */
int  nent; /* number of entries in the dirwalk */

/* populate the `ans' buffer with an ls-like entry for each file in DATADIR
 * filename, size, and modification time
 */
int handle_nftw(const char *fpath, const struct stat *sb, int tflag, 
		struct FTW *ftwbuf) {
    char mtime[128], sent[1024];
    int sentLen;

    extern int nent;
    extern int ansind;
    extern char *ans;

    strftime(mtime, 128, "%FT%T", gmtime(&sb->st_mtime));
    sentLen = snprintf(sent, 1024, "%-3s %7d %s %s\n", 
		       (tflag == FTW_D) ? "d" : (tflag == FTW_F) ? "f" : "???",
		       (int) sb->st_size, mtime, fpath);
    memcpy(ans+ansind, sent, sentLen);
    ansind+=sentLen;
    nent++;
    return(0);
}

/* 
 * use nftw (file tree walk) to get all the files in `dir'
 * return -1 on error, 0 if OK
 * malloc and populates `ans' with rows like:
 *
 *    <D|F> FILESIZE MODIFYTIME FILENAME
 *
 * where D if directory, F if regular file
 * MODIFYTIME is in YYYY-MM-DDTHH:MM:SS format.
 * FILENAME is relative to DATADIR
 *
 * modifies *len to be length of `ans'
 */
int run_ls(char dir[], int *len) {
    extern int nent;
    extern int ansind;
    extern char *ans;

    nent = ansind = 0;
    /* ans is an extern */
    if((ans = (char *)malloc(LSBUFSIZE)) == NULL)
	errExit("malloc");
    bzero(ans, LSBUFSIZE);
    if(nftw(DATADIR, handle_nftw, 20, 0) == -1) {
	errExit("nftw");
	return(-1);
    }
    *len = ansind;
    return(0);
}

/* 
 * parse cpcmd - should be of form `cp <filename>'
 * return -1 on error, 0 if OK
 * malloc and populates `ans' with contents of <filename>
 * modifies *len to be length of `ans'
 */
int run_cp(char *cpcmd, int *len) {
    char fname[128];
    struct stat sb;
    int fd, nread;
    sscanf(cpcmd, "cp %s", fname);
    if(debug)
	printf("copying %s\n", fname);
    if(lstat(fname, &sb) < 0) 
	return(-1);
    if((fd = open(fname, O_RDONLY)) < 0) {
	errMsg("run_cp: open");
	return(-1);
    }
    if((ans = malloc(sb.st_size)) == NULL)
	return(-1);
    if((nread = read(fd, ans, sb.st_size)) != sb.st_size) {
	errMsg("run_cp: read");
	return(-1);
    }
    *len = nread;
    return(0);
}
       

char *run_cmd(cJSON *root, int *retlen) {
    cJSON *obj;
    int fd, fp;

    if((obj = cJSON_GetObjectItem(root, "command")) == NULL)
	return(NULL);
    
    if(obj->type != cJSON_String) 
	return(NULL);
    
    if(debug)
	printf("command: %s\n", obj->valuestring);

    char *retbuf;
    if(strncmp(obj->valuestring, "ls", 2) == 0) {
	int l;
	int e = run_ls(DATADIR, &l);
	*retlen = l;
	return(ans);
    }

    if(strncmp(obj->valuestring, "cp", 2) == 0) {
	int l;
	int e = run_cp(obj->valuestring, &l);
	return(ans);
    }
}

    /* old school - use opendir/readdir/lstat */
    /* if(strcmp(obj->valuestring, "ls") == 0) { */
    /* 	DIR *d = opendir(DATADIR); */
    /* 	struct dirent *fb; */
    /* 	struct stat sb; */
    /* 	int nf=0; */
    /* 	char tmp[1024]; */

    /* 	if(d==NULL) { */
    /* 	    errMsg("opendir"); */
    /* 	    return(-1); */
    /* 	} */

    /* 	while((fb=readdir(d)) != NULL) { */
    /* 	    char fname[1024]; */
    /* 	    char mtime[128]; */

    /* 	    /\* ignore these two *\/ */
    /* 	    /\* FIXME - also ignore directories? walk dir? *\/ */
    /* 	    if(strcmp(fb->d_name, ".") || strcmp(fb->d_name, "..")) */
    /* 		continue; */

    /* 	    sprintf(fname, "%s/%s", DATADIR, fb->d_name); */
    /* 	    nf++; */
    /* 	    if(lstat(fname, &sb) == -1) { */
    /* 		errMsg("lstat"); */
    /* 		return(-1); */
    /* 	    } */
    /* 	    strftime(mtime, 128, "%FT%T", gmtime(&sb.st_mtime)); */
    /* 	    sprintf(tmp, "%d %s %s\n", (int)sb.st_size, mtime, fb->d_name); */
    /* 	    if(debug) { */
    /* 		printf("%s\n", tmp); */
    /* 	    } */
    /* 	} */
    /* } */


