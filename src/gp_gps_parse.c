/* gp_gps_parse.c --- 
 * 
 * Filename: gp_gps_parse.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jul  8 07:00:03 2013 (-0400)
 * Version: 
 * Last-Updated: Mon Aug 12 10:18:10 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 39
 * 
 */


/* Code: */

/*
 * getNextPacket - returns a ubx packet.
 * - tout -- timeout
 *
 * reads from file-global variable fd (also needs fdnull open) FIXME?
 * bytes are copied to ring buffer, and packets found there, verified,
 * 
 * packet verification is carried out by a state machine, and ends
 * with a cksum 
 *
 * packet is  copied into ubxpkt_st (returned) returns NULL if
 * no packet found within timeout.
 */

#include <termios.h>
#include "gp_gps_parse.h"
#include "ringbuf.h"
#include "geopebble.h"

/* Signal Catching functions */
extern volatile int STOP;
/* set STOP and return */
void sig_handler(int sig)
{
//     msglog("%d received sig %d - %s", getpid(), sig, (char *)strsignal(sig));
    // not async safe
    fprintf(stderr, "gps: %d received sig %d - %s\n", getpid(), sig, (char *)strsignal(sig));
    STOP=TRUE;
}

/* File-scope local variables */

/* External variables */
extern int gpsfd; // defined in gp_gps.c
extern int debug, info;

/* File-local Functions */
int getACK(int fd, char *msg);
void calculateUBXChecksum(struct ubxpkt_st *u);
void calculateRawUBXChecksum(unsigned char ubxPacket[], int ubxPacketLength);
int getSVINFO(struct ubxpkt_st *u, struct svinfo_st *svi);
int getSTATUS(struct ubxpkt_st *u, struct status_st *stat);
int getTIMEUTC(struct ubxpkt_st *u, struct timeutc_st *t);
int getTIMEGPS(struct ubxpkt_st *u, struct timegps_st *tgps);
int getPOSLLH(struct ubxpkt_st *u, struct posllh_st *posllh);

int initGPS() {
    /* signal */
    struct sigaction         act;

    struct termios           gpstio;

    //    sigemptyset(&act.sa_mask); /* nothing is blocked */
    //    act.sa_flags = 0;

    //    act.sa_handler=sig_handler;
    //    sigaction(SIGTERM, &act, 0);
    //    sigaction(SIGINT, &act, 0);
    //    sigaction(SIGCHLD, &act, 0);
    //    sigaction(SIGPIPE, &act, 0);
    //    sigaction(SIGUSR1, &act, 0);
    //    sigaction(SIGSEGV, &act, 0);

    /* open GPS serial port */
    /* not controlling tty so CTRL-C doesn't kill it */
    printf("in initGPS\n");
    gpsfd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY);
    printf("gps fd=%d\n", gpsfd);
    if(gpsfd < 0) {
	errMsg("no gps connected");
	return(-1);
    }

    cfmakeraw(&gpstio);
    gpstio.c_cflag |= CLOCAL;
    cfsetispeed(&gpstio, B9600);
    cfsetospeed(&gpstio, B9600);

    gpstio.c_cc[VMIN]=14; /* return at least 14 char */
    gpstio.c_cc[VTIME]=2; /* so long as the interbyte time is < 0.2 s */
    /* else return with what is there -- at least one */

    tcflush(gpsfd, TCIFLUSH);
    tcsetattr(gpsfd, TCSANOW, &gpstio);
    return(0);
}

/*
 * getGPSData - read serial port (opened by initGPS, stored in `gpsfd'
 * - tout - timeout in msec
 * - buf  - buffer to store char read from gps
 * - buflen
 *
 *
 */

int getGPSData(int tout, char *buf, int buflen) {
    struct timeval timeout;
    fd_set         readfds, rfds;
    int            fdmax;
    int            ret, numRead, i;

    if(debug)
	printf("in getGPSData: gpsfd=%d\n", gpsfd);
    FD_ZERO(&readfds);
    FD_SET(gpsfd, &readfds);
    fdmax=gpsfd;

    if(tout > 1000) tout=999; // max timeout is 1s

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000 * tout;
    rfds = readfds;
    ret=select(fdmax+1, &rfds, NULL, NULL, &timeout);

    switch(ret) {
    case 0:
	//	errMsg("getGPSData: timeout\n");
	return(0);
	break;
    case -1:
	errExit("getGPSData: err\n");
	break;
    default:
	if(FD_ISSET(gpsfd, &rfds)) {
	    if((numRead=read(gpsfd, buf, buflen)) < 0)
		errExit("gps: read");
	    if(debug) {
		for(i=0;i<numRead;i++) 
		    printf("%x ", (unsigned char)buf[i]);
		printf("\n");
	    }
	    return(numRead);
	}
    } 
    return(-1);
}

