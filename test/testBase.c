/* testGPS.c --- 
 * 
 * Filename: testGPS.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Wed Aug 14 11:41:16 2013 (-0400)
 * Version: 
 * Last-Updated: Wed Aug 14 12:15:27 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 9
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
int main() {
    int sfd;
    while(1) {
	char p1[] = "{\n\"settings\":\n{\"sample rate\" : 500\n}\n}";
	if((sfd = unixConnect(BASE_SOCK_PATH, SOCK_STREAM)) <  0) {
	    errMsg("testBase: connect");
	} else {
	    if(write(sfd, p1, strlen(p1)) != strlen(p1)) {
		errMsg("testBase: incomplete write");
	    }
	    close(sfd);
	}
	sleep(5);
    }

}
/* testGPS.c ends here */
