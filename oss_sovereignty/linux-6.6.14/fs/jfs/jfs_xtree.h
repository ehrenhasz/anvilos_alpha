 
 
#ifndef _H_JFS_XTREE
#define _H_JFS_XTREE

 

#include "jfs_btree.h"


 
typedef struct xad {
	__u8 flag;	 
	__u8 rsvrd[2];	 
	__u8 off1;	 
	__le32 off2;	 
	pxd_t loc;	 
} xad_t;			 

#define MAXXLEN		((1 << 24) - 1)

#define XTSLOTSIZE	16
#define L2XTSLOTSIZE	4

 
#define XADoffset(xad, offset64)\
{\
	(xad)->off1 = ((u64)offset64) >> 32;\
	(xad)->off2 = __cpu_to_le32((offset64) & 0xffffffff);\
}
#define XADaddress(xad, address64) PXDaddress(&(xad)->loc, address64)
#define XADlength(xad, length32) PXDlength(&(xad)->loc, length32)

 
#define offsetXAD(xad)\
	( ((s64)((xad)->off1)) << 32 | __le32_to_cpu((xad)->off2))
#define addressXAD(xad) addressPXD(&(xad)->loc)
#define lengthXAD(xad) lengthPXD(&(xad)->loc)

 
struct xadlist {
	s16 maxnxad;
	s16 nxad;
	xad_t *xad;
};

 
#define XAD_NEW		0x01	 
#define XAD_EXTENDED	0x02	 
#define XAD_COMPRESSED	0x04	 
#define XAD_NOTRECORDED 0x08	 
#define XAD_COW		0x10	 


 
#define XTROOTINITSLOT_DIR 6
#define XTROOTINITSLOT	10
#define XTROOTMAXSLOT	18
#define XTPAGEMAXSLOT	256
#define XTENTRYSTART	2

 
typedef union {
	struct xtheader {
		__le64 next;	 
		__le64 prev;	 

		u8 flag;	 
		u8 rsrvd1;	 
		__le16 nextindex;	 
		__le16 maxentry;	 
		__le16 rsrvd2;	 

		pxd_t self;	 
	} header;		 

	xad_t xad[XTROOTMAXSLOT];	 
} xtpage_t;

 
extern int xtLookup(struct inode *ip, s64 lstart, s64 llen,
		    int *pflag, s64 * paddr, int *plen, int flag);
extern void xtInitRoot(tid_t tid, struct inode *ip);
extern int xtInsert(tid_t tid, struct inode *ip,
		    int xflag, s64 xoff, int xlen, s64 * xaddrp, int flag);
extern int xtExtend(tid_t tid, struct inode *ip, s64 xoff, int xlen,
		    int flag);
extern int xtUpdate(tid_t tid, struct inode *ip, struct xad *nxad);
extern s64 xtTruncate(tid_t tid, struct inode *ip, s64 newsize, int type);
extern s64 xtTruncate_pmap(tid_t tid, struct inode *ip, s64 committed_size);
extern int xtAppend(tid_t tid,
		    struct inode *ip, int xflag, s64 xoff, int maxblocks,
		    int *xlenp, s64 * xaddrp, int flag);
#endif				 
