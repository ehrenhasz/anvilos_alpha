


#ifndef _SYS_DKLABEL_H
#define	_SYS_DKLABEL_H



#include <sys/isa_defs.h>
#include <sys/types32.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	DKL_MAGIC	0xDABE		
#define	FKL_MAGIC	0xff		

#if defined(_SUNOS_VTOC_16)
#define	NDKMAP		16		
#define	DK_LABEL_LOC	1		
#elif defined(_SUNOS_VTOC_8)
#define	NDKMAP		8		
#define	DK_LABEL_LOC	0		
#else
#error "No VTOC format defined."
#endif

#define	LEN_DKL_ASCII	128		
#define	LEN_DKL_VVOL	8		
#define	DK_LABEL_SIZE	512		
#define	DK_MAX_BLOCKS	0x7fffffff	


#define	DK_ACYL		2




struct dk_map {
	uint64_t	dkl_cylno;	
	uint64_t	dkl_nblk;	
					
};


struct dk_map32 {
	daddr32_t	dkl_cylno;	
	daddr32_t	dkl_nblk;	
					
};


struct dk_map2 {
	uint16_t	p_tag;		
	uint16_t	p_flag;		
};

struct dkl_partition    {
	uint16_t	p_tag;		
	uint16_t	p_flag;		
	daddr32_t	p_start;	
	int32_t		p_size;		
};




struct dk_vtoc {
#if defined(_SUNOS_VTOC_16)
	uint32_t v_bootinfo[3];		
	uint32_t v_sanity;		
	uint32_t v_version;		
	char    v_volume[LEN_DKL_VVOL];	
	uint16_t v_sectorsz;		
	uint16_t v_nparts;		
	uint32_t v_reserved[10];	
	struct dkl_partition v_part[NDKMAP];	
	time32_t timestamp[NDKMAP];	
	char    v_asciilabel[LEN_DKL_ASCII];	
#elif defined(_SUNOS_VTOC_8)
	uint32_t	v_version;		
	char		v_volume[LEN_DKL_VVOL];	
	uint16_t	v_nparts;		
	struct dk_map2	v_part[NDKMAP];		
	uint32_t	v_bootinfo[3];		
	uint32_t	v_sanity;		
	uint32_t	v_reserved[10];		
	time32_t	v_timestamp[NDKMAP];	
#else
#error "No VTOC format defined."
#endif
};


#if defined(_SUNOS_VTOC_16)
#define	LEN_DKL_PAD	(DK_LABEL_SIZE - \
			    ((sizeof (struct dk_vtoc) + \
			    (4 * sizeof (uint32_t)) + \
			    (12 * sizeof (uint16_t)) + \
			    (2 * (sizeof (uint16_t))))))
#elif defined(_SUNOS_VTOC_8)
#define	LEN_DKL_PAD	(DK_LABEL_SIZE \
			    - ((LEN_DKL_ASCII) + \
			    (sizeof (struct dk_vtoc)) + \
			    (sizeof (struct dk_map32)  * NDKMAP) + \
			    (14 * (sizeof (uint16_t))) + \
			    (2 * (sizeof (uint16_t)))))
#else
#error "No VTOC format defined."
#endif


struct dk_label {
#if defined(_SUNOS_VTOC_16)
	struct  dk_vtoc dkl_vtoc;	
	uint32_t	dkl_pcyl;	
	uint32_t	dkl_ncyl;	
	uint16_t	dkl_acyl;	
	uint16_t	dkl_bcyl;	
	uint32_t	dkl_nhead;	
	uint32_t	dkl_nsect;	
	uint16_t	dkl_intrlv;	
	uint16_t	dkl_skew;	
	uint16_t	dkl_apc;	
	uint16_t	dkl_rpm;	
	uint16_t	dkl_write_reinstruct;	
	uint16_t	dkl_read_reinstruct;	
	uint16_t	dkl_extra[4];	
	char		dkl_pad[LEN_DKL_PAD];	
#elif defined(_SUNOS_VTOC_8)
	char		dkl_asciilabel[LEN_DKL_ASCII]; 
	struct dk_vtoc	dkl_vtoc;	
	uint16_t	dkl_write_reinstruct;	
	uint16_t	dkl_read_reinstruct;	
	char		dkl_pad[LEN_DKL_PAD]; 
	uint16_t	dkl_rpm;	
	uint16_t	dkl_pcyl;	
	uint16_t	dkl_apc;	
	uint16_t	dkl_obs1;	
	uint16_t	dkl_obs2;	
	uint16_t	dkl_intrlv;	
	uint16_t	dkl_ncyl;	
	uint16_t	dkl_acyl;	
	uint16_t	dkl_nhead;	
	uint16_t	dkl_nsect;	
	uint16_t	dkl_obs3;	
	uint16_t	dkl_obs4;	
	struct dk_map32	dkl_map[NDKMAP]; 
#else
#error "No VTOC format defined."
#endif
	uint16_t	dkl_magic;	
	uint16_t	dkl_cksum;	
};

#if defined(_SUNOS_VTOC_16)
#define	dkl_asciilabel	dkl_vtoc.v_asciilabel
#define	v_timestamp	timestamp

#elif defined(_SUNOS_VTOC_8)


#define	dkl_gap1	dkl_obs1	
#define	dkl_gap2	dkl_obs2	
#define	dkl_bhead	dkl_obs3	
#define	dkl_ppart	dkl_obs4	
#else
#error "No VTOC format defined."
#endif

struct fk_label {			
	uchar_t  fkl_type;
	uchar_t  fkl_magich;
	uchar_t  fkl_magicl;
	uchar_t  filler;
};


#define	DK_DEVID_BLKSIZE	(512)
#define	DK_DEVID_SIZE		(DK_DEVID_BLKSIZE - ((sizeof (uchar_t) * 7)))
#define	DK_DEVID_REV_MSB	(0)
#define	DK_DEVID_REV_LSB	(1)

struct dk_devid {
	uchar_t	dkd_rev_hi;			
	uchar_t	dkd_rev_lo;			
	uchar_t	dkd_flags;			
	uchar_t	dkd_devid[DK_DEVID_SIZE];	
	uchar_t	dkd_checksum3;			
	uchar_t	dkd_checksum2;
	uchar_t	dkd_checksum1;
	uchar_t	dkd_checksum0;			
};

#define	DKD_GETCHKSUM(dkd)	((dkd)->dkd_checksum3 << 24) + \
				((dkd)->dkd_checksum2 << 16) + \
				((dkd)->dkd_checksum1 << 8)  + \
				((dkd)->dkd_checksum0)

#define	DKD_FORMCHKSUM(c, dkd)	(dkd)->dkd_checksum3 = hibyte(hiword((c))); \
				(dkd)->dkd_checksum2 = lobyte(hiword((c))); \
				(dkd)->dkd_checksum1 = hibyte(loword((c))); \
				(dkd)->dkd_checksum0 = lobyte(loword((c)));
#ifdef	__cplusplus
}
#endif

#endif	