/*
 * getNextPacket - parse the input buffer (along with anything passed
 * previously) and return a ubx buffer
 * - buf - chars read from GPS
 * - buflen - number of chars read
 * - reset - force a reset of the state back to init
 * 
 * return ubx packet or NULL 
 *   NOTE: the ubx packet is `malloc'-ed
 *   here...be sure to `free()' the ubx packet after use.
 *
 * Uses a ringbuffer (ringbuf) to store the incoming data.  New data
 * are append to the end of the ringbuffer and parsing begins from the
 * beginning.  Use a state machine to walk through the ubx format.
 *
 */

struct ubxpkt_st
*getNextPacket(char *buf, int *pbuflen, int *reset) {
    extern int     debug;

    char           tmp[1024];
    int            ofs, ofs1;

    char           cls, id, cksum[2];
    short          payloadlen;

    int            buflen = *pbuflen;
    // retain state between calls.
    static int               state=WAIT_FOR_HEADER;
    // retain ringbuf between calls.
    static ringbuf_t          gpsrb = NULL; 
    struct ubxpkt_st *pkt;
    
    if(!gpsrb) // create ringbuf if it doesn't exist
	if((gpsrb = ringbuf_new(GPSBUFSIZ - 1)) == 0)
	    errExit("gps: ringbuf_new");

    
    if(*reset) {
	state = WAIT_FOR_HEADER;
	ringbuf_reset(gpsrb);
	if(debug)
	    printf("doing reset bytes_used = %d\n", ringbuf_bytes_used(gpsrb));
    }

    if(buf != NULL) { 
	ringbuf_memcpy_into(gpsrb, buf, buflen);
	memset(buf, 0, buflen);
	*pbuflen=0;
    }

    /* begin state machine */
    
    if(state==WAIT_FOR_HEADER && ringbuf_bytes_used(gpsrb) > 1) {
	//FIXME - check that ofs and ofs1 are consecutive
	// ringbuf_findchr returns the index of the target or max if
	// not found
	if((ofs = ringbuf_findchr(gpsrb, USYNC1, 0)) != ringbuf_bytes_used(gpsrb)) {
	    if((ofs1 = ringbuf_findchr(gpsrb, USYNC2, ofs)) != ringbuf_bytes_used(gpsrb)) {
		    
		ringbuf_memcpy_from(tmp, gpsrb, ofs+2); /* remove from
							   0 through header */
		//		ringbuf_write(fdnull, gpsrb, ofs+2); /* 
		//					writing to  /dev/null */

		memset(tmp, 0, sizeof(tmp));
		if(debug)
		    printf("got sync\n");
		state=WAIT_FOR_CLASS;
	    }
	}
    }
    if(state==WAIT_FOR_CLASS && ringbuf_bytes_used(gpsrb) > 1) { 
	// at least 2 bytes in ringbuffer
	ringbuf_memcpy_from(&cls, gpsrb, 1);
	ringbuf_memcpy_from(&id, gpsrb, 1);
	if(debug)
	    printf("got class %x %x\n", cls, id);
	state=WAIT_FOR_SIZE;
    }
    if(state==WAIT_FOR_SIZE && ringbuf_bytes_used(gpsrb) > 1) {
	ringbuf_memcpy_from(&payloadlen, gpsrb, 2);
	if(debug)
	    printf("got len %d\n", (int)payloadlen);
	state=WAIT_FOR_PAYLOAD;
	// if I found sync pair accidentally, the payload
	// len will be garbage...
	if(payloadlen > MAXPAYLOADLEN || payloadlen < 0) {
	    state = WAIT_FOR_HEADER;
	    return(NULL);
	}
    }
    if(state==WAIT_FOR_PAYLOAD && ringbuf_bytes_used(gpsrb) > (int)payloadlen) {
	ringbuf_memcpy_from(tmp, gpsrb, (int)payloadlen);
	if(debug)
	    printf("got payload\n");
	state=WAIT_FOR_CKSUM;
    }
    if(state==WAIT_FOR_CKSUM && ringbuf_bytes_used(gpsrb) > 1) {
	ringbuf_memcpy_from(cksum, gpsrb, 1);
	ringbuf_memcpy_from(cksum+1, gpsrb, 1);
	if(debug)
	    printf("got cksum %x %x\n", cksum[0], cksum[1]);
	state=CHECK_CKSUM;
    }
    if(state==CHECK_CKSUM) {
	/* populate struct.  Last element is a zero-length array */
	if((pkt = (struct ubxpkt_st *)malloc(sizeof(struct ubxpkt_st) + (int)payloadlen)) == NULL)
	    errExit("gps: malloc");
	pkt->clsid[0] = cls;
	pkt->clsid[1] = id;
	pkt->size = payloadlen;
	memcpy(pkt->payload, tmp, (int) payloadlen);
	calculateUBXChecksum(pkt);
	if(debug)
	    printf("cksum2 %x %x\n", pkt->cksum[0], pkt->cksum[1]);

	state = WAIT_FOR_HEADER;
	*reset = state;

	if(cksum[0] == pkt->cksum[0] && cksum[1] == pkt->cksum[1])  { // proper pkt
	    if(debug)
		printf("got it!\n");
	    return(pkt);
	} else { // cksum did not match.
	    free(pkt);
	    return(NULL);
	}
    }
    
    *reset = state; // I use reset to pass the state back to
		    // the calling program AND to reset the state
		    // machine.  FIXME
    return(NULL);

}
	// printf("rb:got %x %x %x %x %x %x at %d\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], ofs);

