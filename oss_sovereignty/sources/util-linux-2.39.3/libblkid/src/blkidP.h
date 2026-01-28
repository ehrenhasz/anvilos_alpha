

#ifndef _BLKID_BLKIDP_H
#define _BLKID_BLKIDP_H




#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#ifndef UUID_STR_LEN
# define UUID_STR_LEN   37
#endif

#include "c.h"
#include "bitops.h"	
#include "blkdev.h"

#include "debug.h"
#include "blkid.h"
#include "list.h"
#include "encode.h"


struct blkid_struct_dev
{
	struct list_head	bid_devs;	
	struct list_head	bid_tags;	
	blkid_cache		bid_cache;	
	char			*bid_name;	
	char			*bid_xname;	
	char			*bid_type;	
	int			bid_pri;	
	dev_t			bid_devno;	
	time_t			bid_time;	
	suseconds_t		bid_utime;	
	unsigned int		bid_flags;	
	char			*bid_label;	
	char			*bid_uuid;	
};

#define BLKID_BID_FL_VERIFIED	0x0001	
#define BLKID_BID_FL_INVALID	0x0004	
#define BLKID_BID_FL_REMOVABLE	0x0008	


struct blkid_struct_tag
{
	struct list_head	bit_tags;	
	struct list_head	bit_names;	
	char			*bit_name;	
	char			*bit_val;	
	blkid_dev		bit_dev;	
};
typedef struct blkid_struct_tag *blkid_tag;


enum {
	BLKID_CHAIN_SUBLKS,	
	BLKID_CHAIN_TOPLGY,	
	BLKID_CHAIN_PARTS,	

	BLKID_NCHAINS		
};

struct blkid_chain {
	const struct blkid_chaindrv *driver;	

	int		enabled;	
	int		flags;		
	int		binary;		
	int		idx;		
	unsigned long	*fltr;		
	void		*data;		
};


struct blkid_chaindrv {
	const size_t	id;		
	const char	*name;		
	const int	dflt_flags;	
	const int	dflt_enabled;	
	int		has_fltr;	

	const struct blkid_idinfo **idinfos; 
	const size_t	nidinfos;	

	
	int		(*probe)(blkid_probe, struct blkid_chain *);
	int		(*safeprobe)(blkid_probe, struct blkid_chain *);
	void		(*free_data)(blkid_probe, void *);
};


extern const struct blkid_chaindrv superblocks_drv;
extern const struct blkid_chaindrv topology_drv;
extern const struct blkid_chaindrv partitions_drv;


struct blkid_prval
{
	const char	*name;		
	unsigned char	*data;		
	size_t		len;		

	struct blkid_chain	*chain;		
	struct list_head	prvals;		
};


struct blkid_idmag
{
	const char	*magic;		
	unsigned int	len;		

	const char	*hoff;		
	long		kboff;		
	unsigned int	sboff;		

	int		is_zoned;	
	long		zonenum;	
	long		kboff_inzone;	
};


struct blkid_idinfo
{
	const char	*name;		
	int		usage;		
	int		flags;		
	int		minsz;		

					
	int		(*probefunc)(blkid_probe pr, const struct blkid_idmag *mag);

	struct blkid_idmag	magics[];	
};

#define BLKID_NONE_MAGIC	{{ NULL }}


#define BLKID_IDINFO_TOLERANT	(1 << 1)

struct blkid_bufinfo {
	unsigned char		*data;
	uint64_t		off;
	uint64_t		len;
	struct list_head	bufs;	
};


struct blkid_hint {
	char			*name;
	uint64_t		value;
	struct list_head	hints;
};


struct blkid_struct_probe
{
	int			fd;		
	uint64_t		off;		
	uint64_t		size;		

	dev_t			devno;		
	dev_t			disk_devno;	
	unsigned int		blkssz;		
	mode_t			mode;		
	uint64_t		zone_size;	

