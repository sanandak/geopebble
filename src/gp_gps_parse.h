/* gp_gps_parse.h --- 
 * 
 * Filename: gp_gps_parse.h
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Jul  8 07:01:31 2013 (-0400)
 * Version: 
 * Last-Updated: Mon Aug 12 08:08:51 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 12
 * 
 */


/* Defines. */

#ifndef _GP_GPS_PARSE_H
#define _GP_GPS_PARSE_H

#define LEAPSEC 16 

/* Unix timestamp of the GPS epoch 1980-01-06 00:00:00 UTC */
#define GPS_EPOCH 315964800

#define USYNC1 0xB5
#define USYNC2 0x62

#define CLSNAV 0x01
#define IDNAVPOSECEF 0x01
#define IDNAVPOSLLH  0x02
#define IDNAVSTATUS  0x03
#define IDNAVDOP     0x04
#define IDNAVTIMEGPS 0x20
#define NAVTIMEGPS_VALIDTOW BIT0
#define NAVTIMEGPS_VALIDWKN BIT1
#define NAVTIMEGPS_VALIDUTC BIT2
#define IDNAVTIMEUTC 0x21
// flags for TIMEUTC
#define NAVTIMEUTC_VALIDTOW BIT0
#define NAVTIMEUTC_VALIDWKN BIT1
#define NAVTIMEUTC_VALIDUTC BIT2
#define IDNAVSVINFO  0x30

#define CLSRXM 0x02
#define IDRXMRAW     0x10 
#define IDRXMSFRB    0x11 // subframe

#define CLSINF 0x04

#define CLSACK 0x05
#define IDACKACK     0x01
#define IDACKNAK     0x00

#define CLSCFG 0x06
#define IDCFGCFG     0x09 /* pg. 106, length 12 (no non-volatile storage) or
			     13 with */
#define IDCFGMSG     0x01

#define CLSMON 0x0A
#define IDMONVER     0x04

#define CLSAID 0x0B
#define CLSTIM 0x0D
#define CLSESF 0x10

#define GPSBUFSIZ 4096
#define MAXPAYLOADLEN 512 // max payload is 16+24*nsv ~ 400

/* enums */
enum states {
    WAIT_FOR_HEADER,
    WAIT_FOR_CLASS,
    WAIT_FOR_SIZE,
    WAIT_FOR_PAYLOAD,
    WAIT_FOR_CKSUM,
    CHECK_CKSUM,
    INTERPRET_UBXPKT
};


enum pktTypes {
    POSLLH,
    TIMEGPS,
    TIMEUTC,
    STATUS,
    SVINFO
};

/* structs */
/* ubxpkt_st has a zero length array at the end.  This allows variable
   size payloads, but the length of the payload has to be in size */
struct ubxpkt_st {
    char sync[2];
    char clsid[2];
    short size;
    char cksum[2];
    char payload[]; // zero length array element
};

struct timeutc_st {
    unsigned int iTOW;
    unsigned int tAcc;
    int          nano;
    unsigned short year;
    unsigned char month;
    unsigned char day;
    unsigned char hour;
    unsigned char min;
    unsigned char sec;
    char          valid;
};

struct posllh_st {
    unsigned int iTOW;
    int longitude;
    int latitude;
    int height;
    unsigned int hAcc;
    unsigned int vAcc;
};

struct timegps_st {
    unsigned int iTOW;
    int fTOW;
    short week;
    char nleapS;
    char valid;
    unsigned int tAcc;
};

struct status_st {
    unsigned int iTOW;
    unsigned char gpsFix;
    unsigned char flags;
    unsigned char fixStat;
    unsigned char flags2;
    unsigned int ttff;
    unsigned int msss;
};

#define MAXSVS 24
struct sv {
    unsigned char ch;
    unsigned char svid;
    unsigned char flags;
    unsigned char quality;
    unsigned char cno;
    char elev;
    short azim;
    int prRes;
};
struct svinfo_st {
    unsigned int iTOW;
    unsigned char numCh;
    unsigned char globalFlags;
    struct sv svs[MAXSVS];
};

/* Function declarations */
int txMsg(char *msg, int len, int fd, int ntry);
int pollMsg(char *msg, int len, int fd, int ntry);
int getGPSData(int tout, char *buf, int buflen);
struct ubxpkt_st *getNextPacket(char *buf, int *pbuflen, int *reset);
int getTIMEUTC(struct ubxpkt_st *u, struct timeutc_st *t);
int getTIMEGPS(struct ubxpkt_st *u, struct timegps_st *tgps);

#endif

/* gp_gps_parse.h ends here */
