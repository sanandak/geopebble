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

int main() {
    struct gps_st gpsStat;
    while(1) {
	int ret = get_gpsStatus(&gpsStat);
	printf("ret=%d health=%d tv-sec=%ld tv-usec=%ld\n", ret, gpsStat.health, 
	       gpsStat.currInternalTime.tv_sec, 
	       gpsStat.currInternalTime.tv_usec);
	sleep(5);
    }

}
/* testGPS.c ends here */
