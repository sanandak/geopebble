/* testADC.c --- 
 * 
 * Filename: testADC.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Tue Jul  9 08:31:05 2013 (-0400)
 * Version: 
 * Last-Updated: Tue Jul  9 09:36:13 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 42
 * 
 */


/* Code: */
/* Headers */
#include "geopebble.h"

/* Macros */

/* File-scope variables */

/* shared memory and semaphores */
sem_t             *sem_adc;
struct shm_adc_st *shm_adc;
sem_t             *sem_gps;
struct shm_gps_st *shm_gps;

int debug;
/* command line processing */
extern int 	optind, opterr;
extern char 	*optarg;

/* External variables */

/* External functions */

/* Structures and Unions */

/* Signal catching functions */
volatile int STOP=FALSE;
void
sig_handler(int sig)
{
    fprintf(stderr, "adc: %d received sig %d - %s\n", getpid(), sig, (char *)strsignal(sig));
    STOP = TRUE;
}

static struct timeval startTime;
static struct timeval oldTime = { 0 };
int sampsPerRow=0;
static volatile sig_atomic_t gotSamptim = 0;
void
itv_handler(int sig)
{
    //    fprintf(stderr, "adc: timer: %d received sig %d - %s\n", getpid(), sig,(char *)strsignal(sig));
    gotSamptim = 1;
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
void setup_ADC();
int validateSampleRate(char *s);
int validateGain(char *s, float g[]);
int validateChannels(char *s, int ch[]);
int populateShmAdc(int *prow, int *psamp, int standalone);
/* Main */

int main(int argc, char *argv[]) {
    /* child PID */
    int                      storepid;
    /* signal */
    struct sigaction         act;
    /* socket */
    struct sockaddr_un       addr;
    int                      sfd=STDIN_FILENO, sockfd_store=0;
    ssize_t                  numRead, ret;
    char                     buf[BUF_SIZE];
    /* select */
    int                      fdmax;
    fd_set                   readfds, rfds;
    struct timeval           timeout;
    /* getopt */
    char                    *optstring="dhr:g:c:s:DS";
    int                      opt, dofork=1;
    int                      defaultSettings=0;
    int                      standalone=0;
    int                      sampRate,  channels[4];
    int                      sampInt_us;
    float                    gains[4];
    /* shared memory */

    printf("started gp_adc\n");

    debug=opterr=0;
    /* parameter setting hierarchy:
     * - settings from previous run $(GP_HOME)/etc/previousSettings.json
     * - next, any command line flags overwrite above
     * - if -D, always use ALL defaults $(GP_HOME)/etc/defaultSettings.json,
    */
    defaultSettings=0;
    /* set the params to previous settings initially */
    /* FIXME - these should be read from previousSettings. */
    sampRate = 1000;
    gains[0]=1; gains[1]=1; gains[2]=1;gains[3]=1;
    channels[0]=0;    channels[1]=0;    channels[2]=0;    channels[3]=0;
    
    /* 
     * command-line processing
     */
    while((opt=getopt(argc,argv,optstring)) != -1) {
	switch(opt) {
	case 'S': /* run standalone - don't use shared mem/semaph, write to stdout */
	    standalone=1;
	    break;
	case 'D': /* use the default settings */
	    defaultSettings=1;
	    break;
	case 'r': /* sample Rate, 10k, 5k, 4k, 2k, 1k, 0.5, 0.25, 0.1 */
	    if((sampRate=validateSampleRate(optarg)) == -1)
		sampRate=1000;
	    break;
	case 'g': /* gains of 0.2, 1, 10, 20, 30, 40, 60, 80, 119, and 157 */
	          /* default = 1 */
	          /* comma-separated list of gains, round down to closest */
	    if(validateGain(optarg, &gains) == -1)
		gains[0]=gains[1]=gains[2]=gains[3]=1.;
	    break;
	case 'c': /* -c 0 (default all internal; set i-th bit for external */
	          /* -c 0x01 (ch0 = external, ch1-3 external) -c 0x0f all ext */
	    if(validateChannels(optarg, &channels) == -1)
		channels[0]=channels[1]=channels[2]=channels[3]=0;
	    break;
	case 'd':
	    debug++;
	    break;
	case 's':  /* socketpair from parent (geopebble server) */
	    sfd = atoi(optarg);
	    break;
	case '?':
	case 'h':
	default:
	    fprintf(stderr, "Usage: %s\n", "gp_adc TODO");
	    exit(-1);
	}
    }
    if(defaultSettings) {
	/* FIXME - these should be read from defaultSettings. */
	sampRate = 1000;
	gains[0]=1; gains[1]=1; gains[2]=1;gains[3]=1;
	channels[0]=0;    channels[1]=0;    channels[2]=0;    channels[3]=0;
    }
    sampsPerRow = sampRate/10; // 0.1 seconds per row in shared memory
    sampInt_us = 1e6/sampRate;

    if(debug) {
	printf("gp_adc: samp = %d sampInt-us = %d gain = %f %f %f %f ch = %d, %d %d %d sfd = %d\n", sampRate, sampInt_us, gains[0], gains[1], gains[2], gains[3], channels[0], channels[1], channels[2], channels[3], sfd);
    }

    sigemptyset(&act.sa_mask); /* nothing is blocked */
    act.sa_flags = 0;

    act.sa_handler=sig_handler;
    sigaction(SIGTERM, &act, 0);
    //    sigaction(SIGINT, &act, 0);
    //    sigaction(SIGCHLD, &act, 0);
    //    sigaction(SIGPIPE, &act, 0);
    //    sigaction(SIGUSR1, &act, 0);
    //    sigaction(SIGHUP, &act, 0);
    //    sigaction(SIGCONT, &act, 0);



    if(!standalone)
	attachshm();

    //    setup_ADC();

    act.sa_handler=itv_handler;
    sigaction(SIGALRM, &act, NULL);

    struct itimerval itv;
    struct timeval curr;
    itv.it_interval.tv_sec=0;     // repeat the timer
    itv.it_interval.tv_usec=sampInt_us; // 
    itv.it_value.tv_sec=5;        // start the timer
    itv.it_value.tv_usec=0;
    if(setitimer(ITIMER_REAL, &itv, 0) == -1)
	errExit("setitimer");

    /* set up select on the sfd ("server fd") */
    FD_ZERO(&readfds);
    FD_SET(sfd, &readfds);
    fdmax=sfd;

    while(STOP==FALSE) {
	int r, s;
	rfds = readfds; /* select will modify rfds, so reset */
	//timeout.tv_sec=1; //TODO #define WIFI_READ_TIMEOUT 1;
	//timeout.tv_usec=0;

	// TESTING >>>
	gotSamptim=0;
	// <<< TESTING

	/* block here until data available on the rfds files, timeout,
	   or interrupt/signal? */
	
	//	if(debug)printf("waiting in gp_adc...\n");
	ret=select(fdmax+1, &rfds, NULL, NULL, NULL);// no timeout
	
	switch(ret) {
	case 0: // timeout - send "I'm alive" message to server.
	    snprintf(buf, BUF_SIZE, "gp_adc %d", (int) time(NULL));
	    if(debug) fprintf(stderr, "gp_adc: timeout buf=%s len=%d\n", buf, strlen(buf));
	    if(write(sfd, buf, strlen(buf)) != strlen(buf))
		fatal("partial/failed write");
	    break;
	case -1: /* error or interrupt/signal */
	    //	    if(debug)
	    //		printf("adc intr\n");
	    if(errno == EINTR) { 
		/* handle gotSigio interrupt from ADC here... */
		// TESTING >>>
		// select returned due to timer expiring
		if(gotSamptim) {
		    gotSamptim = 0;
		    if(populateShmAdc(&r, &s, standalone)) {
			;
		    }
		}
		// <<< TESTING
	    }
	    break;
	default: // data available
	    // read from server
	    if(FD_ISSET(sfd, &rfds)) {
		if((numRead = read(sfd, buf, BUF_SIZE)) < 0)
		    errExit("gp_adc");
		if(debug)
		    printf("gp_adc: from geopebble: num=%d\n", numRead);
	    }
	} /* switch */
    } /* while */
    
    exit(0);
    
} /* main */
 
void setup_ADC() {
    ;;
}

int attachshm() {
    struct stat              shm_stat;
    int shm_fd;
    /* GPS shared memory */
    /* set up semaphore for signaling GPS shared memory available */
    sem_gps = sem_open(SHMNAME_GPS, O_RDWR);
    if(sem_gps==SEM_FAILED)
	return(-1);
    /* attach to (already open?) shared memory */
    shm_fd =shm_open(SHMNAME_GPS, O_RDWR, (S_IRUSR | S_IWUSR));
    if(shm_fd == -1)
	return(-1);
    if(fstat(shm_fd, &shm_stat) == -1)
	return(-1);
    /* map shared memory */
    shm_gps = mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm_gps == MAP_FAILED)
	return(-1);
    close(shm_fd); /* no longer needed - use shm */

    /* set up semaphore for signaling ADC shared memory available */
    sem_adc = sem_open(SHMNAME_ADC, O_RDWR);
    if(sem_adc==SEM_FAILED)
	return(-1);

    /* attach to (already open?) shared memory */
    shm_fd =shm_open(SHMNAME_ADC, O_RDWR, (S_IRUSR | S_IWUSR));
    if(shm_fd == -1)
	return(-1);
    if(fstat(shm_fd, &shm_stat) == -1)
	return(-1);
    /* map shared memory */
    shm_adc = mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm_adc == MAP_FAILED)
	return(-1);
    close(shm_fd); /* no longer needed - use shm */
    return(0);
}

