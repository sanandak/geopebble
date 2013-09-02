/* gp_gps_struct_TEST.c --- 
 * 
 * Filename: gp_gps_struct_TEST.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Wed Jun 26 08:09:35 2013 (-0400)
 * Version: 
 * Last-Updated: Wed Jun 26 08:09:42 2013 (-0400)
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



/* gp_gps_struct_TEST.c ends here */

int loadDefaultConfig(int fd)
{
    struct ubxpkt cfg_cfg;
    char   buf[BUF_SIZE];
    unsigned int clearMask = 0x61F;
    unsigned int saveMask=0x0;
    unsigned int loadMask=0x0;
    int nwrt, doprint, i;
    
    char *pubxpkt_c;
    int   n;

    cfg_cfg.sync[0] = USYNC1;     
    cfg_cfg.sync[1] = USYNC2; 
    cfg_cfg.clsid[0] = CLSCFG;
    cfg_cfg.clsid[1] = IDCFG;
    cfg_cfg.size=12; /* with no device mask */
    cfg_cfg.payload = (char *)calloc(cfg_cfg.size, sizeof(char));
    memcpy(cfg_cfg.payload, &clearMask, sizeof(unsigned int));
    memcpy(cfg_cfg.payload+4, &saveMask, sizeof(unsigned int));
    memcpy(cfg_cfg.payload+8, &loadMask, sizeof(unsigned int));

    pubxpkt_c = (char *)calloc(cfg_cfg.size + 8, sizeof(char)); /* payload + header + cksum */
    n = ubxpktToChar(&cfg_cfg, pubxpkt_c); /* convert to char and set checksum */
    tcflush(fd, TCIOFLUSH);
    //    for(i=0;i<n;i++)
    //	printf("%02x ", pubxpkt_c+i);
    //    printf("\n");

    if((nwrt=write(fd, pubxpkt_c, n)) < 0)
	errExit("write");
    free(pubxpkt_c);

    n=0;
    while(1) {
	int gotACK;
	n=read(fd, buf, BUF_SIZE); // TODO err check and loop until 10 bytes?
	printf("ack/nak: read %d ", n);
	doprint=0;
	for(i=0;i<n;i++) {
	    if(buf[i] == 0xB5)
		doprint=1;
	    if(doprint)
		printf("%02x ", buf[i]);
	}
	printf("\n");
	doprint=0;
	//	    printf("%02x " buf+i);
	//	if((gotACK=isACK(buf, n, CLSCFG, IDCFG)) == 0) {
	//	    printf("nak\n");
	//	    break;
	//	} else if(gotACK < 0) /* neither ack nor nak keep looking */
	//	    continue;
    }
    return 0;
}

int isACK(char *buf, int n, char cls, char id) {
    int i, nack, nnak;
    int gotACK=1; /* 1=gotACK, 0=gotNAK, -1=something else */
    struct ubxpkt cfg_ack, cfg_nak;
    char   ubxpkt_ack_c[10], ubxpkt_nak_c[10];

    cfg_ack.sync[0] = USYNC1;     
    cfg_ack.sync[1] = USYNC2; 
    cfg_ack.clsid[0] = CLSCFG;
    cfg_ack.clsid[1] = IDCFG;
    cfg_ack.size=2; /* with no device mask */
    cfg_ack.payload = (char *)calloc(cfg_ack.size, sizeof(char));
    cfg_ack.payload[0] = cls;
    cfg_ack.payload[1] = id;

    nack = ubxpktToChar(&cfg_ack, &ubxpkt_ack_c); /* convert to char and set checksum */
    for(i=0; i<n; i++)
	if(buf[i] == USYNC1)
	    break;
    if(i==n) /* no sync byte found */
	return -1;

    /* sync byte found... */
    for(i;i<n;i++) {
	if(buf[i] != ubxpkt_ack_c[i]) {  /* but doesn't match ack - likely nak? */
	    gotACK=0;
	}
    }
    return gotACK;
}


/* 
 * ubxpktToChar - given pointer to struct ubxpkt pu, alloc and populate char array pu_c
 * calculate and set checksum
 * return length of pu_c
 */
int ubxpktToChar(struct ubxpkt *pu, char *pu_c) {
    int i=0;

    pu_c[i++] = pu->sync[0];
    pu_c[i++] = pu->sync[1];
    pu_c[i++] = pu->clsid[0];
    pu_c[i++] = pu->clsid[1];
    memcpy(pu_c+i, &pu->size, sizeof(short));
    i+=2;
    memcpy(pu_c+i, pu->payload, pu->size);
    i+=pu->size;
    memcpy(pu_c+i, pu->cksum, 2);
    i+=2;
    calculateUBXChecksum(pu_c, i);
    return i;
}
