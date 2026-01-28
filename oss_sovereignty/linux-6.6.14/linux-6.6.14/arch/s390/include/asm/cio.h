#ifndef _ASM_S390_CIO_H_
#define _ASM_S390_CIO_H_
#include <linux/bitops.h>
#include <linux/genalloc.h>
#include <asm/types.h>
#include <asm/tpi.h>
#define LPM_ANYPATH 0xff
#define __MAX_CSSID 0
#define __MAX_SUBCHANNEL 65535
#define __MAX_SSID 3
#include <asm/scsw.h>
struct ccw1 {
	__u8  cmd_code;
	__u8  flags;
	__u16 count;
	__u32 cda;
} __attribute__ ((packed,aligned(8)));
struct ccw0 {
	__u8 cmd_code;
	__u32 cda : 24;
	__u8  flags;
	__u8  reserved;
	__u16 count;
} __packed __aligned(8);
#define CCW_FLAG_DC		0x80
#define CCW_FLAG_CC		0x40
#define CCW_FLAG_SLI		0x20
#define CCW_FLAG_SKIP		0x10
#define CCW_FLAG_PCI		0x08
#define CCW_FLAG_IDA		0x04
#define CCW_FLAG_SUSPEND	0x02
#define CCW_CMD_READ_IPL	0x02
#define CCW_CMD_NOOP		0x03
#define CCW_CMD_BASIC_SENSE	0x04
#define CCW_CMD_TIC		0x08
#define CCW_CMD_STLCK           0x14
#define CCW_CMD_SENSE_PGID	0x34
#define CCW_CMD_SUSPEND_RECONN	0x5B
#define CCW_CMD_RDC		0x64
#define CCW_CMD_RELEASE		0x94
#define CCW_CMD_SET_PGID	0xAF
#define CCW_CMD_SENSE_ID	0xE4
#define CCW_CMD_DCTL		0xF3
#define SENSE_MAX_COUNT		0x20
struct erw {
	__u32 res0  : 3;
	__u32 auth  : 1;
	__u32 pvrf  : 1;
	__u32 cpt   : 1;
	__u32 fsavf : 1;
	__u32 cons  : 1;
	__u32 scavf : 1;
	__u32 fsaf  : 1;
	__u32 scnt  : 6;
	__u32 res16 : 16;
} __attribute__ ((packed));
struct erw_eadm {
	__u32 : 16;
	__u32 b : 1;
	__u32 r : 1;
	__u32  : 14;
} __packed;
struct sublog {
	__u32 res0  : 1;
	__u32 esf   : 7;
	__u32 lpum  : 8;
	__u32 arep  : 1;
	__u32 fvf   : 5;
	__u32 sacc  : 2;
	__u32 termc : 2;
	__u32 devsc : 1;
	__u32 serr  : 1;
	__u32 ioerr : 1;
	__u32 seqc  : 3;
} __attribute__ ((packed));
struct esw0 {
	struct sublog sublog;
	struct erw erw;
	__u32  faddr[2];
	__u32  saddr;
} __attribute__ ((packed));
struct esw1 {
	__u8  zero0;
	__u8  lpum;
	__u16 zero16;
	struct erw erw;
	__u32 zeros[3];
} __attribute__ ((packed));
struct esw2 {
	__u8  zero0;
	__u8  lpum;
	__u16 dcti;
	struct erw erw;
	__u32 zeros[3];
} __attribute__ ((packed));
struct esw3 {
	__u8  zero0;
	__u8  lpum;
	__u16 res;
	struct erw erw;
	__u32 zeros[3];
} __attribute__ ((packed));
struct esw_eadm {
	__u32 sublog;
	struct erw_eadm erw;
	__u32 : 32;
	__u32 : 32;
	__u32 : 32;
} __packed;
struct irb {
	union scsw scsw;
	union {
		struct esw0 esw0;
		struct esw1 esw1;
		struct esw2 esw2;
		struct esw3 esw3;
		struct esw_eadm eadm;
	} esw;
	__u8   ecw[32];
} __attribute__ ((packed,aligned(4)));
struct ciw {
	__u32 et       :  2;
	__u32 reserved :  2;
	__u32 ct       :  4;
	__u32 cmd      :  8;
	__u32 count    : 16;
} __attribute__ ((packed));
#define CIW_TYPE_RCD	0x0    	 
#define CIW_TYPE_SII	0x1    	 
#define CIW_TYPE_RNI	0x2    	 
#define ND_VALIDITY_VALID	0
#define ND_VALIDITY_OUTDATED	1
#define ND_VALIDITY_INVALID	2
struct node_descriptor {
	union {
		struct {
			u32 validity:3;
			u32 reserved:5;
		} __packed;
		u8 byte0;
	} __packed;
	u32 params:24;
	char type[6];
	char model[3];
	char manufacturer[3];
	char plant[2];
	char seq[12];
	u16 tag;
} __packed;
#define DOIO_ALLOW_SUSPEND	 0x0001  
#define DOIO_DENY_PREFETCH	 0x0002  
#define DOIO_SUPPRESS_INTER	 0x0004  
#define CIO_GONE       0x0001
#define CIO_NO_PATH    0x0002
#define CIO_OPER       0x0004
#define CIO_REVALIDATE 0x0008
#define CIO_BOXED      0x0010
struct ccw_dev_id {
	u8 ssid;
	u16 devno;
};
static inline int ccw_dev_id_is_equal(struct ccw_dev_id *dev_id1,
				      struct ccw_dev_id *dev_id2)
{
	if ((dev_id1->ssid == dev_id2->ssid) &&
	    (dev_id1->devno == dev_id2->devno))
		return 1;
	return 0;
}
static inline u8 pathmask_to_pos(u8 mask)
{
	return 8 - ffs(mask);
}
extern void css_schedule_reprobe(void);
extern void *cio_dma_zalloc(size_t size);
extern void cio_dma_free(void *cpu_addr, size_t size);
extern struct device *cio_get_dma_css_dev(void);
void *cio_gp_dma_zalloc(struct gen_pool *gp_dma, struct device *dma_dev,
			size_t size);
void cio_gp_dma_free(struct gen_pool *gp_dma, void *cpu_addr, size_t size);
void cio_gp_dma_destroy(struct gen_pool *gp_dma, struct device *dma_dev);
struct gen_pool *cio_gp_dma_create(struct device *dma_dev, int nr_pages);
int chsc_sstpc(void *page, unsigned int op, u16 ctrl, long *clock_delta);
int chsc_sstpi(void *page, void *result, size_t size);
int chsc_stzi(void *page, void *result, size_t size);
int chsc_sgib(u32 origin);
int chsc_scud(u16 cu, u64 *esm, u8 *esm_valid);
#endif
