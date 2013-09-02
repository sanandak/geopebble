/* 
 * mkfname - make the filename to write to.
 * 
 * args    - char *nm - prefix
 *           char *ext - the extension
 *
 * return  - the filename of form PREFIX.yyyy.mm.dd.hh.mm.ss.NNN.ext
 *
 * NNN is a unique file number
 */

#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* look to see if fname exists in THIS dir*/
int exists(char *fname)
{
     struct stat stbuf;
     if(stat(fname, &stbuf) < 0) { /* doesn't exists */
	  return(0);
     }
     return(1);
}

/* mkfname: make filename from prefix nm, extension ext; if t==Null,
   use current time, else use t.  Uniquify. */

void mkfname(char *fname, char *nm, char *ext, struct timeval *t)
{
    time_t tt = (t == NULL) ? time(NULL) : t->tv_sec;
    struct tm *tm = gmtime(&tt);
	
    int num=0;
    do {
	sprintf(fname, "%s.%04d.%02d.%02d.%02d.%02d.%02d.%01d.%s", 
		nm, tm->tm_year+1900, tm->tm_mon, tm->tm_mday, tm->tm_hour,
		tm->tm_min, tm->tm_sec, num, ext);
    } while(exists(fname) && num++ < 10);
    return;
}





