/* gp_gps.c --- 
 * 
 * Filename: gp_gps.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jun 24 22:49:48 2013 (-0400)
 * Version: 
 * Last-Updated: Thu Aug 15 08:58:54 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 314
 * 
 */

/* Code: */
/* Headers */
#include <termios.h>
#include "geopebble.h"
#include "gp_gps_parse.h"
#include "unix_sockets.h"
#include "gp_gps.h"
#include "gp_fns.h"

/* Macros */
#define BACKLOG 5

#define MAXNULLS 100 // FIXME?

/* File-scope variables */

int GPS_OK = 0;
struct gps_st   gpsStat;

/* External variables */
int             info, debug, standalone;
char            progname[128];
int             gpsfd;

/* command line processing */
extern int 	optind, opterr;
extern char 	*optarg;

/* External functions */

/* Structures and Unions */

// current as of Jul 2012

char ubxpkt[512]; // raw packet from ublox; max size of a pkt is 8+24*nsv+8


/* Signal catching functions */
volatile int STOP=FALSE;
/* Functions */

int initGPS(void);
int resetGPS(void);
int turnOnRaw(int fd);
int turnOffNMEA(int fd);
time_t settime(struct timeutc_st t);
int getVersion(char swVer[], char hwVer[], char romVer[]);
int writeGPS(int ubxfd, char *buf, int buflen);

void updateGPSStat(struct ubxpkt_st *u, struct gps_st *gpsStat);
/* Main */

