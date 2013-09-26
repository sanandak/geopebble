/* geopebble.c ---
 * 
 * Filename: geopebble.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Wed Jun 19 11:06:48 2013 (-0400)
 * Version: 
 * 
 * geopebble acts as a server, with a listen()-ing socket on FIXME
 * other processes (gp_wifi, gp_adc, etc) should connect() once a second or so
 * to indicate that they are alive, and to receive any instructions.
 */

/* Change Log:
 * 
 * 
 */

/* Code: */

/* Headers */
#include "geopebble.h"
#include "unix_sockets.h"
#include "gp_params.h" /* */

/* Macros */
#define BACKLOG 5

/* File-scope variables */
/* shared memory and semaphores */
int                      shm_fd;
sem_t             *sem_adc;
sem_t             *sem_gps;
struct shm_adc_st *shm_adc;
struct gps_st *shm_gps;

/* External variables */
int             debug;
char            progname[128];
				/* command line processing */
extern int 	optind, opterr;
extern char 	*optarg;

/* External functions */

/* Structures and Unions */

/* Signal catching functions */
volatile int STOP=FALSE;
/* set STOP and return */
void sig_handler(int sig)
{
    //     msglog("%d received sig %d - %s", getpid(), sig, (char *)strsignal(sig));
    // NOT SAFE IN SIG_HANDLER
    fprintf(stderr, "%d received sig %d - %s\n", getpid(), sig, (char *)strsignal(sig));
    STOP=TRUE;
}

static volatile sig_atomic_t gotSigio = 0;
void
int_handler()
{
    /* on interrupt, read TCP packet from redpine */
    gotSigio = 1;
}


/* Functions */
int boot_redpine();
int mkshm();
int run_gp_gps(int mainc, char *mainv[], int *sockfd);
int run_gp_store(int mainc, char *mainv[], int *sockfd);
/* Main */