	int			flags;		
	int			prob_flags;	

	uint64_t		wipe_off;	
	uint64_t		wipe_size;	
	struct blkid_chain	*wipe_chain;	

	struct list_head	buffers;	
	struct list_head	hints;

	struct blkid_chain	chains[BLKID_NCHAINS];	
	struct blkid_chain	*cur_chain;		

	struct list_head	values;		

	struct blkid_struct_probe *parent;	
	struct blkid_struct_probe *disk_probe;	
};


#define BLKID_FL_PRIVATE_FD	(1 << 1)	
#define BLKID_FL_TINY_DEV	(1 << 2)	
#define BLKID_FL_CDROM_DEV	(1 << 3)	
#define BLKID_FL_NOSCAN_DEV	(1 << 4)	
#define BLKID_FL_MODIF_BUFF	(1 << 5)	
#define BLKID_FL_OPAL_LOCKED	(1 << 6)	


#define BLKID_PROBE_FL_IGNORE_PT (1 << 1)	

extern blkid_probe blkid_clone_probe(blkid_probe parent);
extern blkid_probe blkid_probe_get_wholedisk_probe(blkid_probe pr);


enum {
	BLKID_EVAL_UDEV = 0,
	BLKID_EVAL_SCAN,

	__BLKID_EVAL_LAST
};


struct blkid_config {
	int eval[__BLKID_EVAL_LAST];	
	int nevals;			
	int uevent;			
	char *cachefile;		
};

extern struct blkid_config *blkid_read_config(const char *filename)
			__ul_attribute__((warn_unused_result));
extern void blkid_free_config(struct blkid_config *conf);


#define BLKID_PROBE_MIN		2


#define BLKID_PROBE_INTERVAL	200


struct blkid_struct_cache
{
	struct list_head	bic_devs;	
	struct list_head	bic_tags;	
	time_t			bic_time;	
	time_t			bic_ftime;	
	unsigned int		bic_flags;	
	char			*bic_filename;	
	blkid_probe		probe;		
};

#define BLKID_BIC_FL_PROBED	0x0002	
#define BLKID_BIC_FL_CHANGED	0x0004	


#define BLKID_CONFIG_FILE	"/etc/blkid.conf"


#define BLKID_RUNTIME_TOPDIR	"/run"
#define BLKID_RUNTIME_DIR	BLKID_RUNTIME_TOPDIR "/blkid"
#define BLKID_CACHE_FILE	BLKID_RUNTIME_DIR "/blkid.tab"


#define BLKID_CACHE_FILE_OLD	"/etc/blkid.tab"

#define BLKID_ERR_IO	 5
#define BLKID_ERR_SYSFS	 9
#define BLKID_ERR_MEM	12
#define BLKID_ERR_CACHE	14
#define BLKID_ERR_DEV	19
#define BLKID_ERR_PARAM	22
#define BLKID_ERR_BIG	27


#define BLKID_PRI_UBI	50
#define BLKID_PRI_DM	40
#define BLKID_PRI_EVMS	30
#define BLKID_PRI_LVM	20
#define BLKID_PRI_MD	10

#define BLKID_DEBUG_HELP	(1 << 0)
#define BLKID_DEBUG_INIT	(1 << 1)
#define BLKID_DEBUG_CACHE	(1 << 2)
#define BLKID_DEBUG_CONFIG	(1 << 3)
#define BLKID_DEBUG_DEV		(1 << 4)
#define BLKID_DEBUG_DEVNAME	(1 << 5)
#define BLKID_DEBUG_DEVNO	(1 << 6)
#define BLKID_DEBUG_EVALUATE	(1 << 7)
#define BLKID_DEBUG_LOWPROBE	(1 << 8)
#define BLKID_DEBUG_PROBE	(1 << 9)
#define BLKID_DEBUG_READ	(1 << 10)
#define BLKID_DEBUG_SAVE	(1 << 11)
#define BLKID_DEBUG_TAG		(1 << 12)
#define BLKID_DEBUG_BUFFER	(1 << 13)
#define BLKID_DEBUG_ALL		0xFFFF		

