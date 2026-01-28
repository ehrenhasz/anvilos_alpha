#ifndef _SYS_DKTP_FDISK_H
#define	_SYS_DKTP_FDISK_H
#ifdef	__cplusplus
extern "C" {
#endif
#define	MAX_SECT	(63)
#define	MAX_CYL		(1022)
#define	MAX_HEAD	(254)
#define	BOOTSZ		440	 
#define	FD_NUMPART	4	 
#define	MBB_MAGIC	0xAA55	 
#define	DEFAULT_INTLV	4	 
#define	MINPSIZE	4	 
#define	TSTPAT		0xE5	 
struct ipart {
	unsigned char bootid;	 
	unsigned char beghead;	 
	unsigned char begsect;	 
	unsigned char begcyl;	 
	unsigned char systid;	 
	unsigned char endhead;	 
	unsigned char endsect;	 
	unsigned char endcyl;	 
	uint32_t relsect;	 
	uint32_t numsect;	 
};
#define	NOTACTIVE	0
#define	ACTIVE		128
#define	UNUSED		0	 
#define	DOSOS12		1	 
#define	PCIXOS		2	 
#define	DOSOS16		4	 
#define	EXTDOS		5	 
#define	DOSHUGE		6	 
#define	FDISK_IFS	7	 
#define	FDISK_AIXBOOT	8	 
#define	FDISK_AIXDATA	9	 
#define	FDISK_OS2BOOT	10	 
#define	FDISK_WINDOWS	11	 
#define	FDISK_EXT_WIN	12	 
#define	FDISK_FAT95	14	 
#define	FDISK_EXTLBA	15	 
#define	DIAGPART	18	 
#define	FDISK_LINUX	65	 
#define	FDISK_LINUXDSWAP	66	 
#define	FDISK_LINUXDNAT	67	 
#define	FDISK_CPM	82	 
#define	DOSDATA		86	 
#define	OTHEROS		98	 
#define	UNIXOS		99	 
#define	FDISK_NOVELL2	100	 
#define	FDISK_NOVELL3	101	 
#define	FDISK_QNX4	119	 
#define	FDISK_QNX42	120	 
#define	FDISK_QNX43	121	 
#define	SUNIXOS		130	 
#define	FDISK_LINUXNAT	131	 
#define	FDISK_NTFSVOL1	134	 
#define	FDISK_NTFSVOL2	135	 
#define	FDISK_BSD	165	 
#define	FDISK_NEXTSTEP	167	 
#define	FDISK_BSDIFS	183	 
#define	FDISK_BSDISWAP	184	 
#define	X86BOOT		190	 
#define	SUNIXOS2	191	 
#define	EFI_PMBR	238	 
#define	EFI_FS		239	 
#define	MAXDOS		65535L	 
struct mboot {	 
	char	bootinst[BOOTSZ];
	uint16_t win_volserno_lo;
	uint16_t win_volserno_hi;
	uint16_t reserved;
	char	parts[FD_NUMPART * sizeof (struct ipart)];
	ushort_t signature;
};
#if defined(__i386) || defined(__amd64)
#define	FDISK_PART_TABLE_START	446
#define	MAX_EXT_PARTS	32
#else
#define	MAX_EXT_PARTS	0
#endif	 
#ifdef	__cplusplus
}
#endif
#endif	 