int main(int argc, char *argv[]) {
    /* getopt */
    char                    *optstring="dihSs:";
    int                      opt;
    /* socket */
    ssize_t                  numRead, ret;
    char                     buf[BUF_SIZE];

    /* select */
    int                      fdmax;
    fd_set                   readfds, rfds;
    struct timeval           timeout, currComputerTime;

    /* state machine */
    int                      reset_state;

    /* GPS data */
    int                      nNull=0;  // number of times in a row I
				       // don't get a legal packet

    debug=opterr=info=0;
    standalone=0;
    /* 
     * command-line processing
     */
    while((opt=getopt(argc,argv,optstring)) != -1) {
	switch(opt) {
	case 'i':
	    info++;
	    break;
	case 'd':
	    debug++;
	    break;
	case '?':
	    break;
	case 'h':
	default:
	    fprintf(stderr, "Usage: %s\n", "geopebble TODO");
	    exit(-1);
	}
    }

    sprintf(progname, "%s-%s", argv[0], VERSION);
    //    init_slog(progname);

    if(info || debug)
	printf("gps: starting...\n");

    char ubxfile[128];
    int ubxfd;
    mkfname(ubxfile, "GPXXX", "ubx", NULL);
    if((ubxfd = open(ubxfile, (O_WRONLY | O_CREAT))) < 0)
	errExit("gps: ubxfd open");
    if(info)
	printf("ubxfile: %s %d\n", ubxfile, ubxfd);

    gpsStat.health=1;
    if(initGPS()) {
	errMsg("error in initGPS");
	GPS_OK = 0;
	gpsStat.health = 0;
    }

    /* WARNING WARNING WARNING - this does a COLD START to simulate that
     * change to warm start */
    if(gpsStat.health)
	ret = resetGPS();

    //    loadDefaultConfig(fd); /* CFG-CFG clear and load */
    if(info) printf("gps: before turnOffNMEA\n");
    if(gpsStat.health)
	ret = turnOffNMEA(gpsfd);     /* CFG-MSG */

    /* get version numbers */
    char swVer[32], hwVer[32], romVer[32];
    swVer[0] = 0;
    hwVer[0] = 0;
    romVer[0] = 0;
    if(info)printf("before getVersion\n");
    if(gpsStat.health)
	ret=getVersion(swVer, hwVer, romVer);
    if(info)
	printf("swver = %s hwVer=%s romver=%s\n", swVer, hwVer, romVer);

    if(info) printf("gps: before turnOnRaw\n");
    if(gpsStat.health)
	ret = turnOnRaw(gpsfd);

    //  setToStationary(); /* CFG-NAV5 - ``dynModel'' */

    /* open the "server" xxx_srv socket */
    /* requests on this socket are served up the gps status */
    int sfd_srv, cfd_srv;
    if(remove(GPS_SOCK_PATH) == -1 && errno != ENOENT) // remove old if still around...
	errExit("remove - %s", GPS_SOCK_PATH);
    if((sfd_srv = unixListen(GPS_SOCK_PATH, BACKLOG)) < 0)
	errExit("gps: listen");
    
    /* set up select on the sockets */
    FD_ZERO(&readfds);
    FD_SET(sfd_srv, &readfds);
    fdmax = sfd_srv;

    while(STOP == FALSE) {
	struct ubxpkt_st *u; 
	
	u = NULL;
	if(gpsStat.health) {
	    if((numRead = getGPSData(500, buf, BUF_SIZE)) == 0) // timeout
		continue;
	    if(debug)
		printf("got %d bytes\n", numRead);
	    
	    ret = writeGPS(ubxfd, buf, numRead); // write raw data to disk.
	  
	    reset_state = 0;
	    if(nNull > MAXNULLS)
		reset_state=1; // exceeded allowed consecutive failed parse attempts
	    while((u = getNextPacket(buf, &numRead, &reset_state)) != NULL){
		if(debug)
		    printf("parser state = %d\n", reset_state);
		nNull = 0;
		if(info)
		    printf("pkt %x %x\n", (unsigned char)u->clsid[0], (unsigned char)u->clsid[1]);
		/* TODO - update GPS struct here */
		updateGPSStat(u, &gpsStat);
		free(u);
	    }
	} else {
	    nNull++;
	    if(debug)
		printf("u is NULL %d\n", nNull);
	    gettimeofday(&currComputerTime, NULL);
	    gpsStat.currInternalTime = currComputerTime;
	    sleep(1);
	}
	
	/* read from socket */
	rfds = readfds; /* select will modify rfds, so reset */
	timeout.tv_sec=0;  	
	timeout.tv_usec=0;
	
	ret=select(fdmax+1, &rfds, NULL, NULL, &timeout);
	
	switch(ret) {
	case 0: // timeout - nothing to read from socket
	    break;
	case -1: /* error or interrupt/signal */
	    exit(-1);
	    break;
	default: // data available from socket
	    if(FD_ISSET(sfd_srv, &rfds)) { /* accept connection */
		char rdbuf[BUF_SIZE];
		if(info)
		    printf("got connection request\n");
		if((cfd_srv = accept(sfd_srv, 0, 0)) == -1)
		    errExit("gps: accept");
		if((ret=read(cfd_srv, rdbuf, BUF_SIZE)) < 0)
		    errExit("gps: socket read");
		/* could do something with reqeuest - for now ignore */		
		/* serve client */
		if((ret=write(cfd_srv, &gpsStat, sizeof(gpsStat))) != sizeof(gpsStat))
		    errExit("gps: partial write");
		if(close(cfd_srv) == -1)
		    errExit("gps:close");
	    }
	}
    }
    close(ubxfd);
    exit(0);
}

/*
 * settime - set time from u-blox timeutc
 *
 * Argument
 * -- struct timeutc_st t
 *
 * Return
 * -- time_t time that the clock is set to, or 0 if timeutc is not valid.
 * must be run as superuser or with time-setting capability.
 */

time_t settime(struct timeutc_st t) {
    struct tm bt; // broken down time from ublox timeutc
    time_t et; // epoch time corresponding to ublox timeutc
    struct timeval st;

    if(!(t.valid & NAVTIMEUTC_VALIDUTC)) 
	return(0);

    bt.tm_sec = (int)t.sec;
    bt.tm_min = (int)t.min;
    bt.tm_hour = (int)t.hour;
    bt.tm_mday = (int)t.day;
    bt.tm_mon = (int)t.month - 1; // u-blox month is 1-12, unix is 0-11
    bt.tm_year = (int)t.year - 1900; // unix year is years since 1900
    
    et = mktime(&bt);
    //printf("ctime = %s\n", ctime(&et));

    st.tv_sec = et;
    st.tv_usec = 0;
    settimeofday(&st, NULL);

    return et;
}

