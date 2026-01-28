


#if !defined(_IO_EDGEPORT_H_)
#define	_IO_EDGEPORT_H_

#define MAX_RS232_PORTS		8	


#ifndef LOW8
	#define LOW8(a)		((unsigned char)(a & 0xff))
#endif
#ifndef HIGH8
	#define HIGH8(a)	((unsigned char)((a & 0xff00) >> 8))
#endif

#include "io_usbvend.h"


struct edgeport_product_info {
	__u16	ProductId;			
	__u8	NumPorts;			
	__u8	ProdInfoVer;			

	__u32	IsServer        :1;		
	__u32	IsRS232         :1;		
	__u32	IsRS422         :1;		
	__u32	IsRS485         :1;		
	__u32	IsReserved      :28;		

	__u8	RomSize;			
	__u8	RamSize;			
	__u8	CpuRev;				
	__u8	BoardRev;			

	__u8	BootMajorVersion;		
	__u8	BootMinorVersion;		
	__le16	BootBuildNumber;		

	__u8	FirmwareMajorVersion;		
	__u8	FirmwareMinorVersion;		
	__le16	FirmwareBuildNumber;		

	__u8	ManufactureDescDate[3];		
	__u8	HardwareType;

	__u8	iDownloadFile;			
	__u8	EpicVer;			

	struct edge_compatibility_bits Epic;
};

#endif
