#ifndef _SYS_DKIO_H
#define	_SYS_DKIO_H
#include <sys/dklabel.h>	 
#ifdef	__cplusplus
extern "C" {
#endif
#define	DK_DEVLEN	16		 
struct dk_cinfo {
	char	dki_cname[DK_DEVLEN];	 
	ushort_t dki_ctype;		 
	ushort_t dki_flags;		 
	ushort_t dki_cnum;		 
	uint_t	dki_addr;		 
	uint_t	dki_space;		 
	uint_t	dki_prio;		 
	uint_t	dki_vec;		 
	char	dki_dname[DK_DEVLEN];	 
	uint_t	dki_unit;		 
	ushort_t dki_partition;		 
	ushort_t dki_maxtransfer;	 
};
#define	DKC_UNKNOWN	0
#define	DKC_CDROM	1	 
#define	DKC_WDC2880	2
#define	DKC_XXX_0	3	 
#define	DKC_XXX_1	4	 
#define	DKC_DSD5215	5
#define	DKC_ACB4000	7
#define	DKC_MD21	8
#define	DKC_XXX_2	9	 
#define	DKC_NCRFLOPPY	10
#define	DKC_SMSFLOPPY	12
#define	DKC_SCSI_CCS	13	 
#define	DKC_INTEL82072	14	 
#define	DKC_MD		16	 
#define	DKC_INTEL82077	19	 
#define	DKC_DIRECT	20	 
#define	DKC_PCMCIA_MEM	21	 
#define	DKC_PCMCIA_ATA	22	 
#define	DKC_VBD		23	 
#define	DKC_CUSTOMER_BASE	1024
#define	DKI_BAD144	0x01	 
#define	DKI_MAPTRK	0x02	 
#define	DKI_FMTTRK	0x04	 
#define	DKI_FMTVOL	0x08	 
#define	DKI_FMTCYL	0x10	 
#define	DKI_HEXUNIT	0x20	 
#define	DKI_PCMCIA_PFD	0x40	 
struct dk_allmap {
	struct dk_map	dka_map[NDKMAP];
};
#if defined(_SYSCALL32)
struct dk_allmap32 {
	struct dk_map32	dka_map[NDKMAP];
};
#endif  
struct dk_geom {
	unsigned short	dkg_ncyl;	 
	unsigned short	dkg_acyl;	 
	unsigned short	dkg_bcyl;	 
	unsigned short	dkg_nhead;	 
	unsigned short	dkg_obs1;	 
	unsigned short	dkg_nsect;	 
	unsigned short	dkg_intrlv;	 
	unsigned short	dkg_obs2;	 
	unsigned short	dkg_obs3;	 
	unsigned short	dkg_apc;	 
	unsigned short	dkg_rpm;	 
	unsigned short	dkg_pcyl;	 
	unsigned short	dkg_write_reinstruct;	 
	unsigned short	dkg_read_reinstruct;	 
	unsigned short	dkg_extra[7];	 
};
#define	dkg_bhead	dkg_obs1	 
#define	dkg_gap1	dkg_obs2	 
#define	dkg_gap2	dkg_obs3	 
#define	DKIOC		(0x04 << 8)
#define	DKIOCGGEOM	(DKIOC|1)		 
#define	DKIOCINFO	(DKIOC|3)		 
#define	DKIOCEJECT	(DKIOC|6)		 
#define	DKIOCGVTOC	(DKIOC|11)		 
#define	DKIOCSVTOC	(DKIOC|12)		 
#define	DKIOCFLUSHWRITECACHE	(DKIOC|34)	 
struct dk_callback {
	void (*dkc_callback)(void *dkc_cookie, int error);
	void *dkc_cookie;
	int dkc_flag;
};
#define	FLUSH_VOLATILE		0x1	 
#define	DKIOCGETWCE		(DKIOC|36)	 
#define	DKIOCSETWCE		(DKIOC|37)	 
#define	DKIOCSGEOM	(DKIOC|2)		 
#define	DKIOCSAPART	(DKIOC|4)		 
#define	DKIOCGAPART	(DKIOC|5)		 
#define	DKIOCG_PHYGEOM	(DKIOC|32)		 
#define	DKIOCG_VIRTGEOM	(DKIOC|33)		 
#define	DKIOCLOCK	(DKIOC|7)	 
#define	DKIOCUNLOCK	(DKIOC|8)	 
#define	DKIOCSTATE	(DKIOC|13)	 
#define	DKIOCREMOVABLE	(DKIOC|16)	 
#define	DKIOCHOTPLUGGABLE	(DKIOC|35)	 
#define	DKIOCADDBAD	(DKIOC|20)	 
#define	DKIOCGETDEF	(DKIOC|21)	 
#ifdef _SYSCALL32
struct defect_header32 {
	int		head;
	caddr32_t	buffer;
};
#endif  
struct defect_header {
	int		head;
	caddr_t		buffer;
};
#define	DKIOCPARTINFO	(DKIOC|22)	 
#ifdef _SYSCALL32
struct part_info32 {
	uint32_t	p_start;
	int		p_length;
};
#endif  
struct part_info {
	uint64_t	p_start;
	int		p_length;
};
#define	DKIOC_EBP_ENABLE  (DKIOC|40)	 
#define	DKIOC_EBP_DISABLE (DKIOC|41)	 
enum dkio_state { DKIO_NONE, DKIO_EJECTED, DKIO_INSERTED, DKIO_DEV_GONE };
#define	DKIOCGMEDIAINFO	(DKIOC|42)	 
#define	DKIOCGMBOOT	(DKIOC|43)	 
#define	DKIOCSMBOOT	(DKIOC|44)	 
#define	DKIOCGTEMPERATURE	(DKIOC|45)	 
struct	dk_temperature	{
	uint_t		dkt_flags;	 
	short		dkt_cur_temp;	 
	short		dkt_ref_temp;	 
};
#define	DKT_BYPASS_PM		0x1
#define	DKT_INVALID_TEMP	0xFFFF
struct dk_minfo {
	uint_t		dki_media_type;	 
	uint_t		dki_lbsize;	 
	diskaddr_t	dki_capacity;	 
};
#define	DK_UNKNOWN		0x00	 
#define	DK_REMOVABLE_DISK	0x02  
#define	DK_MO_ERASABLE		0x03  
#define	DK_MO_WRITEONCE		0x04  
#define	DK_AS_MO		0x05  
#define	DK_CDROM		0x08  
#define	DK_CDR			0x09  
#define	DK_CDRW			0x0A  
#define	DK_DVDROM		0x10  
#define	DK_DVDR			0x11  
#define	DK_DVDRAM		0x12  
#define	DK_FIXED_DISK		0x10001	 
#define	DK_FLOPPY		0x10002  
#define	DK_ZIP			0x10003  
#define	DK_JAZ			0x10004  
#define	DKIOCSETEFI	(DKIOC|17)		 
#define	DKIOCGETEFI	(DKIOC|18)		 
#define	DKIOCPARTITION	(DKIOC|9)		 
#define	DKIOCGETVOLCAP	(DKIOC | 25)	 
#define	DKIOCSETVOLCAP	(DKIOC | 26)	 
#define	DKIOCDMR	(DKIOC | 27)	 
typedef uint_t volcapinfo_t;
typedef uint_t volcapset_t;
#define	DKV_ABR_CAP 0x00000001		 
#define	DKV_DMR_CAP 0x00000002		 
typedef struct volcap {
	volcapinfo_t vc_info;	 
	volcapset_t vc_set;	 
} volcap_t;
#define	VOL_SIDENAME 256
typedef struct vol_directed_rd {
	int		vdr_flags;
	offset_t	vdr_offset;
	size_t		vdr_nbytes;
	size_t		vdr_bytesread;
	void		*vdr_data;
	int		vdr_side;
	char		vdr_side_name[VOL_SIDENAME];
} vol_directed_rd_t;
#define	DKV_SIDE_INIT		(-1)
#define	DKV_DMR_NEXT_SIDE	0x00000001
#define	DKV_DMR_DONE		0x00000002
#define	DKV_DMR_ERROR		0x00000004
#define	DKV_DMR_SUCCESS		0x00000008
#define	DKV_DMR_SHORT		0x00000010
#ifdef _MULTI_DATAMODEL
#if _LONG_LONG_ALIGNMENT == 8 && _LONG_LONG_ALIGNMENT_32 == 4
#pragma pack(4)
#endif
typedef struct vol_directed_rd32 {
	int32_t		vdr_flags;
	offset_t	vdr_offset;	 
	size32_t	vdr_nbytes;
	size32_t	vdr_bytesread;
	caddr32_t	vdr_data;
	int32_t		vdr_side;
	char		vdr_side_name[VOL_SIDENAME];
} vol_directed_rd32_t;
#if _LONG_LONG_ALIGNMENT == 8 && _LONG_LONG_ALIGNMENT_32 == 4
#pragma pack()
#endif
#endif	 
#define	DKIOC_GETDISKID	(DKIOC|46)
#define	DKD_ATA_TYPE	0x01  
#define	DKD_SCSI_TYPE	0x02  
#define	DKD_ATA_MODEL	40	 
#define	DKD_ATA_FWVER	8	 
#define	DKD_ATA_SERIAL	20	 
#define	DKD_SCSI_VENDOR	8	 
#define	DKD_SCSI_PRODUCT 16	 
#define	DKD_SCSI_REVLEVEL 4	 
#define	DKD_SCSI_SERIAL 12	 
typedef struct dk_disk_id {
	uint_t	dkd_dtype;
	union {
		struct {
			char dkd_amodel[DKD_ATA_MODEL];		 
			char dkd_afwver[DKD_ATA_FWVER];		 
			char dkd_aserial[DKD_ATA_SERIAL];	 
		} ata_disk_id;
		struct {
			char dkd_svendor[DKD_SCSI_VENDOR];	 
			char dkd_sproduct[DKD_SCSI_PRODUCT];	 
			char dkd_sfwver[DKD_SCSI_REVLEVEL];	 
			char dkd_sserial[DKD_SCSI_SERIAL];	 
		} scsi_disk_id;
	} disk_id;
} dk_disk_id_t;
#define	DKIOC_UPDATEFW		(DKIOC|47)
typedef struct dk_updatefw {
	caddr_t		dku_ptrbuf;	 
	uint_t		dku_size;	 
	uint8_t		dku_type;	 
} dk_updatefw_t;
#ifdef _SYSCALL32
typedef struct dk_updatefw_32 {
	caddr32_t	dku_ptrbuf;	 
	uint_t		dku_size;	 
	uint8_t		dku_type;	 
} dk_updatefw_32_t;
#endif  
#define	FW_TYPE_TEMP	0x0		 
#define	FW_TYPE_PERM	0x1		 
#ifdef	__cplusplus
}
#endif
#endif  
