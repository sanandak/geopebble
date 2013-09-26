/* testStore.c --- 
 * 
 * Filename: testStore.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Thu Aug 15 10:14:22 2013 (-0400)
 * Version: 
 * Last-Updated: Thu Aug 15 10:31:59 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 7
 * URL: 
 * Doc URL: 
 * Keywords: 
 * Compatibility: 
 * 
 */

/* Commentary: 
 * 
 * 
 * 
 */

/* Change Log:
 * 
 * 
 */

/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110-1301, USA.
 */

/* Code: */
#include "geopebble.h"
#include "gp_params.h"

int main() {
    struct trig_st { // FIXME - this should be in geopebble.h
	struct timeval trigTime;
	struct param_st theParams;
    } theTrig;
    
    struct param_st theParams;
    struct timeval currTime, trigTime;

    theParams.sampleRate=1000;
    theParams.recordLength=4;
    
    gettimeofday(&currTime, NULL);
    trigTime = currTime;
    trigTime.tv_sec += 5; // trigger in 5 sec

    printf("curr = %ld trig = %ld\n", currTime.tv_sec, trigTime.tv_sec);
    int sockfd_store, ret;
    int pid = run_gp_store(0, NULL, &sockfd_store);

    theTrig.theParams = theParams;
    theTrig.trigTime = trigTime;
    if((ret=write(sockfd_store, &theTrig, sizeof(theTrig))) == -1)
	errExit("gp: writing params to scheduler failed");

}

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


    


/* testStore.c ends here */