int populateShmAdc(int *prow, int *psamp, int standalone) {
    int startedNewRow=0;
    static int s=0; // sampInd 
    static int r=-1; // rowNum
    int theSampleValue0, theSampleValue1, theSampleValue2, theSampleValue3;
    int theSampleValue4;
    double period=10; // TESTING T (s) for fake sine wave.
    double ts=0, fake=0;
    struct timeval currTime;

    // TESTING - use gpsTime here?
    gettimeofday(&currTime, NULL); 
    ts=(double)currTime.tv_sec + (double)currTime.tv_usec / 1e6;
    fake = sin(2.*M_PI*(ts/period));
    theSampleValue0 = (int)(1000.*fake);
    theSampleValue1 = (int)(2000.*fake);
    theSampleValue2 = (int)(3000.*fake);
    theSampleValue3 = (int)(4000.*fake);
    theSampleValue4=currTime.tv_usec;
    // TESTING

    if(standalone) {
	printf("%d %lf %d %d %d %d %d %d\n", s, ts, currTime.tv_sec, currTime.tv_usec,
	       theSampleValue0, theSampleValue1, theSampleValue2, theSampleValue3);
	s++;
    } else {
	// determine if new row, and store gpstime for that row
	// start a new row if we have sampsPerRow samps (1/10sec at samprate)
	// OR if the time rolls over to a new second. (these SHOULD coincide).
	if(s % sampsPerRow == 0 ||           // new row
	   currTime.tv_sec - oldTime.tv_sec > 0) {  // new second
	    startedNewRow = 1; 
	    r++; // initial value is -1, so now 0 or larger...

	    //if(debug) printf("adc: in populate. s=%d r=%d\n",s, r);

	    /* CRITICAL SECTION */
	    sem_post(sem_adc);
	    shm_adc->currentRow = r;
	    shm_adc->GPSTimeAtPPS[r % SHM_NROWS].tv_sec=currTime.tv_sec;
	    shm_adc->GPSTimeAtPPS[r % SHM_NROWS].tv_usec=currTime.tv_usec;
	    sem_wait(sem_adc);
	    /* END CRITICAL SECTION */
	    // save the time to check for new sec next loop...
	    oldTime.tv_sec=currTime.tv_sec;
	    oldTime.tv_usec=currTime.tv_usec;
	}
	/* CRITICAL SECTION */
	/* FIXME - is this needed?  Nobody should be reading this
	 * if I am writing 
	 * They should be paying attention to shm_adc.currentRow
	 */
	//		    sem_post(sem_adc);
	// TESTING - use rdata0 etc here >>>
	shm_adc->data0[r % SHM_NROWS][s % sampsPerRow] = theSampleValue0;
	shm_adc->data1[r % SHM_NROWS][s % sampsPerRow] = theSampleValue1;
	shm_adc->data2[r % SHM_NROWS][s % sampsPerRow] = theSampleValue2;
	shm_adc->data3[r % SHM_NROWS][s % sampsPerRow] = theSampleValue3;
	// TESTING - use count at drydout here
	shm_adc->data4[r % SHM_NROWS][s % sampsPerRow] = theSampleValue4;
	s++;
	//		    sem_wait(sem_adc);
	/* END CRITICAL SECTION */
    }
    *prow=r;
    *psamp=s;
    return(startedNewRow);
}

