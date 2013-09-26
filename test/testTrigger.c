/* testTrigger.c --- 
 * 
 * Filename: testTrigger.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jul  1 17:56:04 2013 (-0400)
 * Version: 
 * Last-Updated: Mon Jul  1 18:01:54 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 6
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
main(){
    int trig=300;
    int rem;
    struct timeval s;
    gettimeofday(&s, NULL);
    printf("%d %s\n", s.tv_sec, asctime(gmtime(&s.tv_sec)));
    
    rem=trig - s.tv_sec % trig;
    printf("%d\n", rem);
}


/* testTrigger.c ends here */