UL_DEBUG_DECLARE_MASK(libblkid);
#define DBG(m, x)	__UL_DBG(libblkid, BLKID_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(libblkid, BLKID_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(libblkid)
#include "debugobj.h"

extern void blkid_debug_dump_dev(blkid_dev dev);



struct dir_list {
	char	*name;
	struct dir_list *next;
};
extern void blkid__scan_dir(char *, dev_t, struct dir_list **, char **)
			__attribute__((nonnull(1,4)));
extern int blkid_driver_has_major(const char *drvname, int drvmaj)
			__attribute__((warn_unused_result));


extern void blkid_read_cache(blkid_cache cache)
			__attribute__((nonnull));


extern int blkid_flush_cache(blkid_cache cache)
			__attribute__((nonnull));


extern char *blkid_safe_getenv(const char *arg)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern char *blkid_get_cache_filename(struct blkid_config *conf)
			__attribute__((warn_unused_result));

extern void blkid_free_tag(blkid_tag tag);
extern blkid_tag blkid_find_tag_dev(blkid_dev dev, const char *type)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int blkid_set_tag(blkid_dev dev, const char *name,
			 const char *value, const int vlength)
			__attribute__((nonnull(1,2)));


extern blkid_dev blkid_new_dev(void)
			__attribute__((warn_unused_result));
extern void blkid_free_dev(blkid_dev dev);


