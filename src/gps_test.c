/* gp_gps.c --- 
 * 
 * Filename: gp_gps.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jun 24 22:49:48 2013 (-0400)
 * Version: 
 * Last-Updated: Sat Jul  6 19:12:33 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 274
 * 
 */

/* Code: */
/* Headers */
#include <termios.h>
#include "geopebble.h"
#include "unix_sockets.h"
#include "ringbuf.h"

/* Macros */
#define BACKLOG 5

#define USYNC1 0xB5
#define USYNC2 0x62
#define CLSNAV 0x01
#define CLSRXM 0x02
#define CLSINF 0x04
#define CLSACK 0x05

#define CLSCFG 0x06
#define IDCFG  0x09 /* p106, length 12 (no non-volatile storage) or 13 with */

#define CLSMON 0x0A
#define CLSAID 0x0B
#define CLSTIM 0x0D
#define CLSESF 0x10

#define GPSBUFSIZ 1024

#define LEAPSEC 16 

/* File-scope variables */
int                      fd, fdnull;

/* External variables */
int             info, debug, standalone;
char            progname[128];
				/* command line processing */
extern int 	optind, opterr;
extern char 	*optarg;

/* External functions */

/* Structures and Unions */

// current as of Jul 2012
struct ubxpkt_st {
    char sync[2];
    char clsid[2];
    short size;
    char cksum[2];
    char payload[]; // this is a zero length element.
};

char ubxpkt[512]; // raw packet from ublox; max size of a pkt is 8+24*nsv+8

enum states {
    WAIT_FOR_HEADER,
    WAIT_FOR_CLASS,
    WAIT_FOR_SIZE,
    WAIT_FOR_PAYLOAD,
    WAIT_FOR_CKSUM,
    CHECK_CKSUM,
    INTERPRET_UBXPKT
};

/* Signal catching functions */
volatile int STOP=FALSE;
/* set STOP and return */
void sig_handler(int sig)
{
//     msglog("%d received sig %d - %s", getpid(), sig, (char *)strsignal(sig));
    // not async safe
    fprintf(stderr, "gps: %d received sig %d - %s\n", getpid(), sig, (char *)strsignal(sig));
    STOP=TRUE;
}

/* Functions */

void parse_pkt(char *raw, int len);
int turnOnRaw(int fd);
int turnOffNMEA(int fd);
int txMsg(char *msg, int len, int fd, int ntry);
int getACK(int fd, char *msg);
struct ubxpkt_st *getNextPacket(int tout);
void calculateRawUBXChecksum(unsigned char ubxPacket[], int ubxPacketLength);
void calculateUBXChecksum(struct ubxpkt_st *u);

/* Main */

