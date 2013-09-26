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
#include "cJSON.h"

/* Macros */
#define BACKLOG 5

/* File-scope variables */
/* shared memory and semaphores */
int                      shm_fd;
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
int run_gp_sched(int mainc, char *mainv[], int *sockfd);
void setHardwareParams(struct param_st theParams);

/* Main */

int main(int argc, char *argv[]) {
    /* child PID for exec-ed processes */
    int                      gpspid, schedpid;
    /* signal */
    struct sigaction         act;
    /* getopt */
    char                    *optstring="dhGS";
    int                      opt, forkgps=1, forksched=1;
    /* socket */
    int                      sockfd_sched;
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
	    forksched=0;
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
	printf("Starting... sched=%d gps=%d\n", forksched, forkgps);

    if(setenv("TZ","UTC",1) == -1)
	errMsg("timezone not set");

    sigemptyset(&act.sa_mask); /* nothing is blocked */
    act.sa_flags = 0;

    act.sa_handler=sig_handler;
    sigaction(SIGTERM, &act, 0);
    //    sigaction(SIGINT, &act, 0);
    //    sigaction(SIGCHLD, &act, 0);
    //    sigaction(SIGPIPE, &act, 0);
    //    sigaction(SIGUSR1, &act, 0);

    /* run gp_sched */
    if(forksched) {
	if((schedpid=run_gp_sched(argc, argv, &sockfd_sched)) < 0) {
	    fprintf(stderr, "run_gp_sched failed");
	    exit(EXIT_FAILURE);
	}
	if(debug)
	    printf("geopebble: started gp_sched: pid: %d sockfd_sched: %d\n", schedpid, sockfd_sched);
    }
    
    /* set the parameters */
    struct param_st theParams;
    if(parseJSONFile(DEFAULT_PARAMS_FILE,  &theParams)) {
	;; // error handler
    }
    /* the params.txt file is the most-recently updated params */
    if(parseJSONFile("params.txt",  &theParams)) {
	;; // error handler
    }
    setHardwareParams(theParams);
    	
    /* set up select on the sockets */
    fdmax=STDIN_FILENO;
    FD_ZERO(&readfds);

    if(forksched) {
	FD_SET(sockfd_sched, &readfds);
	fdmax=max(fdmax, sockfd_sched);
	// send the parameters for triggering.  These can be resent later if
	// the base stations sends new parameters.
	if((ret=write(sockfd_sched, &theParams, sizeof(theParams))) == -1)
	    errExit("gp: writing params to scheduler failed");
    }

    /* open a socket to the base station */
    /* FIXME - this will block until gp_base creates a socket and
       begins listening */
    int sfd;
    if((sfd = unixConnect(BASE_SOCK_PATH, SOCK_STREAM)) <0) {
	errMsg("gp: connect");
	return(-1);
    }
    FD_SET(sfd, &readfds);
    fdmax=max(fdmax, sfd);

    //    FD_SET(STDIN_FILENO, &readfds);
    if(debug)
	printf("fdmax=%d\n", fdmax);

    while(STOP==FALSE) {

	/* get GPS status */
	struct gps_st gpsStat;
	int gpsServerBAD = get_gpsStatus(&gpsStat); // if bad, then =1, if OK then =0
	
	rfds = readfds; /* select will modify rfds, so reset */
	timeout.tv_sec=1;  //FIXME? set to never time out? watchdog?
	timeout.tv_usec=0;
	
	/* block here until data available on the rfds files, timeout,
	   or interrupt/signal? */
	ret=select(fdmax+1, &rfds, NULL, NULL, &timeout);
	
	switch(ret) {
	case 0: // timeout 
	    /* FIXME pet watchdog */
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
	    if(forksched) {
		if(FD_ISSET(sockfd_sched, &rfds)) {
		    if((numRead = read(sockfd_sched, buf, BUF_SIZE)) < 0)
			errExit("gp: sched socket");
		    if(debug)
			printf("gp: from sched: num=%d buf=%s\n", numRead, buf);
		}
	    }
	    // this is a stand-in for TCP cmds for testing
	    // expecting json packets
	    if(FD_ISSET(sfd, &rfds)) { 
		if(debug)
		    printf("gp: got base sta\n");
		/* read from base station... */
		int indx=0;
	    	while((numRead = read(sfd, buf+indx, BUF_SIZE-indx)) > 0)
		    indx += numRead;

		// error reading
		if(numRead < 0) {
	    	    errExit("gp: base sta read");
		}
		// gp_base has died...restart system?
		if(numRead==0) { 
		    errMsg("gp: base has died");
		    FD_CLR(sfd, &readfds);
		    break;
		}

	    	if(debug)
	    	    printf("gp: from stdin/tcp: num=%d buf=%s\n", indx, buf);
		/* 
		 *  FIXME - these settings need to be stored to `params.txt' for use on 
		 *  next boot
		 */
		//		updateParamsFile("params.txt", buf);

		
		cJSON *root = cJSON_Parse(buf);
		cJSON *obj;

		if(root==NULL)
		    break;
		
		// got settings changes - apply them
		if((obj = cJSON_GetObjectItem(root, "settings")) || (obj = cJSON_GetObjectItem(root, "triggering"))) { 
		    if(debug)
			printf("gp: json settings\n");
		    parseJSONString(buf, &theParams);
		    if((ret=write(sockfd_sched, &theParams, sizeof(theParams))) == -1)
			errExit("gp: writing params to scheduler failed");
		}
		if((obj = cJSON_GetObjectItem(root, "communications"))) {
		    // handle changes in the communications settings
		}
		if((obj = cJSON_GetObjectItem(root, "state"))) {
		    // handle requests for state (of health)
		}
		if((obj = cJSON_GetObjectItem(root, "command"))) {
		    // "command" is not a section but a string?
		    // handle commands such as ls, cp
		}
		cJSON_Delete(root);
		cJSON_Delete(obj);

		break;
	    }
	}
    }
    exit(0);
}

/*
 * run_gp_sched - exec `gp_sched' 
 *
 * return the PID of the exec-ed child or -1 on failed socketpair.
 */
int run_gp_sched(int mainc, char *mainv[], int *sockfd_sched) 
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
	    if(debug)
		printf("geopebble: fork: argsockfd = %s\n", argsockfd);
	    execl("./gp_sched", "gp_sched", "-d", "-s", argsockfd, (char *)NULL);
	    /* should never get here */
	    fprintf(stderr, "exec gp_sched failed: %m");
	    exit(EXIT_FAILURE);
	    break;
	default:		/* parent */
	    close(sockfd[1]); /* don't need in parent */
	    *sockfd_sched=sockfd[0];
	    break;
	}
    }
    return(pktpid);
}

/* geopebble.c ends here */
