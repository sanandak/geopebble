/* testGPS.c --- 
 * 
 * Filename: testGPS.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Wed Aug 14 11:41:16 2013 (-0400)
 * Version: 
 * Last-Updated: Wed Aug 14 17:54:58 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 12
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
#include "unix_sockets.h"

int get_gpsStatus(struct gps_st *gpsStatp) {
    int sfd;
    char buf[BUF_SIZE];

    /* read until the server closes the socket */
    if((sfd = unixConnect(GPS_SOCK_PATH, SOCK_STREAM)) <0) {
	errMsg("get_gpsStatus: connect");
	return(-1);
    }

    int indx=0;
    int nread;

    while((nread = read(sfd, buf+indx, BUF_SIZE-indx)) > 0)
	indx+=nread;

    close(sfd);
    memcpy(gpsStatp, buf, sizeof(struct gps_st));

    return(0);
}
/* testGPS.c ends here */
