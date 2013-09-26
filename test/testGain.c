/* testGain.c --- 
 * 
 * Filename: testGain.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jul  1 11:39:28 2013 (-0400)
 * Version: 
 * Last-Updated: Mon Jul  1 12:08:03 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 15
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
#define COMMA ","
int validateGain(char *s, float g[]) {
    char *tok = strtok(s, COMMA); // first call with string s ...
    char *end;
    int i;

    // defaults
    g[0]=g[1]=g[2]=g[3]=1.;

    for(i=0; i<4; i++) {
	errno=0;
	g[i]=strtof(tok, &end); 
	if(tok==end) // unable to convert to number
	    return(-1);
	tok=strtok(NULL, ",");// subsequent calls with NULL
	if(tok==NULL)
	    break;
    }
    if(i==1 || i==2) { // only 2 or 3 values - not allowed.
	return(-1);
    }
	
    if(i==0) { // only one value...
	g[1]=g[2]=g[3]=g[0]; // set all gains to first gain;
    }
    for(i=0;i<4;i++) { // and regularize gains to allowed values...
	if(g[i] < 1.0) g[i] = 0.5;
	if(g[i] >= 1.0 && g[i] < 10.) g[i]=1.0;
	if(g[i] >= 1.0 && g[i] < 10.) g[i]=1.0;
	if(g[i] >= 10.0 && g[i] < 20.) g[i]=10.0;
	if(g[i] >= 20.0 && g[i] < 30.) g[i]=20.0;
	if(g[i] >= 30.0 && g[i] < 40.) g[i]=30.0;
	if(g[i] >= 40.0 && g[i] < 60.) g[i]=40.0;
	if(g[i] >= 60.0 && g[i] < 80.) g[i]=60.0;
	if(g[i] >= 80.0 && g[i] < 119.) g[i]=80.0;
	if(g[i] >= 119.0 && g[i] < 157.) g[i]=119.0;
	if(g[i] >= 157.0) g[i]=157.0;
    }
    return(0);
}

int main() {
    char s0[] = "xyz";
    float g[4];
    printf("s=\"%s\"\n", s0);
    if(validateGain(s0, g) == -1) {
	g[0]=g[1]=g[2]=g[3]=1.;
    }
    printf("s=\"%s\", g=%f %f %f %f\n", s0, g[0], g[1], g[2], g[3]);
} 
    


/* testGain.c ends here */
