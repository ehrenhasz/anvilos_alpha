
#ifndef UTIL_LINUX_PT_BSD_H
#define UTIL_LINUX_PT_BSD_H

#define BSD_MAXPARTITIONS	16
#define BSD_FS_UNUSED		0

#ifndef BSD_DISKMAGIC
# define BSD_DISKMAGIC     ((uint32_t) 0x82564557)
#endif

#define BSD_LINUX_BOOTDIR "/usr/ucb/mdec"

#if defined (__alpha__) || defined (__powerpc__) || \
    defined (__ia64__) || defined (__hppa__)
# define BSD_LABELSECTOR   0
# define BSD_LABELOFFSET   64
#else
# define BSD_LABELSECTOR   1
# define BSD_LABELOFFSET   0
#endif

#define	BSD_BBSIZE        8192		
#define	BSD_SBSIZE        8192		

struct bsd_disklabel {
	uint32_t	d_magic;		
	int16_t		d_type;			
	int16_t		d_subtype;		
	char		d_typename[16];		
	char		d_packname[16];		

			
	uint32_t	d_secsize;		
	uint32_t	d_nsectors;		
	uint32_t	d_ntracks;		
	uint32_t	d_ncylinders;		
	uint32_t	d_secpercyl;		
	uint32_t	d_secperunit;		

	
	uint16_t	d_sparespertrack;	
	uint16_t	d_sparespercyl;		

	
	uint32_t	d_acylinders;		

			
	
	uint16_t	d_rpm;			
	uint16_t	d_interleave;		
	uint16_t	d_trackskew;		
	uint16_t	d_cylskew;		
	uint32_t	d_headswitch;		
	uint32_t	d_trkseek;		
	uint32_t	d_flags;		
	uint32_t	d_drivedata[5];		
	uint32_t	d_spare[5];		
	uint32_t	d_magic2;		
	uint16_t	d_checksum;		

			
	uint16_t	d_npartitions;	        
	uint32_t	d_bbsize;	        
	uint32_t	d_sbsize;	        

	struct bsd_partition	 {		
		uint32_t	p_size;	        
		uint32_t	p_offset;       
		uint32_t	p_fsize;        
		uint8_t		p_fstype;	
		uint8_t		p_frag;		
		uint16_t	p_cpg;	        
	} __attribute__((packed)) d_partitions[BSD_MAXPARTITIONS];	
} __attribute__((packed));



#define	BSD_DTYPE_SMD		1		
#define	BSD_DTYPE_MSCP		2		
#define	BSD_DTYPE_DEC		3		
#define	BSD_DTYPE_SCSI		4		
#define	BSD_DTYPE_ESDI		5		
#define	BSD_DTYPE_ST506		6		
#define	BSD_DTYPE_HPIB		7		
#define BSD_DTYPE_HPFL		8		
#define	BSD_DTYPE_FLOPPY	10		


#define BSD_DSTYPE_INDOSPART	0x8		
#define BSD_DSTYPE_DOSPART(s)	((s) & 3)	
#define BSD_DSTYPE_GEOMETRY	0x10		


#define	BSD_FS_UNUSED	0		
#define	BSD_FS_SWAP    	1		
#define	BSD_FS_V6      	2		
#define	BSD_FS_V7      	3		
#define	BSD_FS_SYSV    	4		
#define	BSD_FS_V71K    	5		
#define	BSD_FS_V8      	6		
#define	BSD_FS_BSDFFS	7		
#define	BSD_FS_BSDLFS	9		
#define	BSD_FS_OTHER	10		
#define	BSD_FS_HPFS	11		
#define	BSD_FS_ISO9660	12		
#define BSD_FS_ISOFS	BSD_FS_ISO9660
#define	BSD_FS_BOOT	13		
#define BSD_FS_ADOS	14		
#define BSD_FS_HFS	15		
#define BSD_FS_ADVFS	16		


#ifdef __alpha__
#define	BSD_FS_EXT2	8		
#else
#define	BSD_FS_MSDOS	8		
#endif


#define	BSD_D_REMOVABLE	0x01		
#define	BSD_D_ECC      	0x02		
#define	BSD_D_BADSECT	0x04		
#define	BSD_D_RAMDISK	0x08		
#define	BSD_D_CHAIN    	0x10		
#define	BSD_D_DOSPART	0x20		

#endif 