void updateGPSStat(struct ubxpkt_st *u, struct gps_st *g) {
    struct timeutc_st timeutc;
    struct timegps_st timegps;
    struct posllh_st posllh;
    struct status_st stat;
    struct svinfo_st svi;

    if(u->clsid[0] == CLSNAV && u->clsid[1] == IDNAVTIMEUTC) {
	if(getTIMEUTC(u, &timeutc) < 0)
	    return;
	if(info)
	    printf("timeutc=%d %d %d %d:%d:%d valid=%d\n", timeutc.year, 
		   timeutc.month, 
		   timeutc.day, timeutc.hour, timeutc.min, timeutc.sec,
		   timeutc.valid);
	if(timeutc.valid & NAVTIMEUTC_VALIDUTC) {
	    struct tm tmutc;

	    tmutc.tm_year = timeutc.year - 1900;
	    tmutc.tm_mon = timeutc.month - 1;
	    tmutc.tm_mday = timeutc.day;
	    tmutc.tm_hour = timeutc.hour;
	    tmutc.tm_min = timeutc.min;
	    tmutc.tm_sec = timeutc.sec;
	    time_t tutc = mktime(&tmutc);
	    g->currUTCTime.tv_sec = tutc;
	    g->currUTCTime.tv_usec = timeutc.nano / 1000;
	    g->currUTCTimeValid =1;
	    g->tAcc = timeutc.tAcc;
	    // set computer clock - settime, adjtime?
	    //	setTime = settime(timeutc);
	    //	gettimeofday(&tt, NULL);
	    //  printf("time set to %d %d\n", setTime, tt.tv_sec);
	    if(info)
		printf("time: %ld(s) %ld(us) +/- %d(ns)\n", g->currUTCTime.tv_sec, g->currUTCTime.tv_usec, g->tAcc);
	}
    }
    if(u->clsid[0] == CLSNAV && u->clsid[1] == IDNAVTIMEGPS) {
	int gps_minus_utc = LEAPSEC; // use this unless valid below
	time_t t;
	if(getTIMEGPS(u, &timegps) < 0)
	    return;
	g->leapS = timegps.nleapS;
	g->gpsWeek = timegps.week;
	g->gpsTOW = timegps.iTOW; // ms
	g->gpsfTOW = timegps.fTOW; // ns
	g->currGPSTimeValid = 1;
	if(timegps.valid & NAVTIMEGPS_VALIDUTC) {
	    g->reportedLeapsecValid = 1;
	    gps_minus_utc = timegps.nleapS;
	}
	t = GPS_EPOCH - gps_minus_utc;
	t += 7 * 24 * 3600 * g->gpsWeek;
	t += g->gpsTOW / 1000;
	g->currGPSTime.tv_sec = t;
	g->currGPSTime.tv_usec = (int)(g->gpsfTOW * 1000);
	if(info)
	    printf("tgps: %ld week %d tow %d nleap %d valid %d\n", 
		   g->currGPSTime.tv_sec,
		   g->gpsWeek, g->gpsTOW, gps_minus_utc, 
		   g->reportedLeapsecValid);
    }

    if(u->clsid[0] == CLSNAV && u->clsid[1] == IDNAVSTATUS) {
	if(getSTATUS(u, &stat) < 0)
	    return;
	g->gpsFix = stat.gpsFix;
	if(info)
	    printf("fix %d\n", g->gpsFix);
    }

    if(u->clsid[0] == CLSNAV && u->clsid[1] == IDNAVSVINFO) {
	if(getSVINFO(u, &svi) < 0)
	    return;
	g->nSV = svi.numCh;
	if(info)
	    printf("nsv %d\n", g->nSV);
	//g->pDOP;
    }

    if(u->clsid[0] == CLSNAV && u->clsid[1] == IDNAVPOSLLH) {
	if(getPOSLLH(u, &posllh) < 0)
	    return;
	g->longitude = posllh.longitude / 1.e7;
	g->latitude = posllh.latitude / 1.e7;
	g->heightEllipsoid = posllh.height/1000.;
	g->verticalAcc = posllh.vAcc;
	g->horizontalAcc = posllh.hAcc;	
	if(info)
	    printf("pos %lf %lf %f\n", g->longitude, g->latitude, g->heightEllipsoid);
    }
    return;
}

