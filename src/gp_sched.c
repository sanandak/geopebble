/* gp_scheduler.c --- 
 * 
 * Filename: gp_scheduler.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Tue Jun 18 14:26:26 2013 (-0400)
 * Version: 
 * Last-Updated: Thu Aug 15 13:22:56 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 162
 * 
 */

/* Headers */
#include "geopebble.h"
#include "error_functions.h"
#include "cJSON.h"
#include "gp_params.h"

/* Macros */
#define MAXQUERY 10

/* File-scope variables */

int debug;
/* command line processing */

/* External variables */

/* External functions */

/* Structures and Unions */

/* Signal catching functions */
volatile int STOP=FALSE;
void
sig_handler(int sig)
{
    STOP = TRUE;
}

/* timer expired */
static volatile sig_atomic_t gotAlarm = 0;
static void sigalrmHandler()
{
    gotAlarm = 1;
}
/* child exited - reap */
static void sigchldHandler()
{
    while(waitpid(-1, NULL, WNOHANG) > 0)
	continue;
}

/* Functions */
/*
 * timeToTrig - calculate the time to the nth trigger after the current time
 * - theParams has the triggering type, etc
 * - gpsStat has the gps validity (needed?)
 * - trigTimep is modified to contain the trigger time for...
 * - nth - is the nth trigger to calculate time for(0=next, 1=after, etc)
 * 
 * return dt, which is seconds to nth triggger or -1 if no valid trigger
 *
 */
int timeToTrig(struct param_st pm, struct gps_st gpsStat, struct timeval *trigTimep, int nth) {
    struct tm tm;
    struct tm trigtm;
    int    mv, dmv, dt;
    struct timeval currTime;
    time_t tt;

    gettimeofday(&currTime, NULL);
    gmtime_r(&currTime.tv_sec, &tm);
    gmtime_r(&currTime.tv_sec, &trigtm);
    tm.tm_isdst = 0;
    trigtm.tm_isdst = 0;


    if(pm.triggerType == MODULO) {
	// check that we are inside our window of valid triggering times.
	if(tm.tm_hour < pm.enableHour || tm.tm_hour > pm.enableHour+pm.enableLength)
	    return(-1);

	mv = pm.triggerModuloValue;
	// note that mktime handles any illegal seconds/minutes/hrs
	// so if minutes goes to 60 (outside the 0-59 range) mktime fixes it.
	// seconds to next round mv secs
	if(pm.triggerModuloUnits==SECONDS) {
	    dmv = mv - tm.tm_sec % mv;
	    trigtm.tm_sec += dmv + mv*nth;
	    // trigger time from broken down time to epoch time
	    tt = mktime(&trigtm);
	    dt = tt-currTime.tv_sec;
	}
	// ... to next round mv min
	if(pm.triggerModuloUnits==MINUTES) {
	    dmv = mv - tm.tm_min % mv;
	    trigtm.tm_min += dmv + mv*nth; // set to correct minute...
	    trigtm.tm_sec = 0;    // and set secs to zero
	    tt = mktime(&trigtm);
	    dt = tt-currTime.tv_sec;
	}
	// ... to next round mv hours
	if(pm.triggerModuloUnits==HOURS) {
	    dmv = mv - tm.tm_hour % mv;
	    trigtm.tm_hour += dmv + mv*nth; // set to correct hour...
	    trigtm.tm_min = 0;     // with zero sec & min
	    trigtm.tm_sec = 0;
	    tt = mktime(&trigtm);
	    dt = tt-currTime.tv_sec;
	}

	if(debug) {
	    char str[100];
	    asctime_r(&tm, str);
	    printf("tm: %s\n", str);
	    asctime_r(&trigtm, str);
	    printf("trigtm: %s\n", str);
	    printf("tm: %d %d %d %d trigtm %d %d %d %d\n", tm.tm_sec, tm.tm_min,
		   tm.tm_hour, tm.tm_isdst, trigtm.tm_sec, trigtm.tm_min, 
		   trigtm.tm_hour, trigtm.tm_isdst);
	    printf("dmv = %d mv=%d tt=%ld dt=%d\n", dmv, mv, tt, dt);
	}
	
	// set time of next trigger and return seconds to next trigger
	trigTimep->tv_sec=tt;
	trigTimep->tv_usec=0;
	return(dt);
    } else if(pm.triggerType == ABSOLUTE) {
	return(-1);
    } else if(pm.triggerType == CONTINUOUS) {
	return(-1);
    } else if(pm.triggerType == IMMEDIATE) {
	return(-1);
    }
    return(-1);
}

int run_gp_store(int mainc, char *mainv[], int *sockfd);

