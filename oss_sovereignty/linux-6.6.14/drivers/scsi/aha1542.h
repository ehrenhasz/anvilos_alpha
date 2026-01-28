#ifndef _AHA1542_H_
#define _AHA1542_H_
#include <linux/types.h>
#define STATUS(base) base
#define STST	BIT(7)		 
#define DIAGF	BIT(6)		 
#define INIT	BIT(5)		 
#define IDLE	BIT(4)		 
#define CDF	BIT(3)		 
#define DF	BIT(2)		 
#define INVDCMD	BIT(0)		 
#define STATMASK (STST | DIAGF | INIT | IDLE | CDF | DF | INVDCMD)
#define INTRFLAGS(base) (STATUS(base)+2)
#define ANYINTR	BIT(7)		 
#define SCRD	BIT(3)		 
#define HACC	BIT(2)		 
#define MBOA	BIT(1)		 
#define MBIF	BIT(0)		 
#define INTRMASK (ANYINTR | SCRD | HACC | MBOA | MBIF)
#define CONTROL(base) STATUS(base)
#define HRST	BIT(7)		 
#define SRST	BIT(6)		 
#define IRST	BIT(5)		 
#define SCRST	BIT(4)		 
#define DATA(base) (STATUS(base)+1)
#define CMD_NOP		0x00	 
#define CMD_MBINIT	0x01	 
#define CMD_START_SCSI	0x02	 
#define CMD_INQUIRY	0x04	 
#define CMD_EMBOI	0x05	 
#define CMD_BUSON_TIME	0x07	 
#define CMD_BUSOFF_TIME	0x08	 
#define CMD_DMASPEED	0x09	 
#define CMD_RETDEVS	0x0a	 
#define CMD_RETCONF	0x0b	 
#define CMD_RETSETUP	0x0d	 
#define CMD_ECHO	0x1f	 
#define CMD_EXTBIOS     0x28     
#define CMD_MBENABLE    0x29     
struct mailbox {
	u8 status;	 
	u8 ccbptr[3];	 
};
struct chain {
	u8 datalen[3];	 
	u8 dataptr[3];	 
};
static inline void any2scsi(u8 *p, u32 v)
{
	p[0] = v >> 16;
	p[1] = v >> 8;
	p[2] = v;
}
#define scsi2int(up) ( (((long)*(up)) << 16) + (((long)(up)[1]) << 8) + ((long)(up)[2]) )
#define xscsi2int(up) ( (((long)(up)[0]) << 24) + (((long)(up)[1]) << 16) \
		      + (((long)(up)[2]) <<  8) +  ((long)(up)[3]) )
#define MAX_CDB 12
#define MAX_SENSE 14
struct ccb {
	u8 op;		 
	u8 idlun;	 
	u8 cdblen;	 
	u8 rsalen;	 
	u8 datalen[3];	 
	u8 dataptr[3];	 
	u8 linkptr[3];	 
	u8 commlinkid;	 
	u8 hastat;	 
	u8 tarstat;	 
	u8 reserved[2];
	u8 cdb[MAX_CDB + MAX_SENSE];	 
};
#define AHA1542_REGION_SIZE 4
#define AHA1542_MAILBOXES 8
#endif  