int getPOSLLH(struct ubxpkt_st *u, struct posllh_st *posllh) {
    if(u->clsid[0] != CLSNAV || u->clsid[1] != IDNAVPOSLLH) 
	return(-1);
    posllh->iTOW=*(unsigned int *)(u->payload);
    posllh->longitude=*(int *)(u->payload+4);
    posllh->latitude=*(int *)(u->payload+8);
    posllh->height=*(int *)(u->payload+12); // h above ellipsoid
    posllh->hAcc=*(unsigned int *)(u->payload+20);
    posllh->vAcc=*(unsigned int *)(u->payload+24);
    return(0);
    //	if(info){
    //  printf("gps:got posllh packet ll: %d %d\n", lat, lon);
    //}
}

int getTIMEGPS(struct ubxpkt_st *u, struct timegps_st *tgps) {
    if(u->clsid[0] != CLSNAV || u->clsid[1] != IDNAVTIMEGPS) 
	return(-1);
    tgps->iTOW = *(unsigned int *)(u->payload);
    tgps->fTOW = *(int *)(u->payload+4);
    tgps->week = *(short *)(u->payload+8);
    tgps->nleapS = *(char *)(u->payload+10);
    tgps->valid = *(char *)(u->payload+11);
    tgps->tAcc = *(unsigned int *)(u->payload+12);
    return(0);
}

int getTIMEUTC(struct ubxpkt_st *u, struct timeutc_st *t) {
    if(u->clsid[0] != CLSNAV || u->clsid[1] != IDNAVTIMEUTC) 
	return(-1);
    
    t->iTOW = *(unsigned int *)(u->payload);
    t->tAcc = *(unsigned int *)(u->payload+4);
    t->nano = *(int *)(u->payload+8);
    t->year = *(unsigned short *)(u->payload+12);
    t->month = *(unsigned char *)(u->payload+14);
    t->day = *(unsigned char *)(u->payload+15);
    t->hour = *(unsigned char *)(u->payload+16);
    t->min = *(unsigned char *)(u->payload+17);
    t->sec = *(unsigned char *)(u->payload+18);
    t->valid = *(char *)(u->payload+19);
    if(debug)
	printf("gps: timeutc iTOW=%d tAcc=%d nano=%d date=%d/%d/%d %d:%d:%d valid=%d\n", t->iTOW, t->tAcc, t->nano, t->year, t->month, t->day, t->hour, t->min, t->sec, t->valid);
    return(0);
}

int getSTATUS(struct ubxpkt_st *u, struct status_st *stat) {
    if(u->clsid[0] != CLSNAV || u->clsid[1] != IDNAVSTATUS) 
	return(-1);
    stat->iTOW = *(unsigned int *)(u->payload);
    stat->gpsFix = *(unsigned char *)(u->payload+4);
    stat->flags =  *(unsigned char *)(u->payload+5);
    stat->fixStat =  *(unsigned char *)(u->payload+6);
    stat->flags2 =  *(unsigned char *)(u->payload+7);
    stat->ttff = *(unsigned int *)(u->payload+8);
    stat->msss = *(unsigned int *)(u->payload+12);
    return(0);
}

