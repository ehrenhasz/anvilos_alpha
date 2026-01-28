#include "ecma_167.h"
#ifndef _OSTA_UDF_H
#define _OSTA_UDF_H 1
#define UDF_CHAR_SET_TYPE		0
#define UDF_CHAR_SET_INFO		"OSTA Compressed Unicode"
#define UDF_ID_DEVELOPER		"*Linux UDFFS"
#define	UDF_ID_COMPLIANT		"*OSTA UDF Compliant"
#define UDF_ID_LV_INFO			"*UDF LV Info"
#define UDF_ID_FREE_EA			"*UDF FreeEASpace"
#define UDF_ID_FREE_APP_EA		"*UDF FreeAppEASpace"
#define UDF_ID_DVD_CGMS			"*UDF DVD CGMS Info"
#define UDF_ID_VAT_LVEXTENSION		"*UDF VAT LVExtension"
#define UDF_ID_OS2_EA			"*UDF OS/2 EA"
#define UDF_ID_OS2_EA_LENGTH		"*UDF OS/2 EALength"
#define UDF_ID_MAC_VOLUME		"*UDF Mac VolumeInfo"
#define UDF_ID_MAC_FINDER		"*UDF Mac FinderInfo"
#define UDF_ID_MAC_UNIQUE		"*UDF Mac UniqueIDTable"
#define UDF_ID_MAC_RESOURCE		"*UDF Mac ResourceFork"
#define UDF_ID_OS400_DIRINFO		"*UDF OS/400 DirInfo"
#define UDF_ID_VIRTUAL			"*UDF Virtual Partition"
#define UDF_ID_SPARABLE			"*UDF Sparable Partition"
#define UDF_ID_ALLOC			"*UDF Virtual Alloc Tbl"
#define UDF_ID_SPARING			"*UDF Sparing Table"
#define UDF_ID_METADATA			"*UDF Metadata Partition"
#define DOMAIN_FLAGS_HARD_WRITE_PROTECT	0x01
#define DOMAIN_FLAGS_SOFT_WRITE_PROTECT	0x02
struct domainIdentSuffix {
	__le16		UDFRevision;
	uint8_t		domainFlags;
	uint8_t		reserved[5];
} __packed;
struct UDFIdentSuffix {
	__le16		UDFRevision;
	uint8_t		OSClass;
	uint8_t		OSIdentifier;
	uint8_t		reserved[4];
} __packed;
struct impIdentSuffix {
	uint8_t		OSClass;
	uint8_t		OSIdentifier;
	uint8_t		impUse[6];
} __packed;
struct appIdentSuffix {
	uint8_t		impUse[8];
} __packed;
struct logicalVolIntegrityDescImpUse {
	struct regid	impIdent;
	__le32		numFiles;
	__le32		numDirs;
	__le16		minUDFReadRev;
	__le16		minUDFWriteRev;
	__le16		maxUDFWriteRev;
	uint8_t		impUse[];
} __packed;
struct impUseVolDescImpUse {
	struct charspec	LVICharset;
	dstring		logicalVolIdent[128];
	dstring		LVInfo1[36];
	dstring		LVInfo2[36];
	dstring		LVInfo3[36];
	struct regid	impIdent;
	uint8_t		impUse[128];
} __packed;
struct udfPartitionMap2 {
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		reserved1[2];
	struct regid	partIdent;
	__le16		volSeqNum;
	__le16		partitionNum;
} __packed;
struct virtualPartitionMap {
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		reserved1[2];
	struct regid	partIdent;
	__le16		volSeqNum;
	__le16		partitionNum;
	uint8_t		reserved2[24];
} __packed;
struct sparablePartitionMap {
	uint8_t partitionMapType;
	uint8_t partitionMapLength;
	uint8_t reserved1[2];
	struct regid partIdent;
	__le16 volSeqNum;
	__le16 partitionNum;
	__le16 packetLength;
	uint8_t numSparingTables;
	uint8_t reserved2[1];
	__le32 sizeSparingTable;
	__le32 locSparingTable[4];
} __packed;
struct metadataPartitionMap {
	uint8_t		partitionMapType;
	uint8_t		partitionMapLength;
	uint8_t		reserved1[2];
	struct regid	partIdent;
	__le16		volSeqNum;
	__le16		partitionNum;
	__le32		metadataFileLoc;
	__le32		metadataMirrorFileLoc;
	__le32		metadataBitmapFileLoc;
	__le32		allocUnitSize;
	__le16		alignUnitSize;
	uint8_t		flags;
	uint8_t		reserved2[5];
} __packed;
struct virtualAllocationTable20 {
	__le16		lengthHeader;
	__le16		lengthImpUse;
	dstring		logicalVolIdent[128];
	__le32		previousVATICBLoc;
	__le32		numFiles;
	__le32		numDirs;
	__le16		minUDFReadRev;
	__le16		minUDFWriteRev;
	__le16		maxUDFWriteRev;
	__le16		reserved;
	uint8_t		impUse[];
} __packed;
#define ICBTAG_FILE_TYPE_VAT20		0xF8U
struct sparingEntry {
	__le32		origLocation;
	__le32		mappedLocation;
} __packed;
struct sparingTable {
	struct tag	descTag;
	struct regid	sparingIdent;
	__le16		reallocationTableLen;
	__le16		reserved;
	__le32		sequenceNum;
	struct sparingEntry mapEntry[];
} __packed;
#define ICBTAG_FILE_TYPE_MAIN		0xFA
#define ICBTAG_FILE_TYPE_MIRROR		0xFB
#define ICBTAG_FILE_TYPE_BITMAP		0xFC
struct allocDescImpUse {
	__le16		flags;
	uint8_t		impUse[4];
} __packed;
#define AD_IU_EXT_ERASED		0x0001
#define ICBTAG_FILE_TYPE_REALTIME	0xF9U
struct freeEaSpace {
	__le16		headerChecksum;
	uint8_t		freeEASpace[];
} __packed;
struct DVDCopyrightImpUse {
	__le16		headerChecksum;
	uint8_t		CGMSInfo;
	uint8_t		dataType;
	uint8_t		protectionSystemInfo[4];
} __packed;
struct LVExtensionEA {
	__le16		headerChecksum;
	__le64		verificationID;
	__le32		numFiles;
	__le32		numDirs;
	dstring		logicalVolIdent[128];
} __packed;
struct freeAppEASpace {
	__le16		headerChecksum;
	uint8_t		freeEASpace[];
} __packed;
#define UDF_ID_UNIQUE_ID		"*UDF Unique ID Mapping Data"
#define UDF_ID_NON_ALLOC		"*UDF Non-Allocatable Space"
#define UDF_ID_POWER_CAL		"*UDF Power Cal Table"
#define UDF_ID_BACKUP			"*UDF Backup"
#define UDF_ID_MAC_RESOURCE_FORK_STREAM	"*UDF Macintosh Resource Fork"
#define UDF_ID_NT_ACL			"*UDF NT ACL"
#define UDF_ID_UNIX_ACL			"*UDF UNIX ACL"
#define UDF_OS_CLASS_UNDEF		0x00U
#define UDF_OS_CLASS_DOS		0x01U
#define UDF_OS_CLASS_OS2		0x02U
#define UDF_OS_CLASS_MAC		0x03U
#define UDF_OS_CLASS_UNIX		0x04U
#define UDF_OS_CLASS_WIN9X		0x05U
#define UDF_OS_CLASS_WINNT		0x06U
#define UDF_OS_CLASS_OS400		0x07U
#define UDF_OS_CLASS_BEOS		0x08U
#define UDF_OS_CLASS_WINCE		0x09U
#define UDF_OS_ID_UNDEF			0x00U
#define UDF_OS_ID_DOS			0x00U
#define UDF_OS_ID_OS2			0x00U
#define UDF_OS_ID_MAC			0x00U
#define UDF_OS_ID_MAX_OSX		0x01U
#define UDF_OS_ID_UNIX			0x00U
#define UDF_OS_ID_AIX			0x01U
#define UDF_OS_ID_SOLARIS		0x02U
#define UDF_OS_ID_HPUX			0x03U
#define UDF_OS_ID_IRIX			0x04U
#define UDF_OS_ID_LINUX			0x05U
#define UDF_OS_ID_MKLINUX		0x06U
#define UDF_OS_ID_FREEBSD		0x07U
#define UDF_OS_ID_NETBSD		0x08U
#define UDF_OS_ID_WIN9X			0x00U
#define UDF_OS_ID_WINNT			0x00U
#define UDF_OS_ID_OS400			0x00U
#define UDF_OS_ID_BEOS			0x00U
#define UDF_OS_ID_WINCE			0x00U
#endif  
