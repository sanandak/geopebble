/* testValCh.c --- 
 * 
 * Filename: testValCh.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jul  1 12:15:18 2013 (-0400)
 * Version: 
 * Last-Updated: Mon Jul  1 12:41:18 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 11
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
#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#include "geopebble.h"
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
}
int main() {
    char s[] = "15";
    int ch[4];
    int ret;
    ret=validateChannels(s, ch);
    if(ret==-1)
	printf("error parsing %s\n", s);
    else
	printf("s=%s ch=%d %d %d %d\n", s, ch[0], ch[1], ch[2], ch[3]);
    return (0);
}


/* testValCh.c ends here */
