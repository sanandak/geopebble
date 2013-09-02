/*
 * gp_base - TCP and UDP interactions to base via wifi 
 *
 * redpine wifi is a hardware device (either file-based or REG32)
 *
 * UDP packets arrive from geopebble and are writtten to redpine UDP
 *
 * TCP packets arrive from base station, are written to geopebble via
 * socket, and the response from geopebble is sent back to base (via redpine).
 *
 * passive listener ("server") for UDP packets to geopebble
 * active connect ("client") for TCP packets to geopebble
 *
 */

#define BACKLOG 5

#include "geopebble.h"
#include "unix_sockets.h"
#include "error_functions.h"
int debug = 1;

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


/* redpine interactions */
int init_redpine(void);
int writeUDPPacket(char *buf, int nbuf);
int writeTCPPacket(char *buf, int nbuf);
int readTCPPacket(char *buf, int nbuf);

int init_redpine(void) {
    return -1;
}
int writeUDPPacket(char *buf, int nbuf) {
    return(-1);
}
int writeTCPPacket(char *buf, int nbuf) {
    return(-1);
}
int readTCPPacket(char *buf, int nbuf) {
    return(-1);
}

int main(int argc, char *argv[]) {
    /* signal */
    struct sigaction         act;

    /* select */
    fd_set readfds, rfds;
    int    fdmax;
    int    sfd; /* fd for geopebble TCP connection */
    int    rfd; /* fd for redpine */
    char buf[BUF_SIZE];

    sigemptyset(&act.sa_mask); /* nothing is blocked */
    act.sa_flags = 0;

    act.sa_handler=sig_handler;
    sigaction(SIGTERM, &act, 0);

    fdmax=0;
    FD_ZERO(&readfds);

    /* set up a listening socket.. accept connection from geopebble
       server */
    int sfd_srv;
    if(remove(BASE_SOCK_PATH) == -1 && errno != ENOENT)
	errExit("remove-%s", BASE_SOCK_PATH);
    if((sfd_srv = unixListen(BASE_SOCK_PATH, BACKLOG)) < 0)
	errExit("base: tcp listen");
    /* wait for connect request from geopebble */
    if((sfd = accept(sfd_srv, 0, 0)) == -1)
	errExit("base: accept");
    if(debug)
	printf("accepted gp: %d\n", sfd);
    FD_SET(sfd, &readfds);
    fdmax=max(fdmax, sfd);

    /* open a passive socket "server" for receiving UDP packets */
    /* remove old if still around.. */
    if(remove(UDP_SOCK_PATH) == -1 && errno != ENOENT) 
	errExit("remove - %s", UDP_SOCK_PATH);
    int ufd;
    ufd = unixBind(UDP_SOCK_PATH, SOCK_DGRAM);
    if(ufd < 0)
	errMsg("gp_base: udp bind");
    FD_SET(ufd, &readfds);
    fdmax = max(ufd, fdmax);

    /* init redpine */
    rfd = init_redpine();
    if(rfd < 0)
	errMsg("gp_base: no redpine\n");
    else {
	FD_SET(sfd_srv, &readfds);
	fdmax=max(fdmax, sfd_srv);
    }

    while(STOP == FALSE) {
	int ret, nwrt, numRead;
	struct timeval timeout;
	rfds = readfds;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	ret = select(fdmax+1, &rfds, NULL, NULL, &timeout);
	switch(ret) {
	case 0: // timeout
	    break;
	case -1: // error or interrupt
	    if(errno == EINTR) 
		break;
	    errExit("gp_base");
	    break;

	default: // data on sockets or redpine
	    /* handle redpine data */
	    if(rfd > 0 && FD_ISSET(rfd, &rfds)) {
		// read from readpine
		numRead = readTCPPacket(buf, BUF_SIZE);
		if((nwrt = write(sfd, buf, numRead)) != numRead) {
		    errMsg("gp_base: wrt to gp");
		}
	    }
	    /* handle packet from geopebble */
	    if(FD_ISSET(sfd, &rfds)) {
		numRead = read(sfd, buf, BUF_SIZE);
		if(numRead < 0)
		    errMsg("gp_base: tcp close");
		if(debug)
		    printf("sfd: numread=%d\n", numRead);
		/* return a status message */
	    }
	    if(FD_ISSET(ufd, &rfds)) { // udp data
		numRead = read(ufd, buf, BUF_SIZE);
		if(numRead < 0)
		    errMsg("gp_base: udp close");
		writeUDPPacket(buf, numRead);
	    }
	    break;
	} // case
    } // while(STOP==FALSE)

    exit(0);
}