int main(int argc, char *argv[]) {
    /* child PID for exec-ed processes */
    int                      gpspid, storepid;
    /* signal */
    struct sigaction         act;
    /* getopt */
    char                    *optstring="dhGS";
    int                      opt, forkgps=1, forkstore=1;
    /* socket */
    int                      sockfd_gps, sockfd_store;
    ssize_t                  numRead, ret;
    char                     buf[BUF_SIZE];
    /* select */
    int                      fdmax;
    fd_set                   readfds, rfds;
    struct timeval           timeout;

    debug=opterr=0;
    /* 
     * command-line processing
     */
    while((opt=getopt(argc,argv,optstring)) != -1) {
	switch(opt) {
	case 'd':
	    debug++;
	    break;
	case 'G':
	    forkgps=0;
	    break;
	case 'S':
	    forkstore=0;
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
	printf("Starting... store=%d gps=%d\n", forkstore, forkgps);


    sigemptyset(&act.sa_mask); /* nothing is blocked */
    act.sa_flags = 0;

    act.sa_handler=sig_handler;
    sigaction(SIGTERM, &act, 0);
    //    sigaction(SIGINT, &act, 0);
    //    sigaction(SIGCHLD, &act, 0);
    //    sigaction(SIGPIPE, &act, 0);
    //    sigaction(SIGUSR1, &act, 0);

    /* set up shared memory */
    //mkshm();
    
    /* run gp_gps */
    if(forkgps) { 
	if((gpspid=run_gp_gps(argc, argv, &sockfd_gps)) < 0) { // WARN no socketpair to GPS...
	    fprintf(stderr, "run_gp_gps failed");
	    exit(EXIT_FAILURE);
	}
	if(debug)
	    printf("gp: started gp_gps: pid: %d sockfd_gps=%d\n", gpspid, sockfd_gps);
    }

    /* run gp_store */
    /*
    if(forkstore) {
	if((storepid=run_gp_store(argc, argv, &sockfd_store)) < 0) {
	    fprintf(stderr, "run_gp_store failed");
	    exit(EXIT_FAILURE);
	}
	if(debug)
	    printf("gp_adc: started gp_store: pid: %d sockfd_store: %d\n", storepid, sockfd_store);
    }
    */

    if(boot_redpine()) {
	;; // error handler
    }
    
    /* set the parameters */
    struct param_st theParams;
    if(parseJSONFile(DEFAULT_PARAMS_FILE,  &theParams))
	;; // error handler
    }
    if(parseJSONFile("params.txt",  &theParams))
	;; // error handler
    }
    	
    /* set up select on the sockets */
    fdmax=STDIN_FILENO;
    FD_ZERO(&readfds);
    if(forkgps) {
	FD_SET(sockfd_gps, &readfds);
	fdmax=max(fdmax, sockfd_gps);
    }

    /*
    if(forkstore) {
	FD_SET(sockfd_store, &readfds);
	fdmax=max(fdmax, sockfd_store);
    }
    */

    //    FD_SET(STDIN_FILENO, &readfds);
    if(debug)
	printf("fdmax=%d\n", fdmax);

    while(STOP==FALSE) {
	rfds = readfds; /* select will modify rfds, so reset */
	timeout.tv_sec=1;  //FIXME? set to never time out? watchdog?
	timeout.tv_usec=0;
	
	/* block here until data available on the rfds files, timeout,
	   or interrupt/signal? */
	if(debug)printf("waiting in geopebble...\n");
	ret=select(fdmax+1, &rfds, NULL, NULL, &timeout);
	
	switch(ret) {
	case 0: // timeout 
	    if(debug) fprintf(stderr, "gp: timeout...\n");
	    /* FIXME pet watchdog */

	    // FIXME sending a request to gp_gps to get gpsStat "S" = status
	    snprintf(buf, BUF_SIZE, "S");
	    if(write(sockfd_gps, buf, strlen(buf)+1) != strlen(buf)+1)
		errMsg("gp_gps_write failed!");
	    break;

	case -1: /* error or interrupt/signal */
	    if(errno == EINTR) { /* interrupt from redpine */
		if(gotSigio) {
		    gotSigio = 0;
		    /* handle interrupt - TCP from redpine? */
		} else 
		    errExit("gp");
	    }
	    break;

	default: // data available from sockets
	    if(FD_ISSET(sockfd_gps, &rfds)) {
		if((numRead = read(sockfd_gps, buf, BUF_SIZE)) < 0)
		    errExit("gp: gps socket");
		if(debug)
		    printf("gp: from gps: num=%d buf=%d\n", numRead, sizeof(struct gps_st));
	    }
	    if(FD_ISSET(sockfd_store, &rfds)) {
		if((numRead = read(sockfd_store, buf, BUF_SIZE)) < 0)
		    errExit("gp: storage socket");
		if(debug)
		    printf("gp: from store: num=%d buf=%s\n", numRead, buf);
	    }
	    if(FD_ISSET(STDIN_FILENO, &rfds)) { // this is a stand-in for TCP cmds
	    	if((numRead = read(STDIN_FILENO, buf, BUF_SIZE)) < 0)
	    	    errExit("gp: stdin");
	    	if(debug)
	    	    printf("gp: from stdin: num=%d buf=%c\n", numRead, buf[0]);
	    }
	    break;
	}
    }
    exit(0);
}

/* 
 * shared memory (removed semaphores)
 * - shm_adc  (adc data: shm_adc_st)
 * - shm_gps  (gps data: gps_st)
 * - shm_cca and sem_cca (command, control, and aux: shm_cci_st). TODO
 */