extern int blkid_probe_is_tiny(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));
extern int blkid_probe_is_cdrom(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));
extern int blkdid_probe_is_opal_locked(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern unsigned char *blkid_probe_get_buffer(blkid_probe pr,
                                uint64_t off, uint64_t len)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern unsigned char *blkid_probe_get_sector(blkid_probe pr, unsigned int sector)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int blkid_probe_get_dimension(blkid_probe pr,
	                uint64_t *off, uint64_t *size)
			__attribute__((nonnull));

extern int blkid_probe_set_dimension(blkid_probe pr,
	                uint64_t off, uint64_t size)
			__attribute__((nonnull));

extern int blkid_probe_get_idmag(blkid_probe pr, const struct blkid_idinfo *id,
			uint64_t *offset, const struct blkid_idmag **res)
			__attribute__((nonnull(1)));


extern unsigned char *blkid_probe_get_sb_buffer(blkid_probe pr, const struct blkid_idmag *mag, size_t size);
#define blkid_probe_get_sb(_pr, _mag, type) \
			((type *) blkid_probe_get_sb_buffer((_pr), _mag, sizeof(type)))

extern blkid_partlist blkid_probe_get_partlist(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int blkid_probe_is_covered_by_pt(blkid_probe pr,
					uint64_t offset, uint64_t size)
			__attribute__((warn_unused_result));

extern void blkid_probe_chain_reset_values(blkid_probe pr, struct blkid_chain *chn)
			__attribute__((nonnull));
extern int blkid_probe_chain_save_values(blkid_probe pr,
				       struct blkid_chain *chn,
			               struct list_head *vals)
			__attribute__((nonnull));

extern struct blkid_prval *blkid_probe_assign_value(blkid_probe pr,
					const char *name)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern void blkid_probe_free_value(struct blkid_prval *v);


extern void blkid_probe_append_values_list(blkid_probe pr,
				    struct list_head *vals)
			__attribute__((nonnull));

extern void blkid_probe_free_values_list(struct list_head *vals);

extern struct blkid_chain *blkid_probe_get_chain(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern struct blkid_prval *__blkid_probe_get_value(blkid_probe pr, int num)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern struct blkid_prval *__blkid_probe_lookup_value(blkid_probe pr, const char *name)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern unsigned long *blkid_probe_get_filter(blkid_probe pr, int chain, int create)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int __blkid_probe_invert_filter(blkid_probe pr, int chain)
			__attribute__((nonnull));
extern int __blkid_probe_reset_filter(blkid_probe pr, int chain)
			__attribute__((nonnull));
extern int __blkid_probe_filter_types(blkid_probe pr, int chain, int flag, char *names[])
			__attribute__((nonnull));

extern void *blkid_probe_get_binary_data(blkid_probe pr, struct blkid_chain *chn)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern struct blkid_prval *blkid_probe_new_val(void)
			__attribute__((warn_unused_result));
extern int blkid_probe_set_value(blkid_probe pr, const char *name,
				const unsigned char *data, size_t len)
			__attribute__((nonnull));
extern int blkid_probe_value_set_data(struct blkid_prval *v,
				const unsigned char *data, size_t len)
			__attribute__((nonnull));

extern int blkid_probe_vsprintf_value(blkid_probe pr, const char *name,
				const char *fmt, va_list ap)
			__attribute__((nonnull));

extern int blkid_probe_sprintf_value(blkid_probe pr, const char *name,
				const char *fmt, ...)
			__attribute__((nonnull))
			__attribute__ ((__format__ (__printf__, 3, 4)));

extern int blkid_probe_set_magic(blkid_probe pr, uint64_t offset,
				size_t len, const unsigned char *magic)
			__attribute__((nonnull));

extern int blkid_probe_verify_csum(blkid_probe pr, uint64_t csum, uint64_t expected)
			__attribute__((nonnull));
extern int blkid_probe_verify_csum_buf(blkid_probe pr, size_t n, const void *csum,
		const void *expected) __attribute__((nonnull));

extern void blkid_unparse_uuid(const unsigned char *uuid, char *str, size_t len)
			__attribute__((nonnull));
extern int blkid_uuid_is_empty(const unsigned char *buf, size_t len);

extern size_t blkid_rtrim_whitespace(unsigned char *str)
			__attribute__((nonnull));
extern size_t blkid_ltrim_whitespace(unsigned char *str)
			__attribute__((nonnull));

extern void blkid_probe_set_wiper(blkid_probe pr, uint64_t off,
				  uint64_t size)
			__attribute__((nonnull));
extern int blkid_probe_is_wiped(blkid_probe pr, struct blkid_chain **chn,
		                uint64_t off, uint64_t size)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));
extern void blkid_probe_use_wiper(blkid_probe pr, uint64_t off, uint64_t size)
			__attribute__((nonnull));

extern int blkid_probe_get_hint(blkid_probe pr, const char *name, uint64_t *value)
			__attribute__((nonnull(1,2)))
			__attribute__((warn_unused_result));

extern int blkid_probe_get_partitions_flags(blkid_probe pr)
			__attribute__((nonnull));


#define blkid_bmp_wordsize		(8 * sizeof(unsigned long))
#define blkid_bmp_idx_bit(item)		(1UL << ((item) % blkid_bmp_wordsize))
#define blkid_bmp_idx_byte(item)	((item) / blkid_bmp_wordsize)

#define blkid_bmp_set_item(bmp, item)	\
		((bmp)[ blkid_bmp_idx_byte(item) ] |= blkid_bmp_idx_bit(item))

#define blkid_bmp_unset_item(bmp, item)	\
		((bmp)[ blkid_bmp_idx_byte(item) ] &= ~blkid_bmp_idx_bit(item))

#define blkid_bmp_get_item(bmp, item)	\
		((bmp)[ blkid_bmp_idx_byte(item) ] & blkid_bmp_idx_bit(item))

#define blkid_bmp_nwords(max_items) \
		(((max_items) + blkid_bmp_wordsize) / blkid_bmp_wordsize)

#define blkid_bmp_nbytes(max_items) \
		(blkid_bmp_nwords(max_items) * sizeof(unsigned long))

#endif 