int main(int argc, char *argv[]) {
    int                      num_read;
    struct termios           gpstio;

    /* ring buffer */
    ringbuf_t                gpsrb = ringbuf_new(GPSBUFSIZ-1);

    /* Signal */
    struct sigaction         act;
    /* getopt */
    char                    *optstring="dihS";
    int                      opt, dofork=1;
    /* socket */
    struct sockaddr_un       addr;
    int                      sfd, cfd, sockfd_adc;
    ssize_t                  numRead, ret;
    /* select */
    fd_set                   readfds, rfds;
    struct timeval           timeout;

    /* state machine */
    int                      state;
    char  header[2], clsid[2], cksum[2];
    short payloadlen;
    int ind;
    int ofs, ofs1;

    /* GPS data */
    struct timeval           currTime;

    debug=opterr=info=0;
    standalone=0;
    /* 
     * command-line processing
     */
    while((opt=getopt(argc,argv,optstring)) != -1) {
	switch(opt) {
	case 'S':
	    standalone++; // don't deal with shared memory - for testing
	    break;
	case 'i': // info is for informational msgs
	    info++;
	    break;
	case 'd': // debug is for detailed debugging msgs
	    debug++;
	    break;
	case 'F':
	    dofork=0;
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
    if(debug)
	printf("gps: starting...\n");

    sigemptyset(&act.sa_mask); /* nothing is blocked */
    act.sa_flags = 0;

    act.sa_handler=sig_handler;
    sigaction(SIGTERM, &act, 0);
    sigaction(SIGINT, &act, 0);
    sigaction(SIGCHLD, &act, 0);
    sigaction(SIGPIPE, &act, 0);
    sigaction(SIGUSR1, &act, 0);


    /* open /dev/null - used to copy unneeded bytes form ringbuffer */
    fdnull = open("/dev/null", O_WRONLY);
    if(fdnull < 0)
	errExit("null open");

    /* open GPS serial port */
    /* not controlling tty so CTRL-C doesn't kill it */
    do {
	fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
	if(fd < 0) {
	    gettimeofday(&currTime, NULL);
	    errMsg("no gps connected");
	}
	sleep(1);
    } while(fd < 0 && STOP==FALSE); // REPEAT THIS UNTIL WE GET A GPS PORT...??

    cfmakeraw(&gpstio);
    gpstio.c_cflag |= CLOCAL;
    cfsetispeed(&gpstio, B9600);
    cfsetospeed(&gpstio, B9600);

    gpstio.c_cc[VMIN]=14; /* return at least 14 char */
    gpstio.c_cc[VTIME]=2; /* so long as the interbyte time is < 0.2 s */
    /* else return with what is there -- at least one */

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &gpstio);

    /* to simulate cold start */
    //printf("before coldstart\n");
    //ret = coldStart();

    //    loadDefaultConfig(fd); /* CFG-CFG clear and load */
    if(info) printf("gps: before turnOffNMEA\n");
    ret = turnOffNMEA(fd);     /* CFG-MSG */
    
    char swVer[32], hwVer[32], romVer[32];
    if(info)printf("before getVersion\n");
    ret=getVersion(swVer, hwVer, romVer);
    if(info) printf("gps: before turnOnRaw\n");
    ret = turnOnRaw(fd);
    while(STOP==FALSE) {
	struct ubxpkt_st *u = getNextPacket(1);
	parsePacket(u);
	//	printf("cls id: %x %x len %d\n", (unsigned char)pkt->clsid[0], (unsigned char)pkt->clsid[1], (int)pkt->size);
    }
    //setToStationary(); /* CFG-NAV5 - ``dynModel'' */
    
}


int parsePacket(struct ubxpkt_st *u) {
    /* POSLLH */
    unsigned int iTOW, hAcc, vAcc;
    int lon, lat, height;
    /* TIMEGPS */
    // iTOW from above
    int fTOW;
    short week;
    char leapS;
    char valid;
    unsigned int tAcc;
    /* TIMEUTC */
    // iTOW and tAcc from above
    unsigned int nano;
    unsigned short year;
    unsigned char month, day, hour, min, sec;
    // valid from above;
    /* STATUS */
    unsigned char gpsFix, flags, fixStat, flags2;
    unsigned int ttff, msss;
    /* SVINFO */
    unsigned char numCh, globalFlags, ch, svid, quality, cno;
    char elev;
    short azim;
    int prRes;

    int i;
    
    switch(u->clsid[0]) {
    case 0x01: // class NAV
	switch(u->clsid[1]) {
	case 0x02: // id POSLLH
	    /* handle POSLLH payload */
	    iTOW=*(unsigned int *)(u->payload);
	    lon=*(int *)(u->payload+4);
	    lat=*(int *)(u->payload+8);
	    height=*(int *)(u->payload+12); // h above ellipsoid
	    hAcc=*(unsigned int *)(u->payload+20);
	    vAcc=*(unsigned int *)(u->payload+24);
	    if(info){
		printf("gps:got posllh packet ll: %d %d\n", lat, lon);
	    }
	    break;
	case 0x20: // id TIMEGPS
	    iTOW = *(unsigned int *)(u->payload);
	    fTOW = *(int *)(u->payload+4);
	    week = *(short *)(u->payload+8);
	    leapS = *(char *)(u->payload+10);
	    valid = *(char *)(u->payload+11);
	    tAcc = *(unsigned int *)(u->payload+12);
	    if(info)
		printf("gps: got timegps packet iTOW=%d fTOW=%d wk=%d leapS=%d valid=%d tAcc=%d\n", iTOW, fTOW, week, leapS, valid, tAcc);
	    break;
	case 0x21: // id TIMEUTC
	    iTOW = *(unsigned int *)(u->payload);
	    tAcc = *(unsigned int *)(u->payload+4);
	    nano = *(int *)(u->payload+8);
	    year = *(unsigned short *)(u->payload+12);
	    month = *(unsigned char *)(u->payload+14);
	    day = *(unsigned char *)(u->payload+15);
	    hour = *(unsigned char *)(u->payload+16);
	    min = *(unsigned char *)(u->payload+17);
	    sec = *(unsigned char *)(u->payload+18);
	    valid = *(char *)(u->payload+19);
	    if(info)
		printf("gps: got timeutc packet iTOW=%d tAcc=%d nano=%d date=%d/%d/%d %d:%d:%d valid=%d\n", iTOW, tAcc, nano, year, month, day, hour, min, sec, valid);
	    break;
	case 0x03: // STATUS
	    iTOW = *(unsigned int *)(u->payload);
	    gpsFix = *(unsigned char *)(u->payload+4);
	    flags =  *(unsigned char *)(u->payload+5);
	    fixStat =  *(unsigned char *)(u->payload+6);
	    flags2 =  *(unsigned char *)(u->payload+7);
	    ttff = *(unsigned int *)(u->payload+8);
	    msss = *(unsigned int *)(u->payload+12);
	    if(info)
		printf("gps: got status packet iTOW=%d gpsFix=%d flags=%x fixStat=%x flags2=%x ttff=%d msss=%d\n", iTOW, gpsFix, flags, fixStat, flags2, ttff, msss);
	case 0x30: // SVINFO
	    iTOW = *(unsigned int *)(u->payload);
	    numCh = *(unsigned char *)(u->payload + 4);
	    globalFlags = *(unsigned char *)(u->payload + 5);
	    for(i=0;i<numCh;i++) {
		ch= *(unsigned char *)(u->payload + 8 + 12*i);
		svid= *(unsigned char *)(u->payload + 9 + 12*i);
		flags = *(unsigned char *)(u->payload + 10 + 12*i); 
		quality = *(unsigned char *)(u->payload + 11 + 12*i);
		cno = *(unsigned char *)(u->payload + 12 + 12*i);
		elev = *(char *)(u->payload + 13 + 12*i);
		azim = *(short *)(u->payload + 14 + 12*i);
		prRes = *(int *)(u->payload + 16 + 12*i); // pseudo range res (cm)
		if(info) 
		    printf("%d %d: %d %x %x %d %d %d %d\n", numCh, ch, svid, flags, quality, cno, elev, azim, prRes);
	    }
	default:
	    break;
	}
	break;
    default:
	break;
    }



    
}

int coldStart() {
    char CFG_MSG_UBX_RST[] = {0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 
			      // 2 bytes navbbr mask, 0x0000 = hotstart
			      // 0x0001 = warmstart
			      // 0xffff = coldstart
			      // 1 byte reset type, 00=hw reset
			      // 01 = controlled sw reset
			      // 02 = sw reset (gps only)
			      // 04 = hw reset after shutdown
			      // 08 = gps stop
			      // 09 = gps start
			      0x0FF, 0x0FF, 0x00, 0x00,
			      0x00, 0x00};
    int success=0,i,n;
    calculateRawUBXChecksum(CFG_MSG_UBX_RST, sizeof(CFG_MSG_UBX_RST));
    printf(">>> ");
    for(i=0;i<sizeof(CFG_MSG_UBX_RST);i++) printf("%x ", CFG_MSG_UBX_RST[i]);
    printf("\n");

    if((n=write(fd, CFG_MSG_UBX_RST, sizeof(CFG_MSG_UBX_RST))) < 0)
	errExit("write");
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
    int success=0;
    if(!(success=txMsg(CFG_MSG_NAV_POSLLH, sizeof(CFG_MSG_NAV_POSLLH), fd, 10)))
	printf("couldn't turn on POSLLH: %d\n", success);
    if(!(success=txMsg(CFG_MSG_NAV_TIMEGPS, sizeof(CFG_MSG_NAV_TIMEGPS), fd, 10)))
	printf("couldn't turn on TIMEGPS: %d\n", success);
    if(!(success=txMsg(CFG_MSG_NAV_TIMEUTC, sizeof(CFG_MSG_NAV_TIMEUTC), fd, 10)))
	printf("couldn't turn on TIMEUTC: %d\n", success);
    if(!(success=txMsg(CFG_MSG_NAV_STATUS, sizeof(CFG_MSG_NAV_STATUS), fd, 10)))
	printf("couldn't turn on STATUS: %d\n", success);
    if(!(success=txMsg(CFG_MSG_NAV_SVINFO, sizeof(CFG_MSG_NAV_SVINFO), fd, 10)))
	printf("couldn't turn on SVINFO: %d\n", success);
    if(!(success=txMsg(CFG_MSG_RXM_RAW, sizeof(CFG_MSG_RXM_RAW), fd, 10)))
	printf("couldn't turn on RXM: %d\n", success);
    return success;
}

int getVersion(char swVer[], char hwVer[], char romVer[]) {
    /* ring buffer */
    struct ubxpkt_st *verpkt;
    int i,n;
    char buf[BUF_SIZE];
    char CFG_MSG_MON_VER[] = {0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 
			      0x00, 0x00};
    int success, ret;
    pollMsg(CFG_MSG_MON_VER, sizeof(CFG_MSG_MON_VER), fd, 1);

    verpkt = getNextPacket(5);
    //BUG BUG BUG 
    // u-blox documentation says (p 166) 
    // swver 30 bytes, hwver 10 bytes, romver 30 bytes, and then options...
    // but it appears that only swver and hwver are avail.
    printf("swver: >>%s<<\n", verpkt->payload);
    printf("hwver: >>%s<<\n", verpkt->payload+30);

    return(0);
}    

struct ubxpkt_st
*getNextPacket(int tout) {
    char           tmp[1024];
    struct timeval timeout;
    struct timeval stTime, currTime;
    fd_set         readfds, rfds;
    int            fdmax;
    int            ret, num_read, ofs, ofs1, i;
    ringbuf_t      gpsrb = ringbuf_new(GPSBUFSIZ - 1);
    char                     buf[BUF_SIZE];

    int            state=WAIT_FOR_HEADER;
    char           cls, id, cksum[2];
    short          payloadlen;

    fdmax = max(fd, fdnull);
    FD_SET(fd, &readfds);
    FD_SET(fdnull, &readfds);

    struct ubxpkt_st *pkt;

    gettimeofday(&stTime, NULL);
    if(debug)
	printf("<<< ");
    while(1) {
	timeout.tv_sec = tout;
	timeout.tv_usec = 0;
	rfds = readfds;
	ret=select(fdmax+1, &rfds, NULL, NULL, &timeout);
	
	switch(ret) {
	case 0:
	    printf("getNextPacket: timeout\n");
	    return(NULL);
	    break;
	case -1:
	    printf("getNextPacket: err\n");
	    return(NULL);
	    break;
	default:
	    if(!debug) { // don't do this check while debugging...
		gettimeofday(&currTime, NULL);
		if(currTime.tv_sec - stTime.tv_sec > tout)
		    return(NULL);
	    }
	    if(FD_ISSET(fd, &rfds)) {
		if((num_read=read(fd, buf, BUF_SIZE)) < 0)
		    errExit("read");
		if(debug) {
		    for(i=0;i<num_read;i++) printf("%x ", (unsigned char)buf[i]);
		    printf("\n");
		}
		ringbuf_memcpy_into(gpsrb, buf, num_read);

		if(state==WAIT_FOR_HEADER) {
		    //FIXME - check that ofs and ofs1 are consecutive
		    if((ofs = ringbuf_findchr(gpsrb, USYNC1, 0)) != ringbuf_bytes_used(gpsrb)) {
			if((ofs1 = ringbuf_findchr(gpsrb, USYNC2, ofs)) != ringbuf_bytes_used(gpsrb)) {
		    
			    ringbuf_write(fdnull, gpsrb, ofs+2); /* remove from 0 through header by
								  writing to  /dev/null */
			    memset(tmp, 0, sizeof(tmp));
			    state=WAIT_FOR_CLASS;
			}
		    }
		}
		if(state==WAIT_FOR_CLASS && ringbuf_bytes_used(gpsrb) > 1) { 
		    // at least 2 bytes in ringbuffer
		    ringbuf_memcpy_from(&cls, gpsrb, 1);
		    ringbuf_memcpy_from(&id, gpsrb, 1);
		    state=WAIT_FOR_SIZE;
		}
		if(state==WAIT_FOR_SIZE && ringbuf_bytes_used(gpsrb) > 1) {
		    ringbuf_memcpy_from(&payloadlen, gpsrb, 2);
		    state=WAIT_FOR_PAYLOAD;
		}
		if(state==WAIT_FOR_PAYLOAD && ringbuf_bytes_used(gpsrb) > (int)payloadlen) {
		    ringbuf_memcpy_from(tmp, gpsrb, (int)payloadlen);
		    state=WAIT_FOR_CKSUM;
		}
		if(state==WAIT_FOR_CKSUM && ringbuf_bytes_used(gpsrb) > 1) {
		    ringbuf_memcpy_from(cksum, gpsrb, 1);
		    ringbuf_memcpy_from(cksum+1, gpsrb, 1);
		    state=CHECK_CKSUM;
		}
		if(state==CHECK_CKSUM) {
		    /* populate struct.  Last element is a zero-length array */
		    pkt = (struct ubxpkt_st *)malloc(sizeof(struct ubxpkt_st) + (int)payloadlen);
		    pkt->clsid[0] = cls;
		    pkt->clsid[1] = id;
		    pkt->size = payloadlen;
		    memcpy(pkt->payload, tmp, (int) payloadlen);
		    calculateUBXChecksum(pkt);
		    if(cksum[0] == pkt->cksum[0] && cksum[1] == pkt->cksum[1])  { // proper pkt
			return(pkt);
		    } else { // cksum did not match.
			free(pkt);
			return(NULL);
		    }
		}
	    }
	}
	    
	// printf("rb:got %x %x %x %x %x %x at %d\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], ofs);
    }
}
    

int turnOffNMEA(int fd) {
    int   n, success=0;
    int   nwrt=0, doprint, i;
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

    if(!(success=txMsg(CFG_MSG_GLL, sizeof(CFG_MSG_GLL), fd, 10)))
	printf("couldn't turn off GLL: %d\n", success);
    if(!(success=txMsg(CFG_MSG_GGA, sizeof(CFG_MSG_GGA), fd, 10)))
	printf("couldn't turn off GGA\n");
    if(!(success=txMsg(CFG_MSG_GSV, sizeof(CFG_MSG_GSV), fd, 10)))
	printf("couldn't turn off GSV\n");
    if(!(success=txMsg(CFG_MSG_GSA, sizeof(CFG_MSG_GSA), fd, 10)))
	printf("couldn't turn off GSA\n");
    if(!(success=txMsg(CFG_MSG_ZDA, sizeof(CFG_MSG_ZDA), fd, 10)))
	printf("couldn't turn off ZDA\n");
    if(!(success=txMsg(CFG_MSG_RMC, sizeof(CFG_MSG_RMC), fd, 10)))
	printf("couldn't turn off RMC\n");
    if(!(success=txMsg(CFG_MSG_VTG, sizeof(CFG_MSG_VTG), fd, 10)))
	printf("couldn't turn off VTG\n");
}

/*
 * Send command msg and check for ack 
 */
int txMsg(char *msg, int len, int fd, int ntry) {
    int success=0, nwrt=0, n, i;
    calculateRawUBXChecksum(msg, len);
    if(debug) {
	printf(">>> ");
	for(i=0;i<len;i++) printf("%x ", msg[i]);
	printf("\n");
    }
    while(success<=0 && nwrt++<ntry) { // repeat
	if((n=write(fd, msg, len)) < 0)
	    errExit("write");
	success=getACK(fd, msg);
    }
}
/*
 * Poll - Send command msg and get response (no ACK/NAK)
 */
int pollMsg(char *msg, int len, int fd, int ntry) {
    int success=0, nwrt=0, n, i;
    calculateRawUBXChecksum(msg, len);
    if(debug) {
	printf(">>> ");
	for(i=0;i<len;i++) printf("%x ", msg[i]);
	printf("\n");
    }
    if((n=write(fd, msg, len)) < 0)
	errExit("write");
    return(0);
}
    

int getACK(int fd, char *msg) {
    char CFG_ACK[]={0xb5, 0x62, 0x05, 0x01, 0x02, 0x00, 0xff, 0xff, 0xff, 0xff};
    int msglen=sizeof(CFG_ACK), n=0, ackind=0;
    time_t t0, t1;
    char   c;
    struct timeval timeout;
    fd_set rdfs, readfds;
    timeout.tv_sec=1;
    timeout.tv_usec=0;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    time(&t0);  
    //              hdr,  hdr,  cls,  id,   size, size, payload..., ck,   ck
    CFG_ACK[6]=msg[2]; // payload is MSG CLS and MSG ID (bytes 2 & 3 of message)
    CFG_ACK[7]=msg[3]; // payload is MSG CLS and MSG ID (bytes 2 & 3 of message)

    calculateRawUBXChecksum(CFG_ACK, msglen);

    if(debug) {
	printf("<<< ");
	fflush(NULL);
    }

    while(ackind<10) {
	time(&t1);
	if(t1-t0>=3.)       // timeout
	    return(-1);
	switch(select(fd+1, &readfds, NULL, NULL, &timeout)) {
	case 0:
	    return(-1);
	    break;
	default:
	    n+=read(fd, &c, 1);
	    if(debug){
		printf("%x ", c);
		fflush(NULL); 
	    }
	    if(c==CFG_ACK[ackind])  // got a matching byte... 
		ackind++;           // need 10 consecutive matching...
	    else 
		ackind=0;           // reset to beginning of ack
	}
    }
    printf("\n");
    return ackind; // had to read this many bytes to success
}
    
int setToStationary() {
    return 0;
}

/*
 * Calculates the UBX checksum. The checksum is calculated over the
 * packet, starting & including the CLASS field, up until, but
 * excluding, the Checksum Field. The checksum algorithm used is the
 * 8-bit Fletcher Algorithm, which is used in the TCP standard (RFC
 * 1145).
 */
void calculateRawUBXChecksum(unsigned char ubxPacket[], int ubxPacketLength) {
  
    /*
     * Loop starts at 2, because checksum doesn't include sync chars at 0 & 1.
     * Loop stops 2 from the end, as cheksum doesn't include itself(!).
     */
    unsigned char CK_A = 0, CK_B = 0;
    int i, limit = ubxPacketLength - 2;

    for (i = 2; i < limit; i++)  {
	CK_A = CK_A + ubxPacket[i];
	CK_B = CK_B + CK_A;
    }
    
    ubxPacket[ubxPacketLength-2] = CK_A;
    ubxPacket[ubxPacketLength-1] = CK_B;
  
}


void calculateUBXChecksum(struct ubxpkt_st *u)
{
    unsigned char CK_A = 0, CK_B = 0;
    int i;
    char tmp[2];

    memcpy(tmp, u->clsid, 2);
    CK_A += tmp[0];
    CK_B += CK_A;
    CK_A += tmp[1];
    CK_B += CK_A;

    memcpy(tmp, &u->size, 2);
    CK_A += tmp[0];
    CK_B += CK_A;
    CK_A += tmp[1];
    CK_B += CK_A;

    for(i=0; i<u->size; i++) {
	CK_A += u->payload[i];
	CK_B += CK_A;
    }

    u->cksum[0] = CK_A;
    u->cksum[1] = CK_B;

    return;
}
/* gp_gps.c ends here */