int mkshm() {

    /* ADC shared memory */

    /* create shared memory */
    shm_unlink(SHMNAME_ADC); // close if already open - shouldn't be the case.
    shm_fd =shm_open(SHMNAME_ADC, O_CREAT | O_RDWR, (S_IRUSR | S_IWUSR));
    if(shm_fd == -1)
	errExit("gp: shm_fd_adc");
    if(ftruncate(shm_fd, sizeof(struct shm_adc_st)) == -1)
	errExit("gp: ftruncate shm");
    /* map shared memory */
    shm_adc = mmap(NULL, sizeof(struct shm_adc_st), 
		    PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm_adc == MAP_FAILED)
	errExit("gp: mmap shm");
    close(shm_fd); /* no longer needed - use shm */

    /* GPS shared memory */
    /* create shared memory */
    shm_unlink(SHMNAME_GPS); // close if already open - shouldn't be the case.
    shm_fd =shm_open(SHMNAME_GPS, O_CREAT | O_RDWR, (S_IRUSR | S_IWUSR));
    if(shm_fd == -1)
	errExit("gp: shmfdgps");
    if(ftruncate(shm_fd, sizeof(struct gps_st)) == -1)
	errExit("gp: ftruncate");
    /* map shared memory */
    /* shm_gps is a pointer to struct gps_st */
    /* so shm_gps->nSV */
    shm_gps = mmap(NULL, sizeof(struct gps_st), 
		    PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm_gps == MAP_FAILED)
	errExit("gp: shmgps");
    close(shm_fd); /* no longer needed - use shm */

    return(0);
}


/*
 * run_gp_gps - exec `gp_gps'
 *
 * return the PID of the exec-ed child or -1 on failed `pipe' or failed dup.
 * sockfd_gps is the file for socket read/write to gps
 */

int run_gp_gps(int mainc, char *mainv[], int *sockfd_gps) 
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
	    //	       execlp("an_pktbuf", "an_pktbuf", "-d", (char *)0);
	    if(debug)
		printf("gp: fork: argsockfd = %s\n", argsockfd);

	    //	    execl("./gp_adc", "gp_adc", "-d", "-s", argsockfd, *(char *)NULL);
	    execl("./gp_gps", "gp_gps",  "-i", "-d", "-s", argsockfd, (char *)NULL);
	    //	    execl("/bin/ls", "ls", (char *)NULL);
	    /* should never get here */
	    fprintf(stderr, "exec gp_adc failed: %m");
	    exit(EXIT_FAILURE);
	    break;
	default:		/* parent */
	    close(sockfd[1]); /* don't need in parent */
	    *sockfd_gps=sockfd[0];
	    break;
	}
    }
    return(pktpid);
}


/*
 * run_gp_store - exec `gp_store' 
 *
 * return the PID of the exec-ed child or -1 on failed socketpair.
 */



int run_gp_store(int mainc, char *mainv[], int *sockfd_store) 
{
    int sockfd[2];
    pid_t pktpid;
    char argsockfd[10];

    if(socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd)) {
	fprintf(stderr, "socketpair failed: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    } else {
	if(debug)
	    printf("geopebble: sockfd: %d %d\n", sockfd[0], sockfd[1]);
	switch(pktpid=fork()) {
	case -1:
	    return(-1);
	    break;
	case 0:		/* child */
	    close(sockfd[0]); 
	    /* stringify the socket fd that will be in parent */
	    snprintf(argsockfd, sizeof(argsockfd), "%d", sockfd[1]);
	    //	       execlp("an_pktbuf", "an_pktbuf", "-d", (char *)0);
	    if(debug)
		printf("gp_adc: fork: argsockfd = %s\n", argsockfd);
	    execl("./gp_store", "gp_store", "-d", "-s", argsockfd, (char *)NULL);
	    //	    execl("/bin/ls", "ls", (char *)NULL);
	    /* should never get here */
	    fprintf(stderr, "exec gp_store failed: %m");
	    exit(EXIT_FAILURE);
	    break;
	default:		/* parent */
	    close(sockfd[1]); /* don't need in parent */
	    *sockfd_store=sockfd[0];
	    break;
	}
    }
    return(pktpid);
}



int boot_redpine() {
    return(0);
}

/* geopebble.c ends here */