int getVersion(char swVer[], char hwVer[], char romVer[]) {
    /* ring buffer */
    struct ubxpkt_st *verpkt;
    int  numRead, reset_state;
    char buf[BUF_SIZE];
    char CFG_MSG_MON_VER[] = {0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 
			      0x00, 0x00};
    int olddebug=debug;
    debug = 1;

    if(debug)
	printf(">>>getVersion: in get Ver\n");

    sleep(2);

    /* reset the state machine and clear the ringbuf */
    reset_state=1;
    int tmp;
    getNextPacket(NULL, &tmp, &reset_state);
    tcflush(gpsfd, TCIOFLUSH);

    reset_state = 0;
    int numPolls=0, gotverpkt=0;;
    do {
	pollMsg(CFG_MSG_MON_VER, sizeof(CFG_MSG_MON_VER), gpsfd, 1);
	if(debug)
	    printf("in getver npolls=%d\n", numPolls);
    
	if((numRead = getGPSData(500, buf, BUF_SIZE)) == 0)
	    return(-1);
	if(debug)
	    printf(">>>getVer: got %d bytes\n", numRead);

	while((verpkt = getNextPacket(buf, &numRead, &reset_state)) != NULL) {
	    printf("in while: %x %x\n", verpkt->clsid[0], verpkt->clsid[1]);
	    if((verpkt->clsid[0] == CLSMON) && (verpkt->clsid[1] == IDMONVER)) {
		gotverpkt=1;
		break;
	    } else
		free(verpkt);
	}
    } while(gotverpkt==0 && numPolls++ < 10);
    if(numPolls == 10) 
	return(-1);

    printf("successful return from verpkt loop npolls = %d\n", numPolls);

    //BUG BUG BUG 
    // u-blox documentation says (p 166) 
    // swver 30 bytes, hwver 10 bytes, romver 30 bytes, and then options...
    // but it appears that only swver and hwver are avail.
    //    len=strlen(verpkt->payload);
    memcpy(swVer, verpkt->payload, 30);
    swVer[29] = 0;
    printf("swver: >>%s<<\n", swVer);

    //    len=strlen(verpkt->payload+30);
    memcpy(hwVer, verpkt->payload+30, 10);
    hwVer[9] = 0;

    printf("hwver: >>%s<<\n", hwVer);

    free(verpkt);
    
    debug = olddebug;
    return(0);
}    

int resetGPS() {
   char CFG_RST[] = {0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 
		     0x00, 0x00, 0x00, 0x00, // payload
		     0x00, 0x00}; // cksum
   // payload always starts at byte 6: header(2) + cls/id (2) + len (2)
   CFG_RST[6] = 0x00; // ff ff = cold start; 00 00 = hot start; 00 01 = warm
   CFG_RST[6+1] = 0x00; //
   CFG_RST[6+2] = 0x02; // controlled sw reset of gps (0x00 = hw reset)

   if(debug)
       printf("in resetGPS\n");
   if(txMsg(CFG_RST, sizeof(CFG_RST), gpsfd, 10)) {
       errMsg("couldn't reset with CFG_RST\n");
       return(-1);
   }
   return(0);
}