int validateSampleRate(char *s) {
    int r, tryr;
    tryr=strtol(s, NULL, 10);
    if(tryr>=10000) r=10000;
    if(tryr>=5000 && tryr <10000) r=5000;
    if(tryr>=4000 && tryr <5000) r=4000;
    if(tryr>=2000 && tryr <4000) r=2000;
    if(tryr>=1000 && tryr <2000) r=1000;
    if(tryr>=500 && tryr <1000) r=500;
    if(tryr>=250 && tryr <500) r=250;
    if(tryr<250) r=100;
    return r;
}

int validateGain(char *s, float g[]) {
    char *tok = strtok(s, ","); // first call with string s ...
    char *end;
    int i;

    for(i=0; i<4; i++) {
	errno=0;
	g[i]=strtof(tok, &end); 
	if(tok==end) // unable to convert to number
	    return(-1);
	tok=strtok(NULL, ",");// subsequent calls with NULL
	if(tok==NULL)
	    break;
    }
    if(i==1 || i==2) { // only 2 or 3 values - not allowed.
	return(-1);
    }
	
    if(i==0) { // only one value...
	g[1]=g[2]=g[3]=g[0]; // set all gains to first gain;
    }
    for(i=0;i<4;i++) { // and regularize gains to allowed values...
	if(g[i] < 1.0) g[i] = 0.5;
	if(g[i] >= 1.0 && g[i] < 10.) g[i]=1.0;
	if(g[i] >= 1.0 && g[i] < 10.) g[i]=1.0;
	if(g[i] >= 10.0 && g[i] < 20.) g[i]=10.0;
	if(g[i] >= 20.0 && g[i] < 30.) g[i]=20.0;
	if(g[i] >= 30.0 && g[i] < 40.) g[i]=30.0;
	if(g[i] >= 40.0 && g[i] < 60.) g[i]=40.0;
	if(g[i] >= 60.0 && g[i] < 80.) g[i]=60.0;
	if(g[i] >= 80.0 && g[i] < 119.) g[i]=80.0;
	if(g[i] >= 119.0 && g[i] < 157.) g[i]=119.0;
	if(g[i] >= 157.0) g[i]=157.0;
    }
    return(0);
}

int validateChannels(char *s, int ch[]) {
    int ic;
    errno=0;
    if(strchr(s, 'x'))
	ic=strtol(s, NULL, 16); // convert hex number
    else
	ic=strtol(s, NULL, 10);

    if(errno!=0 && ic==0)
	return(-1);

    ch[0] = ic & BIT0;
    ch[1] = ic >> 1 & BIT0;
    ch[2] = ic >> 2 & BIT0;
    ch[3] = ic >> 3 & BIT0;
    return(0);
}



/* gp_adc.c ends here */



/* testADC.c ends here */
