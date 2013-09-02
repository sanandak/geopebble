/* gp_store.c --- 
 * 
 * Filename: gp_store.c
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

/* Headers */
#include "geopebble.h"
#include "gp_params.h"

/* Macros */
/* number of channels.  Order is ch3, ch2, ch1, ch0, cnt_pps, cnt_drydout_n */
#define NCHANNELS 6

/* File-scope variables */

/* shared memory */
struct shm_adc_st *shm_adc;
struct gps_st *shm_gps;

int debug;
				/* command line processing */
extern int 	optind, opterr;
extern char 	*optarg;

/* External variables */

/* External functions */

/* Structures and Unions */

struct trig_st theTrig;

/* Signal catching functions */
volatile int STOP=FALSE;
void
sig_handler(int sig)
{
    STOP = TRUE;
}

/* hardware interrupt from DRYDOUT_N */
int rdata[4];
static volatile sig_atomic_t gotSigio = 0;
void
int_handler()
{
    /* on interrupt, clock in 4 24bit samples */
    /* rdata[0..3] */
    gotSigio = 1;
}

/* Functions */

int main(int argc, char *argv[]) {
    /* child PID */
    int                      storepid;
    /* socket */
    struct sockaddr_un       addr;
    int                      sfd=STDIN_FILENO;
    ssize_t                  numRead, ret;
    char                     buf[BUF_SIZE];
    /* select */
    int                      fdmax;
    fd_set                   readfds, rfds;
    struct timeval           timeout;
    /* getopt */
    char                    *optstring="dhs:F";
    int                      opt;
    /* file/trace info */
    char   *tmpNam="/tmp/geopebble_XXXXXX";
    int     ofd;
    int                      noFile=0; // default is to write a file
    int                      triggerSecs=60;
    int                      recLen=5;
    int                      nch = 5; // FIXME
    int                      datalen, trlen;
    int                      sampRate, nDataPerRow, ind;
    int                      nBytesPerSec, nBytesToCopy;
    int                     *d0, *d1, *d2, *d3, *d4;

    /* TESTING */
    struct timeval startTime; // tv_sec and tv_usec
    struct timeval currTime, ppsTime;
    struct timespec request, remain; // tv_sec and tv_nsec
    struct tm      *startTime_tm; // broken down time
    int    nanoRet;
    int    isTriggered=0;
    int    tail=0, head=0, trigRow=0, currRow, prevRow;

    /* reading from shared memory */
    int                      latestRow;

    printf("started gp_store\n");

    debug=opterr=0;
    /* 
     * command-line processing
     */
    while((opt=getopt(argc,argv,optstring)) != -1) {
	switch(opt) {
	case 'F':
	    noFile++;
	    break;
	case 'd':
	    debug++;
	    break;
	case 's': /* socketpair from parent */
	    sfd = atoi(optarg);
	    break;
	case '?':
	case 'h':
	default:
	    fprintf(stderr, "Usage: %s\n", "gp_store TODO");
	    exit(-1);
	}
    }
    if(debug) {
	printf("gp_store: sfd = %d reclen=%d at round %d secs\n", sfd, recLen, triggerSecs); 
    }

    /* bind to the udp socket */
    int ufd;
    ufd = unixBind(UDP_SOCK_PATH, SOCK_DGRAM);
    if(ufd < 0)
	errMsg("gp_store: udp bind");

    // get the param struct from gp_sched 
    if((numRead = read(sfd, buf, BUF_SIZE)) < 0)
	errExit("gp_store");
    if(debug)
	printf("gp_store: from gp_sched: num=%d\n", numRead);
    memcpy(&theTrig, buf, sizeof(struct trig_st));
    if(debug)
	printf("got param: trigt = %ld samp=%d\n", theTrig.trigTime.tv_sec, theTrig.theParams.sampleRate); 

    int sampleRate = theTrig.theParams.sampleRate;
    int recordLength = theTrig.theParams.recordLength; 
    int ns, nDMAlen, nDMAcurr;
    ns = sampleRate * recordLength;
    nDMAlen = ns * NCHANNELS;

    // nbytes per sec is 1 second worth of data.
    // 
    nBytesPerSec = sampleRate * sizeof(int);

    /* dma will populate here */
    d0 = (int *)malloc(nDMAlen);
    if(d0 == NULL)
	errExit("store:malloc d0");
    /* dma next is here - NEEDED? */
    d1 = (int *)malloc(ns * NCHANNELS * sizeof(int));
    if(d1 == NULL)
	errExit("store:malloc d1");

    /* 
     * Will the data fit in one legal su file or must be broken into multiple?
     */
    if(ns > SU_NFLTS) { /* max allowed by seg-y: 2^16 */
	errMsg("store: ns>SU_NFLTS"); // FIXME
    }

    float *c0, *c1, *c2, *c3;
    unsigned int tsamp;
    struct timeval trigTime, endTime;
    trigTime = theTrig.trigTime;
    endTime = theTrig.endTime;
    if(trigTime == NULL && endTime != NULL) {
	/* only end time specified */
    }

    c0 = (float *)malloc(ns * sizeof(float));
    if(c0 == NULL)
	errExit("malloc: c0");
    c1 = (float *)malloc(ns * sizeof(float));
    if(c1 == NULL)
	errExit("malloc: c1");
    c2 = (float *)malloc(ns * sizeof(float));
    if(c2 == NULL)
	errExit("malloc: c2");
    c3 = (float *)malloc(ns * sizeof(float));
    if(c3 == NULL)
	errExit("malloc: c3");
    tpps = (unsigned int *)malloc(ns * sizeof(unsigned int)); // cnt_pps
    if(tpps == NULL)
	errExit("malloc: tpps");
    tsamp = (unsigned int *)malloc(ns * sizeof(unsigned int)); // cnt_drydout
    if(tsamp == NULL)
	errExit("malloc: tsamp");

    /* here is where we tell the ADC to start at the next pps
     * and give it the number of samples, etc.
     */

    startADC();

    ind=ns;
    
    int nsAvail, i, sampNo=0, oldtPPS=1e10;
    struct udppkt_st udp;
    while(ind > 0) {
	struct timespec slp = {0, 1e8}; // sleep for 100 ms (1e8 nanosec);
	gettimeofday(&currTime, NULL);
	if(currTime.tv_sec < trigTime.tv_sec) {
	    nanosleep(&slp, NULL);
	    continue;
	}

	// copy from DMA to trace data.
	// FIXME can DMA_ns() return > ind?
	if((nsAvail = ind - DMA_ns()) > 0) {
	    for(i=0; i< nsAvail; i++) {
		c3[sampNo] = (float) d0[sampNo+0];
		c2[sampNo] = (float) d0[sampNo+1];
		c1[sampNo] = (float) d0[sampNo+2];
		c0[sampNo] = (float) d0[sampNo+3];
		tpps[sampNo] = (unsigned int) d0[sampNo+4];
		if(sampNo > 0 && tpps[sampNo] < tpps[sampNo-1]) { // new second...
		    gettimeofday(&ppsTime, NULL); // ignore tv_usec
		}
		tsamp[sampNo] = (unsigned int) d0[sampNo+5];
		sampNo++;
		ind--;
	    }
	}
	if(sampNo % TXSAMPS == 0) { // write udp data
	    int s0=sampNo-TXSAMPS;
	    int nwrt;
	    udp.ch = 0;
	    udp.tPPS = ppsTime.tv_sec;
	    udp.cntAtSamp0 = tsamp[s0];
	    memcpy(udp.d, c0+s0, TXSAMPS * sizeof(float));
	    //	    for(i=0;i<TXSAMPS;i++)
	    //		udp.d[i] = c0[s0 + i];
	    if((nwrt = write(ufd, &udp, sizeof(struct udppkt_st))) != sizeof(struct udppkt_st))
		errMsg("gp_store: udp partial write");
	}
    }
    printf("read %d bytes from DMA\n", sampNo);
   
    if(noFile)
	exit(0);
    /* write out data */

} /* main */

/*
 * initADC
 * set the number of samples to collect and the address of the DMA
 */

int DMA_NS;

void initADC(int *dma, int ns) {
    /* first, clear the DMA interrupt - write to clear_interrupt register */
    /* next, set up number of samps to read */
    /* set next_number-of-samples to zero so transfer will stop */
    DMA_NS = ns;
}

/*
 * startADC
 */

void startADC(void) {
    /* write to DMA address to start acq at PPS */
}

/*
 * read register DMA_number_of_samples
 * - this register counts down from the requested number of samples to zero.
 */

int DMA_ns(void) {
    /* TESTING - decrement DMA_NS by 10 each time called */
    DMA_NS -= 10;
    if(DMA_NS < 0)
	DMA_NS = 0;
    return DMA_NS;
}


/* gp_store.c ends here */