int main(int argc, char *argv[]) {
    int opt;
    char *optstring="dhs:";

    int sfd, fdmax, ret;
    fd_set  readfds, rfds;
    struct timeval timeout;
    struct tm *gmp, *locp;

    int numRead;
    char buf[BUF_SIZE];
    
    struct param_st theParams;
    struct gps_st   gpsStat;

    /* 
     * command-line processing
     */
    debug=opterr=0;
    while((opt=getopt(argc,argv,optstring)) != -1) {
	switch(opt) {
	case 's': /* socketpair from parent */
	    sfd = atoi(optarg);
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
	printf("gp_sched: sfd=%d\n", sfd);

    struct sigaction sa;
    // timer
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigalrmHandler;
    if(sigaction(SIGALRM, &sa, NULL) == -1)
	errExit("sched: sigaction");
    // reap children
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigchldHandler;
    if(sigaction(SIGCHLD, &sa, NULL) == -1)
	errExit("sched: sigaction");

    // get the param struct from geopebble... this can be modified, so
    // read again below
    if((numRead = read(sfd, buf, BUF_SIZE)) < 0)
	errExit("gp_scheduler");
    if(debug)
	printf("gp_scheduler: from geopebble: num=%d\n", numRead);
    memcpy(&theParams, buf, sizeof(struct param_st));
    if(debug)
	printf("got param: trig=%d\n", theParams.triggerType);

    // get the gps status.
    int gpsServerBAD;
    int nQuery=0;

    while(nQuery++ < MAXQUERY) {
	gpsServerBAD = get_gpsStatus(&gpsStat);
	if(debug)
	    printf("sched: trying gps again ret: %d nquery: %d\n", gpsServerBAD, nQuery);
	if(gpsServerBAD == 0) break;
	sleep(1);
    }

    if(nQuery == MAXQUERY)
    	errExit("sched: GPS SERVER DOWN\n");

    // ok, gps server good... wait until utc time valid?
    /*    FIXME - REMOVED FOR TESTING
    nQuery=0;
    while(nQuery++ < MAXQUERY) {
	if((gpsServerBAD=get_gpsStatus(&gpsStat))) {
	    sleep(1);
	    continue;
	}
	if(gpsStat.currUTCTimeValid) {
	    break;
	}
	if(debug)
	    printf("sched: utc NOT VALID\n");
    }
    */
    if(debug)
	printf("sched: gps server OK\n");

    FD_ZERO(&readfds);
    FD_SET(sfd, &readfds);
    fdmax = sfd;

    while(STOP==FALSE) {
	// ok, time valid.  how long to next trigger?
	struct timeval currTime, trigTime;
	struct itimerval itv, testitv; // two timers - one to set, one to test.

	int dt = timeToTrig(theParams, gpsStat, &trigTime, 0);
	if(dt <= 0)
	    errMsg("no valid trigger time");
	if(debug) {
	    time_t *t0=time(NULL);
	    printf("currtime: %ld trigtime %ld dt %d modunit %d modval %d\n",
		   t0, trigTime.tv_sec, dt, 
	       theParams.triggerModuloUnits, theParams.triggerModuloValue);
	}
	
	// ok, set the timer to 1 second before trigTime
	//
	if(dt > 0) {
	    if(gettimeofday(&currTime, NULL) == -1)
		errExit("sched: gettime");
	    itv.it_value.tv_sec = dt - 1; // time to timer expiration
	    itv.it_value.tv_usec = 1e6 - currTime.tv_usec; 
	    itv.it_interval.tv_sec=0; // no repeat
	    itv.it_interval.tv_usec=0;
	    if(debug)
		printf("sched: timer exp in %ld %ld\n", itv.it_value.tv_sec, itv.it_value.tv_usec);
	    if(setitimer(ITIMER_REAL, &itv, NULL) ==-1)
		errExit("sched:setitimer");
	}
	    
	rfds = readfds;
	// sleep forever - or until (1) timer expires - spawn
	// gp_store, or (2) msg from server sfd
	timeout.tv_sec=0;
	timeout.tv_usec=0;
	ret = select(fdmax+1, &rfds, NULL, NULL, NULL);
	//	ret = select(fdmax+1, &rfds, NULL, NULL, &timeout);
	switch(ret) {
	case 0: // timeout - nothing to read from socket
	    break;
	case -1: /* error or interrupt/signal */
	    // spawn gp_store
	    if(errno == EINTR) {
		if(gotAlarm) {
		    gotAlarm=0;
		    if(debug)
			printf("sched: got alrm: %ld\n", time(NULL));
		    int sockfd_store;
		    int pid = run_gp_store(argc, argv, &sockfd_store);
		    if(pid < 0)
			errExit("sched: couldn't run gp_store");
		    struct trig_st { // FIXME - this should be in geopebble.h
			struct timeval trigTime;
			struct param_st theParams;
		    } theTrig;
		    theTrig.theParams = theParams;
		    theTrig.trigTime = trigTime;
		    // send the parameters for triggering. 
		    if((ret=write(sockfd_store, &theTrig, sizeof(theTrig))) == -1)
			errExit("gp: writing params to scheduler failed");
		}
	    }		
	    break;
	default: // data available from socket
	    // read from server
	    if(FD_ISSET(sfd, &rfds)) {
		if((numRead = read(sfd, buf, BUF_SIZE)) < 0)
		    errExit("gp_scheduler");
		if(debug)
		    printf("gp_scheduler: from geopebble: num=%d\n", numRead);
		memcpy(&theParams, buf, sizeof(struct param_st));
		if(debug)
		    printf("got param: trig=%d\n", theParams.triggerType);
	    }
	}
    }

} /* main */

int run_gp_store(int mainc, char *mainv[], int *sockfd_store) 
{
    int sockfd[2];
    pid_t pktpid;
    char argsockfd[10];

    if(socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd)) {
	fprintf(stderr, "socketpair failed: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    } else {
	switch(pktpid=fork()) {
	case -1:
	    return(-1);
	    break;
	case 0:		/* child */
	    close(sockfd[0]); 
	    /* stringify the socket fd that will be in parent */
	    snprintf(argsockfd, sizeof(argsockfd), "%d", sockfd[1]);
	    
	    execl("./gp_store", "gp_store",  "-d", "-s", argsockfd, (char *)NULL);
	    
	    /* should never get here */
	    fprintf(stderr, "exec gp_adc failed: %m");
	    exit(EXIT_FAILURE);
	    break;
	    
	default:		/* parent */
	    close(sockfd[1]);
	    *sockfd_store = sockfd[0];
	    break;
	}
    }
    return(pktpid);
}

/* gp_sched.c ends here */