int getSVINFO(struct ubxpkt_st *u, struct svinfo_st *svi) {
    int i;
    if(u->clsid[0] != CLSNAV || u->clsid[1] != IDNAVSVINFO) 
	return(-1);
    svi->iTOW = *(unsigned int *)(u->payload);
    svi->numCh = *(unsigned char *)(u->payload + 4);
    svi->globalFlags = *(unsigned char *)(u->payload + 5);
    for(i=0;i<svi->numCh;i++) {
	svi->svs[i].ch= *(unsigned char *)(u->payload + 8 + 12*i);
	svi->svs[i].svid= *(unsigned char *)(u->payload + 9 + 12*i);
	svi->svs[i].flags = *(unsigned char *)(u->payload + 10 + 12*i); 
	svi->svs[i].quality = *(unsigned char *)(u->payload + 11 + 12*i);
	svi->svs[i].cno = *(unsigned char *)(u->payload + 12 + 12*i);
	svi->svs[i].elev = *(char *)(u->payload + 13 + 12*i);
	svi->svs[i].azim = *(short *)(u->payload + 14 + 12*i);
	svi->svs[i].prRes = *(int *)(u->payload + 16 + 12*i); // pseudo range
    }
    return(0);
}

/*
 * Send command msg and check for ack 
 * Arguments
 * -- `msg' to be sent
 * --  length `len'
 * --  file descriptor `gpsfd',  (this is global flag FIXME?)
 * --  number of retries 'ntry'
 * Return
 * -- 0 on success, -1 on failure.
 */
int txMsg(char *msg, int len, int fd, int ntry) {
    int nwrt=0, n;
    calculateRawUBXChecksum((unsigned char *)msg, len);

    /* reset the state machine and clear the ringbuf */
    int reset_state=1;
    int tmp;
    getNextPacket(NULL, &tmp, &reset_state);
    /* empty the serin fifo */
    tcflush(gpsfd, TCIOFLUSH);

    while(nwrt++<ntry) { // repeat
	if((n=write(fd, msg, len)) < 0)
	    errExit("gp_gps_parse: write");
	if(getACK(fd, msg))
	    continue; // failed.  try again
	else
	    return(0); // success
    }
    return(-1); // failed
}
/*
 * Poll - Send command msg and get response (no ACK/NAK)
 */
int pollMsg(char *msg, int len, int fd, int ntry) {
    int n;
    calculateRawUBXChecksum((unsigned char *)msg, len);
    if((n=write(fd, msg, len)) < 0)
	errExit("write");
    return(0);
}

/* 
 * getAck -- u-blox responds to CFG messages with a packet of class
 * ACK (0x05) and id ACK (0x01) or NAK (0x00).  In addition, the
 * payload of the packet is the class and id that was being
 * configured.
 * - fd - serial port (this is now a global? FIXME)
 * - msg - the CFG message that was sent (bytes 2 & 3 are class and id)
 *
 * Return 0 on success, 1 on NAK and -1 on neither...
 *
 */    

int getACK(int fd, char *msg) {
    struct ubxpkt_st *ackpak;
    char retcls, retid;
    char retpayload[2];
    
    char buf[BUF_SIZE];
    int numRead, reset_state=0, gotack=0, numReads=0;

    do {
	if((numRead = getGPSData(500, buf, BUF_SIZE)) == 0) // timeout
	    return(-1);
	if(debug)
	    printf("getACK: got %d bytes\n", numRead);

	/* parse all the packets in last read */
	while((ackpak = getNextPacket(buf, &numRead, &reset_state)) != NULL) {
	    if(debug)
		printf("ack loop %d\n", numReads);
	    retcls=ackpak->clsid[0];
	    retid=ackpak->clsid[1];
	    memcpy(retpayload, ackpak->payload, 2);
	    free(ackpak);
	    if(retcls == 0x05) {
		if(retid == 0x00) // NAK
		    return(1);
		if(retid == 0x01 && retpayload[0] == msg[2] && retpayload[1] == msg[3]) // ACK
		    return(0);
	    }
	}
    } while(gotack==0 && numReads++ < 10);
    return(-1);
}
    
int setToStationary() {
    return 0;
}

/*
 * Calculates the UBX checksum. The checksum is calculated over the
 * packet, starting after the two USYNC bytes upto (but excluding) the
 * checksum Field. 
 * The checksum algorithm used is the 8-bit Fletcher
 * Algorithm, which is used in the TCP standard (RFC 1145).
 *
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
    
    if(debug)
	printf("calccksum: %x %x\n", CK_A, CK_B);

    return;
}


/* gp_gps_parse.c ends here */
