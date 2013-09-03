/* gp_qc.c --- 
 * 
 * Filename: gp_qc.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Tue Jun 18 14:26:26 2013 (-0400)
 * Version: 
 * Last-Updated: Thu Aug 15 10:38:00 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 87
 * 
 */

#include "geopebble.h"
#include "unix_sockets.h"
#include "cwp.h"


#define NCHANNELS 6

int debug;

/* Signal catching functions */
volatile int STOP=FALSE;
void
sig_handler(int sig)
{
    STOP = TRUE;
}

int main(int argc, char *argv[]) {
    int opt;
    char *optstring="dhs:";

    int sfd, fdmax, ret;
    fd_set  readfds, rfds;
    struct timeval timeout;
    struct tm *gmp, *locp;

    int i, numRead;
    char buf[BUF_SIZE];
    
    struct param_st theParams;
    struct gps_st   gpsStat;

    int sampleRate = 4000;

    /* 
     * command-line processing
     */
    debug=opterr=0;
    while((opt=getopt(argc,argv,optstring)) != -1) {
	switch(opt) {
	case 's': 
	    sampleRate = atoi(optarg);
	    break;
	case 'd':
	    debug++;
	    break;
	case '?':
	case 'h':
	default:
	    fprintf(stderr, "Usage: %s\n", "gp_store TODO");
	    exit(-1);
	}
    }

    if(debug)
	printf("gp_qc: sampRate=%d\n", sampleRate);

    struct sigaction sa;
    // sigterm
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sig_handler;
    if(sigaction(SIGTERM, &sa, NULL) == -1)
	errExit("sched: sigaction");

    /* connect to the udp socket */
    int ufd;
    //    char udp_path[128];
    //    snprintf(udp_path, 128, "%s.%d", UDP_SOCK_PATH, (long) getpid())
	//    ufd = unixBind(udp_path, SOCK_DGRAM);
    ufd = unixConnect(UDP_SOCK_PATH, SOCK_DGRAM);
    if(ufd < 0)
	errMsg("gp_store: udp bind");

    float dt_in = 1. / (float)sampleRate;
    int resampleRate = 500;
    float fpasshi, fstophi, apasshi, astophi;
    int npoleshi;
    float f3dbhi;
    fpasshi = (float) resampleRate / (float) sampleRate;
    fstophi = 1.2 * fpasshi;
    apasshi = 0.95;
    astophi = 0.05;

    if(debug)
	printf("f = %f %f a =%f %f\n", fpasshi, fstophi, apasshi, astophi);
    bfdesign(fpasshi, apasshi, fstophi, astophi, &npoleshi, &f3dbhi);
    if(debug)
	printf("npoles = %d f3db = %f\n", npoleshi, f3dbhi); 

    int n=sampleRate / 10; // operate on 1 s at a time
    float *p, *q;
    if((p=(float *)malloc(n*sizeof(float))) == NULL)
	errExit("qc: malloc");
    if((q=(float *)malloc(n*sizeof(float))) == NULL)
	errExit("qc: malloc");

    int dec = sampleRate / resampleRate;
    int nDec = n / dec;
    float *r, *t, dt_out = dt_in * dec;
    if((r=(float *)malloc(nDec*sizeof(float))) == NULL)
	errExit("qc: malloc");
    if((t=(float *)malloc(nDec*sizeof(float))) == NULL)
	errExit("qc: malloc");
    for(i=0; i<nDec; i++)
	t[i] = i * dt_out;
    if(debug)
	printf("samp %d resamp %d dt %f dtout %f n %d nDec %d\n",
	       sampleRate, resampleRate, dt_in, dt_out, n, nDec);
    
    /* allocate the space for the udppkt.  The zero length array is
       allocated here */
    struct udppkt_st *u;
    int nWrt, pktSize = sizeof(struct udppkt_st) + nDec * sizeof(float);
    if((u=malloc(pktSize)) == NULL)
	errExit("qc: malloc");
    
    while(STOP == FALSE) {
	/* n samples are collected from DMA here... */
	/* copy into p */
	/* antialias filter */
	if(dec > 1) {
	    bflowpass(npoleshi, f3dbhi, n, p, q);
	    ints8r(n, dt_in, 0., q, 0., 0., nDec, t, r);
	    if(ufd>0) { // send the data to base station
		u->dt = dt_out;
		u->ns = nDec;
		// u.t0.tv_sec = ; u.t0.tv_usec =
		memcpy(u->d, r, nDec * sizeof(float));
		if((nWrt = write(ufd, u, pktSize)) != pktSize)
		    errMsg("qc: write");
	    }
	}
    }
}
