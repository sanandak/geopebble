/* testFileMap.c --- 
 * 
 * Filename: testFileMap.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jul  1 22:54:28 2013 (-0400)
 * Version: 
 * Last-Updated: Wed Jul  3 11:37:12 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 104
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
#include "su.h"
#include "par.h"
#include "header.h"
#include "segy.h"
#include "segymap.h"

char *sdoc[] = {" "};
int main() {
    char tmpNam[] = "/tmp/gp_XXXXXX";
    int i,j, trlen, hdrlen, datalen;
    int recLen=1;
    int sampRate=4;
    int ns;
    int nch=5;
    int ofd, fileSize;
    void *faddr;
    segymap *ptr[5];  // the last element is float data[], which is a
		   // so-called flexible array member

    /* open the file and map it to memory */
    if((ofd = mkostemp(tmpNam, O_CREAT|O_EXCL|O_RDWR)) == -1)
	errExit("mkostemp");
    printf("made file %s\n", tmpNam);

    ns = recLen *sampRate;

    datalen=ns *sizeof(float);

    hdrlen = 240; // fixed forever!
    trlen = hdrlen + datalen;
    fileSize = trlen * nch; // 5 ch, 240byte hdr
    if((ftruncate(ofd, fileSize) == -1))
	errExit("ftruncate");

    /* segymap is defined as a struct with the trace header plus the
       zero-length flexible array element `float data[]' at the end of
       the struct.  This is a placeholder.  I get a pointer ptr to
       that struct. */
    /* 
     * when I map my disk file to memory (faddr), and then set the
     * value of my pointer *ptr to that memory location, I can
     * populate the struct pointed at by *ptr, and it will be
     * populating the memory location faddr, which is, in turn mapped
     * to the diskfile tmpNam.
     *
     * This includes the contents of *ptr->data[], the flexible array
     * It is my responsibility to not overrun the boundaries.
     */
    /* 
     * because this is a newly created and ftruncate-ed file, memory
     * is all zeros.
     */
    faddr = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, ofd, 0);
    if(faddr == MAP_FAILED)
	errExit("mmap");

    // initialize the segy hdr
    for(i=0;i<5;i++) {
	/* the mapped memory is cast to an array of segymap structs */
	ptr[i] = (segymap *)(faddr + i*trlen);

	ptr[i]->fldr=1001;
	ptr[i]->ns=ns;
	ptr[i]->dt=1000;
    }
    
    // this is how the data are collected - all 5 ch, one samp at a
    // time...  This is where the advantage of mapping the diskfile to
    // memory comes in.  This is fast.
    for(j=0;j<ns;j++)
	for(i=0;i<5;i++)
	    ptr[i]->data[j] = (float)(i*100+j);

    /* swap bytes to make big endian */
    for(i=0;i<5;i++) {
	for(j=0;j<SU_NKEYS;j++) swaphval((segy *)ptr[i], j);
	for(j=0;j<ns;j++) swap_float_4(ptr[i]->data+j);
    }
    exit(0);
}


/* testFileMap.c ends here */