int turnOnRaw(int fd) {
    char CFG_MSG_NAV_POSLLH[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
				 0x01, 0x02,
				 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
    char CFG_MSG_NAV_TIMEGPS[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
				  0x01, 0x20,
				  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
    char CFG_MSG_NAV_TIMEUTC[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
				  0x01, 0x21,
				  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
    char CFG_MSG_NAV_STATUS[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
				 0x01, 0x03,
				 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
    char CFG_MSG_NAV_SVINFO[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
				 0x01, 0x30,
				 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
    char CFG_MSG_RXM_RAW[]     = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
				  0x02, 0x10,
				  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
    /* TODO - this turns on msg for all ports - 0=DDC, 1=uart1,
       2=uart2, 3=usb, 4=spi, 5=res */
    if(debug)
	printf("in turn on raw\n");
    if(txMsg(CFG_MSG_NAV_POSLLH, sizeof(CFG_MSG_NAV_POSLLH), fd, 10))
	printf("couldn't turn on POSLLH\n");
    if(txMsg(CFG_MSG_NAV_TIMEGPS, sizeof(CFG_MSG_NAV_TIMEGPS), fd, 10))
	printf("couldn't turn on TIMEGPS\n");;
    if(txMsg(CFG_MSG_NAV_TIMEUTC, sizeof(CFG_MSG_NAV_TIMEUTC), fd, 10))
	printf("couldn't turn on TIMEUTC\n");
    if(txMsg(CFG_MSG_NAV_STATUS, sizeof(CFG_MSG_NAV_STATUS), fd, 10))
	printf("couldn't turn on STATUS\n");
    if(txMsg(CFG_MSG_NAV_SVINFO, sizeof(CFG_MSG_NAV_SVINFO), fd, 10))
	printf("couldn't turn on SVINFO\n");
    if(txMsg(CFG_MSG_RXM_RAW, sizeof(CFG_MSG_RXM_RAW), fd, 10))
	printf("couldn't turn on RXM\n");
    return(0);
}

int turnOffNMEA(int fd) {
    // pg 52
    // GLL 0xF0 0x01
    char CFG_MSG_GLL[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
			  //hdr, hdr, cls, id, payload size(2 bytes)
			  0xF0, 0x01,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 
			  //payload...
			  0x00, 0x00};
    //ck, ck
    // GGA 0xF0 0x00
    char CFG_MSG_GGA[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
			  0xF0, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    // GSA 0xF0 0x02
    char CFG_MSG_GSA[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
			  0xF0, 0x02,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    // GSV 0xF0 0x03
    char CFG_MSG_GSV[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
			  0xF0, 0x03,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    // ZDA 0xF0 0x08
    char CFG_MSG_ZDA[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
			  0xF0, 0x08,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    // RMC 0xF0 0x04
    char CFG_MSG_RMC[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
			  0xF0, 0x04,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    // VTG 0xF0 0x05
    char CFG_MSG_VTG[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 
			  0xF0, 0x05,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};

    if(debug) printf("before gll\n");
    if(txMsg(CFG_MSG_GLL, sizeof(CFG_MSG_GLL), fd, 10))
	printf("couldn't turn off GLL\n");

    if(debug) printf("before gga\n");
    if(txMsg(CFG_MSG_GGA, sizeof(CFG_MSG_GGA), fd, 10))
	printf("couldn't turn off GGA\n");

    if(debug) printf("before gsv\n");
    if(txMsg(CFG_MSG_GSV, sizeof(CFG_MSG_GSV), fd, 10))
	printf("couldn't turn off GSV\n");

    if(debug) printf("before gsa\n");
    if(txMsg(CFG_MSG_GSA, sizeof(CFG_MSG_GSA), fd, 10))
	printf("couldn't turn off GSA\n");

    if(debug) printf("before zda\n");
    if(txMsg(CFG_MSG_ZDA, sizeof(CFG_MSG_ZDA), fd, 10))
	printf("couldn't turn off ZDA\n");

    if(debug) printf("before rmc\n");
    if(txMsg(CFG_MSG_RMC, sizeof(CFG_MSG_RMC), fd, 10))
	printf("couldn't turn off RMC\n");

    if(debug) printf("before vtg\n");
    if(txMsg(CFG_MSG_VTG, sizeof(CFG_MSG_VTG), fd, 10))
	printf("couldn't turn off VTG\n");
    return(0);
}

int writeGPS(int ubxfd, char *buf, int buflen) {
    int nwrt=-1;
    if((nwrt = write(ubxfd, buf, buflen)) != buflen)
	errMsg("gps: writeGPS");
    return(nwrt);
}


/* gp_gps.c ends here */
