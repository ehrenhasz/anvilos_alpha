

#ifndef _H_JFS_TYPES
#define	_H_JFS_TYPES



#include <linux/types.h>
#include <linux/nls.h>


typedef u16 tid_t;
typedef u16 lid_t;


struct timestruc_t {
	__le32 tv_sec;
	__le32 tv_nsec;
};



#define LEFTMOSTONE	0x80000000
#define	HIGHORDER	0x80000000u	
#define	ONES		0xffffffffu	


typedef struct {
	__le32 len_addr;
	__le32 addr2;
} pxd_t;



static inline void PXDlength(pxd_t *pxd, __u32 len)
{
	pxd->len_addr = (pxd->len_addr & cpu_to_le32(~0xffffff)) |
			cpu_to_le32(len & 0xffffff);
}

static inline void PXDaddress(pxd_t *pxd, __u64 addr)
{
	pxd->len_addr = (pxd->len_addr & cpu_to_le32(0xffffff)) |
			cpu_to_le32((addr >> 32)<<24);
	pxd->addr2 = cpu_to_le32(addr & 0xffffffff);
}


static inline __u32 lengthPXD(pxd_t *pxd)
{
	return le32_to_cpu((pxd)->len_addr) & 0xffffff;
}

static inline __u64 addressPXD(pxd_t *pxd)
{
	__u64 n = le32_to_cpu(pxd->len_addr) & ~0xffffff;
	return (n << 8) + le32_to_cpu(pxd->addr2);
}

#define MAXTREEHEIGHT 8

struct pxdlist {
	s16 maxnpxd;
	s16 npxd;
	pxd_t pxd[MAXTREEHEIGHT];
};



typedef struct {
	__u8 flag;	
	__u8 rsrvd[3];
	__le32 size;		
	pxd_t loc;		
} dxd_t;			


#define	DXD_INDEX	0x80	
#define	DXD_INLINE	0x40	
#define	DXD_EXTENT	0x20	
#define	DXD_FILE	0x10	
#define DXD_CORRUPT	0x08	


#define	DXDlength(dxd, len)	PXDlength(&(dxd)->loc, len)
#define	DXDaddress(dxd, addr)	PXDaddress(&(dxd)->loc, addr)
#define	lengthDXD(dxd)	lengthPXD(&(dxd)->loc)
#define	addressDXD(dxd)	addressPXD(&(dxd)->loc)
#define DXDsize(dxd, size32) ((dxd)->size = cpu_to_le32(size32))
#define sizeDXD(dxd)	le32_to_cpu((dxd)->size)


struct component_name {
	int namlen;
	wchar_t *name;
};



struct dasd {
	u8 thresh;		
	u8 delta;		
	u8 rsrvd1;
	u8 limit_hi;		
	__le32 limit_lo;	
	u8 rsrvd2[3];
	u8 used_hi;		
	__le32 used_lo;		
};

#define DASDLIMIT(dasdp) \
	(((u64)((dasdp)->limit_hi) << 32) + __le32_to_cpu((dasdp)->limit_lo))
#define setDASDLIMIT(dasdp, limit)\
{\
	(dasdp)->limit_hi = ((u64)limit) >> 32;\
	(dasdp)->limit_lo = __cpu_to_le32(limit);\
}
#define DASDUSED(dasdp) \
	(((u64)((dasdp)->used_hi) << 32) + __le32_to_cpu((dasdp)->used_lo))
#define setDASDUSED(dasdp, used)\
{\
	(dasdp)->used_hi = ((u64)used) >> 32;\
	(dasdp)->used_lo = __cpu_to_le32(used);\
}

#endif				
