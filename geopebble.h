/* geopebble.h --- 
 * 
 * Filename: geopebble.h
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Tue Jun 18 14:27:57 2013 (-0400)
 * Version: 
 * Last-Updated: Wed Aug 14 17:13:41 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 56
 * URL: 
 * Doc URL: 
 * Keywords: 
 * Compatibility: 
 * 
 */

/* Code: */

#ifndef GEOPEBBLE_H
#define GEOPEBBLE_H // Prevent double inclusion

#include <sys/types.h>  /* Type definitions used by many programs */

#include <stdio.h>      /* Standard I/O functions */
#include <stdlib.h>     /* Prototypes of commonly used library functions,
                           plus EXIT_SUCCESS and EXIT_FAILURE constants */
#include <unistd.h>     /* Prototypes for many system calls */
#include <errno.h>      /* Declares errno and defines error constants */
#include <string.h>     /* Commonly used string-handling functions */



#include <fcntl.h>		/* header to manipulate file desc */
#include <limits.h>		/* header for system number sizes */
#include <signal.h>		/* header for signal functions */

#include <time.h>		/* header for ctime() */

#include <sys/wait.h>

#include <sys/un.h>
#include <sys/socket.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include <math.h>
// #include <netinet/in.h>		/* header for htons() */
#include <sys/time.h>		/* header for timeval struct */

#include "version.h"

#include "get_num.h"    /* Declares our functions for handling numeric
                           arguments (getInt(), getLong()) */

#include "error_functions.h"  /* Declares our error-handling functions */

#include "gp_params.h" /* Defines param_st */

extern char *strsignal(int); // NEEDED? FIXME
/* max # of args for exec call */
#define MAXARG             20

#define BUF_SIZE     1024

#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80

#define SU_NFLTS 65536

/* shared memory from GPS */
#define GPS_SOCK_PATH "gpsServer"
#define BASE_SOCK_PATH "baseServer"
#define UDP_SOCK_PATH "udpServer"
#define SHMNAME_GPS "/shmGPS"
struct gps_st {
    int health; /* 0=NO GPS, 1=OK */
    /* internal clock time */
    struct timeval currInternalTime;
    /* time from GPS */
    int            currUTCTimeValid; // 1 for valid
    struct timeval currUTCTime;
    int            currGPSTimeValid;
    struct timeval currGPSTime; // parsed time - valid at last PPS (no leap sec applied)
    int            reportedLeapsecMinusDefinedLeapsec;
    int            gpsWeek;
    int            gpsTOW;
    int            gpsfTOW; // fractions (ns) tow
    int            leapS; // as reported by ublox
    // as it says... should be zero.  Needed because UTC time is as
    // much as 16s off GPS time (which we get quickly) but
    // reportedLeapsec could take upto 12 minutes to get.
    // during that time, use the defined leap sec, and record the diff (should go from 16 to zero).
    // if it goes to +/-1 then they added/sub a leapsec and this code will have to change
    // should this be a user param?
    char           reportedLeapsecValid; // 0 = no, 16 or whatever is value and yes.
    unsigned int   tAcc;        // accuracy in GPStime (ns)
    
    /* posllh */
    unsigned int   posllhiTOW; 
    float          longitude; // deg
    float          latitude;  // deg
    float          psEast;    // m polar stereo
    float          psNorth;   // m polar stereo
    float          heightEllipsoid;      // m
    float          verticalAcc;      // m (vertical accuracy)
    float          horizontalAcc;      // m (horizontal accuracy)
    
    /* sol */
    unsigned char  gpsFix;    // 0=no 1=deadRec 2=2D 3=3D 4=GPS+deadRec 5=time 
    unsigned char  nSV;
    float          pDOP;
};

struct trig_st {
    struct timeval trigTime; /* start recording here; 0=now */
    struct timeval endTime; /* stop recording here; 0=never */
    struct param_st theParams;
};

/* transmit a UDP packet every TXSAMPS samps */
#define TXSAMPS 250 
struct udppkt_st {
    int ch;
    time_t tPPS;
    unsigned int cntAtSamp0;
    float d[TXSAMPS];
};


/* shared memory from ADC */
#define SHMNAME_ADC "/shmADC"
extern const char shmname_adc[]; // = "/shmADC";
#define SHM_NROWS 100
#define SHM_NDATA 1000 /* 1000 samps = 0.1 s at 10ksamps */
struct shm_adc_st {
    int nData;   /* number of samples populated in each row */
    int sampRate; /* float? */
    // currentRow increases monotonically from 0, so must mod SHM_NROWS
    // to get actual row.  (data wrap around)
    int currentRow; /* writing to data[currentRow][0..nData] */
    int data0[SHM_NROWS][SHM_NDATA]; // ch0
    int data1[SHM_NROWS][SHM_NDATA]; // ch1
    int data2[SHM_NROWS][SHM_NDATA]; // ch2
    int data3[SHM_NROWS][SHM_NDATA]; // ch3
    int data4[SHM_NROWS][SHM_NDATA]; // adc_drydout_count
    struct timeval GPSTimeAtPPS[SHM_NROWS]; 
    struct timeval UTCTimeAtPPS[SHM_NROWS]; 
    // time for first samp in each row
    // this will only change every 10th row?
    //    struct shm_gps_st gpsdata[SHM_NROWS]; // FIXME - should I just include
    // all the gps data for each row?

    //    int gpsPPScnt[SHM_NROWS]; // the gps PPS count for the first samp
			      // in each row
};

/* semaphores for ADC shared memory */
#define SEM_NAME_ADC "/semADC"

/* Unfortunately some UNIX implementations define FALSE and TRUE -
   here we'll undefine them */

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

typedef enum { FALSE, TRUE } Boolean;


#define ONEK                    1024
#define FOURK                   (4 * ONEK)
#define MEG                     (ONEK * ONEK)	/* 1 meg of RAM */

#define min(m,n) ((m) < (n) ? (m) : (n))
#define max(m,n) ((m) > (n) ? (m) : (n))

/* function prototypes */
int get_gpsStatus(struct gps_st *gpsStat); 

#endif


/* geopebble.h ends here */
