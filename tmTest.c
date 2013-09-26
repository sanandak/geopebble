/* tmTest.c --- 
 * 
 * Filename: tmTest.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Wed Aug 14 21:01:48 2013 (-0400)
 * Version: 
 * Last-Updated: Wed Aug 14 21:04:25 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 1
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
    struct tm *tm, *trigtm;
    struct timeval currTime;
    
    gettimeofday(&currTime, NULL);
    tm = gmtime(&(currTime.tv_sec));
    trigtm = gmtime(&(currTime.tv_sec));
    
    printf("currtime: %ld %ld\n", currTime.tv_sec, currTime.tv_usec);
    printf("tm: %s trigtm %s\n", asctime(tm), asctime(trigtm));

    trigtm->tm_min=0;
    printf("tm: %d %d %d trigtm %d %d %d\n", tm->tm_sec, tm->tm_min,
	   tm->tm_hour, trigtm->tm_sec, trigtm->tm_min, trigtm->tm_hour);
    printf("tm: %s trigtm %s\n", asctime(tm), asctime(trigtm));
    //    printf("dmv = %d mv=%d tt=%ld dt=%d\n", dmv, mv, tt, dt);
}

    
/* tmTest.c ends here */
