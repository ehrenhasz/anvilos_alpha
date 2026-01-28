


#ifndef LSI_MEGARAID_SAS_H
#define LSI_MEGARAID_SAS_H

#include <scsi/scsi_cmnd.h>


#define MEGASAS_VERSION				"07.725.01.00-rc1"
#define MEGASAS_RELDATE				"Mar 2, 2023"

#define MEGASAS_MSIX_NAME_LEN			32


#define	PCI_DEVICE_ID_LSI_SAS1078R		0x0060
#define	PCI_DEVICE_ID_LSI_SAS1078DE		0x007C
#define	PCI_DEVICE_ID_LSI_VERDE_ZCR		0x0413
#define	PCI_DEVICE_ID_LSI_SAS1078GEN2		0x0078
#define	PCI_DEVICE_ID_LSI_SAS0079GEN2		0x0079
#define	PCI_DEVICE_ID_LSI_SAS0073SKINNY		0x0073
#define	PCI_DEVICE_ID_LSI_SAS0071SKINNY		0x0071
#define	PCI_DEVICE_ID_LSI_FUSION		0x005b
#define PCI_DEVICE_ID_LSI_PLASMA		0x002f
#define PCI_DEVICE_ID_LSI_INVADER		0x005d
#define PCI_DEVICE_ID_LSI_FURY			0x005f
#define PCI_DEVICE_ID_LSI_INTRUDER		0x00ce
#define PCI_DEVICE_ID_LSI_INTRUDER_24		0x00cf
#define PCI_DEVICE_ID_LSI_CUTLASS_52		0x0052
#define PCI_DEVICE_ID_LSI_CUTLASS_53		0x0053
#define PCI_DEVICE_ID_LSI_VENTURA		    0x0014
#define PCI_DEVICE_ID_LSI_CRUSADER		    0x0015
#define PCI_DEVICE_ID_LSI_HARPOON		    0x0016
#define PCI_DEVICE_ID_LSI_TOMCAT		    0x0017
#define PCI_DEVICE_ID_LSI_VENTURA_4PORT		0x001B
#define PCI_DEVICE_ID_LSI_CRUSADER_4PORT	0x001C
#define PCI_DEVICE_ID_LSI_AERO_10E1		0x10e1
#define PCI_DEVICE_ID_LSI_AERO_10E2		0x10e2
#define PCI_DEVICE_ID_LSI_AERO_10E5		0x10e5
#define PCI_DEVICE_ID_LSI_AERO_10E6		0x10e6
#define PCI_DEVICE_ID_LSI_AERO_10E0		0x10e0
#define PCI_DEVICE_ID_LSI_AERO_10E3		0x10e3
#define PCI_DEVICE_ID_LSI_AERO_10E4		0x10e4
#define PCI_DEVICE_ID_LSI_AERO_10E7		0x10e7


#define MEGARAID_INTEL_RS3DC080_SSDID		0x9360
#define MEGARAID_INTEL_RS3DC040_SSDID		0x9362
#define MEGARAID_INTEL_RS3SC008_SSDID		0x9380
#define MEGARAID_INTEL_RS3MC044_SSDID		0x9381
#define MEGARAID_INTEL_RS3WC080_SSDID		0x9341
#define MEGARAID_INTEL_RS3WC040_SSDID		0x9343
#define MEGARAID_INTEL_RMS3BC160_SSDID		0x352B


#define MEGARAID_INTRUDER_SSDID1		0x9371
#define MEGARAID_INTRUDER_SSDID2		0x9390
#define MEGARAID_INTRUDER_SSDID3		0x9370


#define MEGARAID_INTEL_RS3DC080_BRANDING	\
	"Intel(R) RAID Controller RS3DC080"
#define MEGARAID_INTEL_RS3DC040_BRANDING	\
	"Intel(R) RAID Controller RS3DC040"
#define MEGARAID_INTEL_RS3SC008_BRANDING	\
	"Intel(R) RAID Controller RS3SC008"
#define MEGARAID_INTEL_RS3MC044_BRANDING	\
	"Intel(R) RAID Controller RS3MC044"
#define MEGARAID_INTEL_RS3WC080_BRANDING	\
	"Intel(R) RAID Controller RS3WC080"
#define MEGARAID_INTEL_RS3WC040_BRANDING	\
	"Intel(R) RAID Controller RS3WC040"
#define MEGARAID_INTEL_RMS3BC160_BRANDING	\
	"Intel(R) Integrated RAID Module RMS3BC160"






#define MFI_STATE_MASK				0xF0000000
#define MFI_STATE_UNDEFINED			0x00000000
#define MFI_STATE_BB_INIT			0x10000000
#define MFI_STATE_FW_INIT			0x40000000
#define MFI_STATE_WAIT_HANDSHAKE		0x60000000
#define MFI_STATE_FW_INIT_2			0x70000000
#define MFI_STATE_DEVICE_SCAN			0x80000000
#define MFI_STATE_BOOT_MESSAGE_PENDING		0x90000000
#define MFI_STATE_FLUSH_CACHE			0xA0000000
#define MFI_STATE_READY				0xB0000000
#define MFI_STATE_OPERATIONAL			0xC0000000
#define MFI_STATE_FAULT				0xF0000000
#define MFI_STATE_FORCE_OCR			0x00000080
#define MFI_STATE_DMADONE			0x00000008
#define MFI_STATE_CRASH_DUMP_DONE		0x00000004
#define MFI_RESET_REQUIRED			0x00000001
#define MFI_RESET_ADAPTER			0x00000002
#define MEGAMFI_FRAME_SIZE			64

#define MFI_STATE_FAULT_CODE			0x0FFF0000
#define MFI_STATE_FAULT_SUBCODE			0x0000FF00

#define WRITE_SEQUENCE_OFFSET		(0x0000000FC) 
#define HOST_DIAGNOSTIC_OFFSET		(0x000000F8)  
#define DIAG_WRITE_ENABLE			(0x00000080)
#define DIAG_RESET_ADAPTER			(0x00000004)

#define MFI_ADP_RESET				0x00000040
#define MFI_INIT_ABORT				0x00000001
#define MFI_INIT_READY				0x00000002
#define MFI_INIT_MFIMODE			0x00000004
#define MFI_INIT_CLEAR_HANDSHAKE		0x00000008
#define MFI_INIT_HOTPLUG			0x00000010
#define MFI_STOP_ADP				0x00000020
#define MFI_RESET_FLAGS				MFI_INIT_READY| \
						MFI_INIT_MFIMODE| \
						MFI_INIT_ABORT
#define MFI_ADP_TRIGGER_SNAP_DUMP		0x00000100
#define MPI2_IOCINIT_MSGFLAG_RDPQ_ARRAY_MODE    (0x01)


#define MFI_FRAME_POST_IN_REPLY_QUEUE		0x0000
#define MFI_FRAME_DONT_POST_IN_REPLY_QUEUE	0x0001
#define MFI_FRAME_SGL32				0x0000
#define MFI_FRAME_SGL64				0x0002
#define MFI_FRAME_SENSE32			0x0000
#define MFI_FRAME_SENSE64			0x0004
#define MFI_FRAME_DIR_NONE			0x0000
#define MFI_FRAME_DIR_WRITE			0x0008
#define MFI_FRAME_DIR_READ			0x0010
#define MFI_FRAME_DIR_BOTH			0x0018
#define MFI_FRAME_IEEE                          0x0020


#define DRV_DCMD_POLLED_MODE		0x1
#define DRV_DCMD_SKIP_REFIRE		0x2


#define MFI_CMD_STATUS_POLL_MODE		0xFF


enum MFI_CMD_OP {
	MFI_CMD_INIT		= 0x0,
	MFI_CMD_LD_READ		= 0x1,
	MFI_CMD_LD_WRITE	= 0x2,
	MFI_CMD_LD_SCSI_IO	= 0x3,
	MFI_CMD_PD_SCSI_IO	= 0x4,
	MFI_CMD_DCMD		= 0x5,
	MFI_CMD_ABORT		= 0x6,
	MFI_CMD_SMP		= 0x7,
	MFI_CMD_STP		= 0x8,
	MFI_CMD_NVME		= 0x9,
	MFI_CMD_TOOLBOX		= 0xa,
	MFI_CMD_OP_COUNT,
	MFI_CMD_INVALID		= 0xff
};

#define MR_DCMD_CTRL_GET_INFO			0x01010000
#define MR_DCMD_LD_GET_LIST			0x03010000
#define MR_DCMD_LD_LIST_QUERY			0x03010100

#define MR_DCMD_CTRL_CACHE_FLUSH		0x01101000
#define MR_FLUSH_CTRL_CACHE			0x01
#define MR_FLUSH_DISK_CACHE			0x02

#define MR_DCMD_CTRL_SHUTDOWN			0x01050000
#define MR_DCMD_HIBERNATE_SHUTDOWN		0x01060000
#define MR_ENABLE_DRIVE_SPINDOWN		0x01

#define MR_DCMD_CTRL_EVENT_GET_INFO		0x01040100
#define MR_DCMD_CTRL_EVENT_GET			0x01040300
#define MR_DCMD_CTRL_EVENT_WAIT			0x01040500
#define MR_DCMD_LD_GET_PROPERTIES		0x03030000

#define MR_DCMD_CLUSTER				0x08000000
#define MR_DCMD_CLUSTER_RESET_ALL		0x08010100
#define MR_DCMD_CLUSTER_RESET_LD		0x08010200
#define MR_DCMD_PD_LIST_QUERY                   0x02010100

#define MR_DCMD_CTRL_SET_CRASH_DUMP_PARAMS	0x01190100
#define MR_DRIVER_SET_APP_CRASHDUMP_MODE	(0xF0010000 | 0x0600)
#define MR_DCMD_PD_GET_INFO			0x02020000


extern u8 MR_ValidateMapInfo(struct megasas_instance *instance, u64 map_id);



enum MFI_STAT {
	MFI_STAT_OK = 0x00,
	MFI_STAT_INVALID_CMD = 0x01,
	MFI_STAT_INVALID_DCMD = 0x02,
	MFI_STAT_INVALID_PARAMETER = 0x03,
	MFI_STAT_INVALID_SEQUENCE_NUMBER = 0x04,
	MFI_STAT_ABORT_NOT_POSSIBLE = 0x05,
	MFI_STAT_APP_HOST_CODE_NOT_FOUND = 0x06,
	MFI_STAT_APP_IN_USE = 0x07,
	MFI_STAT_APP_NOT_INITIALIZED = 0x08,
	MFI_STAT_ARRAY_INDEX_INVALID = 0x09,
	MFI_STAT_ARRAY_ROW_NOT_EMPTY = 0x0a,
	MFI_STAT_CONFIG_RESOURCE_CONFLICT = 0x0b,
	MFI_STAT_DEVICE_NOT_FOUND = 0x0c,
	MFI_STAT_DRIVE_TOO_SMALL = 0x0d,
	MFI_STAT_FLASH_ALLOC_FAIL = 0x0e,
	MFI_STAT_FLASH_BUSY = 0x0f,
	MFI_STAT_FLASH_ERROR = 0x10,
	MFI_STAT_FLASH_IMAGE_BAD = 0x11,
	MFI_STAT_FLASH_IMAGE_INCOMPLETE = 0x12,
	MFI_STAT_FLASH_NOT_OPEN = 0x13,
	MFI_STAT_FLASH_NOT_STARTED = 0x14,
	MFI_STAT_FLUSH_FAILED = 0x15,
	MFI_STAT_HOST_CODE_NOT_FOUNT = 0x16,
	MFI_STAT_LD_CC_IN_PROGRESS = 0x17,
	MFI_STAT_LD_INIT_IN_PROGRESS = 0x18,
	MFI_STAT_LD_LBA_OUT_OF_RANGE = 0x19,
	MFI_STAT_LD_MAX_CONFIGURED = 0x1a,
	MFI_STAT_LD_NOT_OPTIMAL = 0x1b,
	MFI_STAT_LD_RBLD_IN_PROGRESS = 0x1c,
	MFI_STAT_LD_RECON_IN_PROGRESS = 0x1d,
	MFI_STAT_LD_WRONG_RAID_LEVEL = 0x1e,
	MFI_STAT_MAX_SPARES_EXCEEDED = 0x1f,
	MFI_STAT_MEMORY_NOT_AVAILABLE = 0x20,
	MFI_STAT_MFC_HW_ERROR = 0x21,
	MFI_STAT_NO_HW_PRESENT = 0x22,
	MFI_STAT_NOT_FOUND = 0x23,
	MFI_STAT_NOT_IN_ENCL = 0x24,
	MFI_STAT_PD_CLEAR_IN_PROGRESS = 0x25,
	MFI_STAT_PD_TYPE_WRONG = 0x26,
	MFI_STAT_PR_DISABLED = 0x27,
	MFI_STAT_ROW_INDEX_INVALID = 0x28,
	MFI_STAT_SAS_CONFIG_INVALID_ACTION = 0x29,
	MFI_STAT_SAS_CONFIG_INVALID_DATA = 0x2a,
	MFI_STAT_SAS_CONFIG_INVALID_PAGE = 0x2b,
	MFI_STAT_SAS_CONFIG_INVALID_TYPE = 0x2c,
	MFI_STAT_SCSI_DONE_WITH_ERROR = 0x2d,
	MFI_STAT_SCSI_IO_FAILED = 0x2e,
	MFI_STAT_SCSI_RESERVATION_CONFLICT = 0x2f,
	MFI_STAT_SHUTDOWN_FAILED = 0x30,
	MFI_STAT_TIME_NOT_SET = 0x31,
	MFI_STAT_WRONG_STATE = 0x32,
	MFI_STAT_LD_OFFLINE = 0x33,
	MFI_STAT_PEER_NOTIFICATION_REJECTED = 0x34,
	MFI_STAT_PEER_NOTIFICATION_FAILED = 0x35,
	MFI_STAT_RESERVATION_IN_PROGRESS = 0x36,
	MFI_STAT_I2C_ERRORS_DETECTED = 0x37,
	MFI_STAT_PCI_ERRORS_DETECTED = 0x38,
	MFI_STAT_CONFIG_SEQ_MISMATCH = 0x67,

	MFI_STAT_INVALID_STATUS = 0xFF
};

enum mfi_evt_class {
	MFI_EVT_CLASS_DEBUG =		-2,
	MFI_EVT_CLASS_PROGRESS =	-1,
	MFI_EVT_CLASS_INFO =		0,
	MFI_EVT_CLASS_WARNING =		1,
	MFI_EVT_CLASS_CRITICAL =	2,
	MFI_EVT_CLASS_FATAL =		3,
	MFI_EVT_CLASS_DEAD =		4
};


#define MAX_CRASH_DUMP_SIZE 512
#define CRASH_DMA_BUF_SIZE  (1024 * 1024)

enum MR_FW_CRASH_DUMP_STATE {
	UNAVAILABLE = 0,
	AVAILABLE = 1,
	COPYING = 2,
	COPIED = 3,
	COPY_ERROR = 4,
};

enum _MR_CRASH_BUF_STATUS {
	MR_CRASH_BUF_TURN_OFF = 0,
	MR_CRASH_BUF_TURN_ON = 1,
};


#define MFI_MBOX_SIZE				12

enum MR_EVT_CLASS {

	MR_EVT_CLASS_DEBUG = -2,
	MR_EVT_CLASS_PROGRESS = -1,
	MR_EVT_CLASS_INFO = 0,
	MR_EVT_CLASS_WARNING = 1,
	MR_EVT_CLASS_CRITICAL = 2,
	MR_EVT_CLASS_FATAL = 3,
	MR_EVT_CLASS_DEAD = 4,

};

enum MR_EVT_LOCALE {

	MR_EVT_LOCALE_LD = 0x0001,
	MR_EVT_LOCALE_PD = 0x0002,
	MR_EVT_LOCALE_ENCL = 0x0004,
	MR_EVT_LOCALE_BBU = 0x0008,
	MR_EVT_LOCALE_SAS = 0x0010,
	MR_EVT_LOCALE_CTRL = 0x0020,
	MR_EVT_LOCALE_CONFIG = 0x0040,
	MR_EVT_LOCALE_CLUSTER = 0x0080,
	MR_EVT_LOCALE_ALL = 0xffff,

};

enum MR_EVT_ARGS {

	MR_EVT_ARGS_NONE,
	MR_EVT_ARGS_CDB_SENSE,
	MR_EVT_ARGS_LD,
	MR_EVT_ARGS_LD_COUNT,
	MR_EVT_ARGS_LD_LBA,
	MR_EVT_ARGS_LD_OWNER,
	MR_EVT_ARGS_LD_LBA_PD_LBA,
	MR_EVT_ARGS_LD_PROG,
	MR_EVT_ARGS_LD_STATE,
	MR_EVT_ARGS_LD_STRIP,
	MR_EVT_ARGS_PD,
	MR_EVT_ARGS_PD_ERR,
	MR_EVT_ARGS_PD_LBA,
	MR_EVT_ARGS_PD_LBA_LD,
	MR_EVT_ARGS_PD_PROG,
	MR_EVT_ARGS_PD_STATE,
	MR_EVT_ARGS_PCI,
	MR_EVT_ARGS_RATE,
	MR_EVT_ARGS_STR,
	MR_EVT_ARGS_TIME,
	MR_EVT_ARGS_ECC,
	MR_EVT_ARGS_LD_PROP,
	MR_EVT_ARGS_PD_SPARE,
	MR_EVT_ARGS_PD_INDEX,
	MR_EVT_ARGS_DIAG_PASS,
	MR_EVT_ARGS_DIAG_FAIL,
	MR_EVT_ARGS_PD_LBA_LBA,
	MR_EVT_ARGS_PORT_PHY,
	MR_EVT_ARGS_PD_MISSING,
	MR_EVT_ARGS_PD_ADDRESS,
	MR_EVT_ARGS_BITMAP,
	MR_EVT_ARGS_CONNECTOR,
	MR_EVT_ARGS_PD_PD,
	MR_EVT_ARGS_PD_FRU,
	MR_EVT_ARGS_PD_PATHINFO,
	MR_EVT_ARGS_PD_POWER_STATE,
	MR_EVT_ARGS_GENERIC,
};


#define SGE_BUFFER_SIZE	4096
#define MEGASAS_CLUSTER_ID_SIZE	16

enum MR_PD_QUERY_TYPE {
	MR_PD_QUERY_TYPE_ALL                = 0,
	MR_PD_QUERY_TYPE_STATE              = 1,
	MR_PD_QUERY_TYPE_POWER_STATE        = 2,
	MR_PD_QUERY_TYPE_MEDIA_TYPE         = 3,
	MR_PD_QUERY_TYPE_SPEED              = 4,
	MR_PD_QUERY_TYPE_EXPOSED_TO_HOST    = 5,
};

enum MR_LD_QUERY_TYPE {
	MR_LD_QUERY_TYPE_ALL	         = 0,
	MR_LD_QUERY_TYPE_EXPOSED_TO_HOST = 1,
	MR_LD_QUERY_TYPE_USED_TGT_IDS    = 2,
	MR_LD_QUERY_TYPE_CLUSTER_ACCESS  = 3,
	MR_LD_QUERY_TYPE_CLUSTER_LOCALE  = 4,
};


#define MR_EVT_CFG_CLEARED                              0x0004
#define MR_EVT_LD_STATE_CHANGE                          0x0051
#define MR_EVT_PD_INSERTED                              0x005b
#define MR_EVT_PD_REMOVED                               0x0070
#define MR_EVT_LD_CREATED                               0x008a
#define MR_EVT_LD_DELETED                               0x008b
#define MR_EVT_FOREIGN_CFG_IMPORTED                     0x00db
#define MR_EVT_LD_OFFLINE                               0x00fc
#define MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED             0x0152
#define MR_EVT_CTRL_PROP_CHANGED			0x012f

enum MR_PD_STATE {
	MR_PD_STATE_UNCONFIGURED_GOOD   = 0x00,
	MR_PD_STATE_UNCONFIGURED_BAD    = 0x01,
	MR_PD_STATE_HOT_SPARE           = 0x02,
	MR_PD_STATE_OFFLINE             = 0x10,
	MR_PD_STATE_FAILED              = 0x11,
	MR_PD_STATE_REBUILD             = 0x14,
	MR_PD_STATE_ONLINE              = 0x18,
	MR_PD_STATE_COPYBACK            = 0x20,
	MR_PD_STATE_SYSTEM              = 0x40,
 };

union MR_PD_REF {
	struct {
		u16	 deviceId;
		u16	 seqNum;
	} mrPdRef;
	u32	 ref;
};


union MR_PD_DDF_TYPE {
	 struct {
		union {
			struct {
#ifndef __BIG_ENDIAN_BITFIELD
				 u16	 forcedPDGUID:1;
				 u16	 inVD:1;
				 u16	 isGlobalSpare:1;
				 u16	 isSpare:1;
				 u16	 isForeign:1;
				 u16	 reserved:7;
				 u16	 intf:4;
#else
				 u16	 intf:4;
				 u16	 reserved:7;
				 u16	 isForeign:1;
				 u16	 isSpare:1;
				 u16	 isGlobalSpare:1;
				 u16	 inVD:1;
				 u16	 forcedPDGUID:1;
#endif
			 } pdType;
			 u16	 type;
		 };
		 u16	 reserved;
	 } ddf;
	 struct {
		 u32	reserved;
	 } nonDisk;
	 u32	 type;
} __packed;


union MR_PROGRESS {
	struct  {
		u16 progress;
		union {
			u16 elapsedSecs;
			u16 elapsedSecsForLastPercent;
		};
	} mrProgress;
	u32 w;
} __packed;


struct MR_PD_PROGRESS {
	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u32     rbld:1;
		u32     patrol:1;
		u32     clear:1;
		u32     copyBack:1;
		u32     erase:1;
		u32     locate:1;
		u32     reserved:26;
#else
		u32     reserved:26;
		u32     locate:1;
		u32     erase:1;
		u32     copyBack:1;
		u32     clear:1;
		u32     patrol:1;
		u32     rbld:1;
#endif
	} active;
	union MR_PROGRESS     rbld;
	union MR_PROGRESS     patrol;
	union {
		union MR_PROGRESS     clear;
		union MR_PROGRESS     erase;
	};

	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u32     rbld:1;
		u32     patrol:1;
		u32     clear:1;
		u32     copyBack:1;
		u32     erase:1;
		u32     reserved:27;
#else
		u32     reserved:27;
		u32     erase:1;
		u32     copyBack:1;
		u32     clear:1;
		u32     patrol:1;
		u32     rbld:1;
#endif
	} pause;

	union MR_PROGRESS     reserved[3];
} __packed;

struct  MR_PD_INFO {
	union MR_PD_REF	ref;
	u8 inquiryData[96];
	u8 vpdPage83[64];
	u8 notSupported;
	u8 scsiDevType;

	union {
		u8 connectedPortBitmap;
		u8 connectedPortNumbers;
	};

	u8 deviceSpeed;
	u32 mediaErrCount;
	u32 otherErrCount;
	u32 predFailCount;
	u32 lastPredFailEventSeqNum;

	u16 fwState;
	u8 disabledForRemoval;
	u8 linkSpeed;
	union MR_PD_DDF_TYPE state;

	struct {
		u8 count;
#ifndef __BIG_ENDIAN_BITFIELD
		u8 isPathBroken:4;
		u8 reserved3:3;
		u8 widePortCapable:1;
#else
		u8 widePortCapable:1;
		u8 reserved3:3;
		u8 isPathBroken:4;
#endif

		u8 connectorIndex[2];
		u8 reserved[4];
		u64 sasAddr[2];
		u8 reserved2[16];
	} pathInfo;

	u64 rawSize;
	u64 nonCoercedSize;
	u64 coercedSize;
	u16 enclDeviceId;
	u8 enclIndex;

	union {
		u8 slotNumber;
		u8 enclConnectorIndex;
	};

	struct MR_PD_PROGRESS progInfo;
	u8 badBlockTableFull;
	u8 unusableInCurrentConfig;
	u8 vpdPage83Ext[64];
	u8 powerState;
	u8 enclPosition;
	u32 allowedOps;
	u16 copyBackPartnerId;
	u16 enclPartnerDeviceId;
	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u16 fdeCapable:1;
		u16 fdeEnabled:1;
		u16 secured:1;
		u16 locked:1;
		u16 foreign:1;
		u16 needsEKM:1;
		u16 reserved:10;
#else
		u16 reserved:10;
		u16 needsEKM:1;
		u16 foreign:1;
		u16 locked:1;
		u16 secured:1;
		u16 fdeEnabled:1;
		u16 fdeCapable:1;
#endif
	} security;
	u8 mediaType;
	u8 notCertified;
	u8 bridgeVendor[8];
	u8 bridgeProductIdentification[16];
	u8 bridgeProductRevisionLevel[4];
	u8 satBridgeExists;

	u8 interfaceType;
	u8 temperature;
	u8 emulatedBlockSize;
	u16 userDataBlockSize;
	u16 reserved2;

	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u32 piType:3;
		u32 piFormatted:1;
		u32 piEligible:1;
		u32 NCQ:1;
		u32 WCE:1;
		u32 commissionedSpare:1;
		u32 emergencySpare:1;
		u32 ineligibleForSSCD:1;
		u32 ineligibleForLd:1;
		u32 useSSEraseType:1;
		u32 wceUnchanged:1;
		u32 supportScsiUnmap:1;
		u32 reserved:18;
#else
		u32 reserved:18;
		u32 supportScsiUnmap:1;
		u32 wceUnchanged:1;
		u32 useSSEraseType:1;
		u32 ineligibleForLd:1;
		u32 ineligibleForSSCD:1;
		u32 emergencySpare:1;
		u32 commissionedSpare:1;
		u32 WCE:1;
		u32 NCQ:1;
		u32 piEligible:1;
		u32 piFormatted:1;
		u32 piType:3;
#endif
	} properties;

	u64 shieldDiagCompletionTime;
	u8 shieldCounter;

	u8 linkSpeedOther;
	u8 reserved4[2];

	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u32 bbmErrCountSupported:1;
		u32 bbmErrCount:31;
#else
		u32 bbmErrCount:31;
		u32 bbmErrCountSupported:1;
#endif
	} bbmErr;

	u8 reserved1[512-428];
} __packed;


struct MR_TARGET_PROPERTIES {
	u32    max_io_size_kb;
	u32    device_qdepth;
	u32    sector_size;
	u8     reset_tmo;
	u8     reserved[499];
} __packed;

 
struct MR_PD_ADDRESS {
	__le16	deviceId;
	u16     enclDeviceId;

	union {
		struct {
			u8  enclIndex;
			u8  slotNumber;
		} mrPdAddress;
		struct {
			u8  enclPosition;
			u8  enclConnectorIndex;
		} mrEnclAddress;
	};
	u8      scsiDevType;
	union {
		u8      connectedPortBitmap;
		u8      connectedPortNumbers;
	};
	u64     sasAddr[2];
} __packed;


struct MR_PD_LIST {
	__le32		size;
	__le32		count;
	struct MR_PD_ADDRESS   addr[1];
} __packed;

struct megasas_pd_list {
	u16             tid;
	u8             driveType;
	u8             driveState;
} __packed;

 
union  MR_LD_REF {
	struct {
		u8      targetId;
		u8      reserved;
		__le16     seqNum;
	};
	__le32     ref;
} __packed;


struct MR_LD_LIST {
	__le32     ldCount;
	__le32     reserved;
	struct {
		union MR_LD_REF   ref;
		u8          state;
		u8          reserved[3];
		__le64		size;
	} ldList[MAX_LOGICAL_DRIVES_EXT];
} __packed;

struct MR_LD_TARGETID_LIST {
	__le32	size;
	__le32	count;
	u8	pad[3];
	u8	targetId[MAX_LOGICAL_DRIVES_EXT];
};

struct MR_HOST_DEVICE_LIST_ENTRY {
	struct {
		union {
			struct {
#if defined(__BIG_ENDIAN_BITFIELD)
				u8 reserved:7;
				u8 is_sys_pd:1;
#else
				u8 is_sys_pd:1;
				u8 reserved:7;
#endif
			} bits;
			u8 byte;
		} u;
	} flags;
	u8 scsi_type;
	__le16 target_id;
	u8 reserved[4];
	__le64 sas_addr[2];
} __packed;

struct MR_HOST_DEVICE_LIST {
	__le32			size;
	__le32			count;
	__le32			reserved[2];
	struct MR_HOST_DEVICE_LIST_ENTRY	host_device_list[1];
} __packed;

#define HOST_DEVICE_LIST_SZ (sizeof(struct MR_HOST_DEVICE_LIST) +	       \
			      (sizeof(struct MR_HOST_DEVICE_LIST_ENTRY) *      \
			      (MEGASAS_MAX_PD + MAX_LOGICAL_DRIVES_EXT - 1)))



struct megasas_ctrl_prop {

	u16 seq_num;
	u16 pred_fail_poll_interval;
	u16 intr_throttle_count;
	u16 intr_throttle_timeouts;
	u8 rebuild_rate;
	u8 patrol_read_rate;
	u8 bgi_rate;
	u8 cc_rate;
	u8 recon_rate;
	u8 cache_flush_interval;
	u8 spinup_drv_count;
	u8 spinup_delay;
	u8 cluster_enable;
	u8 coercion_mode;
	u8 alarm_enable;
	u8 disable_auto_rebuild;
	u8 disable_battery_warn;
	u8 ecc_bucket_size;
	u16 ecc_bucket_leak_rate;
	u8 restore_hotspare_on_insertion;
	u8 expose_encl_devices;
	u8 maintainPdFailHistory;
	u8 disallowHostRequestReordering;
	u8 abortCCOnError;
	u8 loadBalanceMode;
	u8 disableAutoDetectBackplane;

	u8 snapVDSpace;

	
	struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:18;
		u32     enableJBOD:1;
		u32     disableSpinDownHS:1;
		u32     allowBootWithPinnedCache:1;
		u32     disableOnlineCtrlReset:1;
		u32     enableSecretKeyControl:1;
		u32     autoEnhancedImport:1;
		u32     enableSpinDownUnconfigured:1;
		u32     SSDPatrolReadEnabled:1;
		u32     SSDSMARTerEnabled:1;
		u32     disableNCQ:1;
		u32     useFdeOnly:1;
		u32     prCorrectUnconfiguredAreas:1;
		u32     SMARTerEnabled:1;
		u32     copyBackDisabled:1;
#else
		u32     copyBackDisabled:1;
		u32     SMARTerEnabled:1;
		u32     prCorrectUnconfiguredAreas:1;
		u32     useFdeOnly:1;
		u32     disableNCQ:1;
		u32     SSDSMARTerEnabled:1;
		u32     SSDPatrolReadEnabled:1;
		u32     enableSpinDownUnconfigured:1;
		u32     autoEnhancedImport:1;
		u32     enableSecretKeyControl:1;
		u32     disableOnlineCtrlReset:1;
		u32     allowBootWithPinnedCache:1;
		u32     disableSpinDownHS:1;
		u32     enableJBOD:1;
		u32     reserved:18;
#endif
	} OnOffProperties;

	union {
		u8 autoSnapVDSpace;
		u8 viewSpace;
		struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
			u16 reserved3:9;
			u16 enable_fw_dev_list:1;
			u16 reserved2:1;
			u16 enable_snap_dump:1;
			u16 reserved1:4;
#else
			u16 reserved1:4;
			u16 enable_snap_dump:1;
			u16 reserved2:1;
			u16 enable_fw_dev_list:1;
			u16 reserved3:9;
#endif
		} on_off_properties2;
	};
	__le16 spinDownTime;
	u8  reserved[24];
} __packed;


struct megasas_ctrl_info {

	
	struct {

		__le16 vendor_id;
		__le16 device_id;
		__le16 sub_vendor_id;
		__le16 sub_device_id;
		u8 reserved[24];

	} __attribute__ ((packed)) pci;

	
	struct {

		u8 PCIX:1;
		u8 PCIE:1;
		u8 iSCSI:1;
		u8 SAS_3G:1;
		u8 SRIOV:1;
		u8 reserved_0:3;
		u8 reserved_1[6];
		u8 port_count;
		u64 port_addr[8];

	} __attribute__ ((packed)) host_interface;

	
	struct {

		u8 SPI:1;
		u8 SAS_3G:1;
		u8 SATA_1_5G:1;
		u8 SATA_3G:1;
		u8 reserved_0:4;
		u8 reserved_1[6];
		u8 port_count;
		u64 port_addr[8];

	} __attribute__ ((packed)) device_interface;

	
	__le32 image_check_word;
	__le32 image_component_count;

	struct {

		char name[8];
		char version[32];
		char build_date[16];
		char built_time[16];

	} __attribute__ ((packed)) image_component[8];

	
	__le32 pending_image_component_count;

	struct {

		char name[8];
		char version[32];
		char build_date[16];
		char build_time[16];

	} __attribute__ ((packed)) pending_image_component[8];

	u8 max_arms;
	u8 max_spans;
	u8 max_arrays;
	u8 max_lds;

	char product_name[80];
	char serial_no[32];

	
	struct {

		u32 bbu:1;
		u32 alarm:1;
		u32 nvram:1;
		u32 uart:1;
		u32 reserved:28;

	} __attribute__ ((packed)) hw_present;

	__le32 current_fw_time;

	
	__le16 max_concurrent_cmds;
	__le16 max_sge_count;
	__le32 max_request_size;

	
	__le16 ld_present_count;
	__le16 ld_degraded_count;
	__le16 ld_offline_count;

	__le16 pd_present_count;
	__le16 pd_disk_present_count;
	__le16 pd_disk_pred_failure_count;
	__le16 pd_disk_failed_count;

	
	__le16 nvram_size;
	__le16 memory_size;
	__le16 flash_size;

	
	__le16 mem_correctable_error_count;
	__le16 mem_uncorrectable_error_count;

	
	u8 cluster_permitted;
	u8 cluster_active;

	
	__le16 max_strips_per_io;

	
	struct {

		u32 raid_level_0:1;
		u32 raid_level_1:1;
		u32 raid_level_5:1;
		u32 raid_level_1E:1;
		u32 raid_level_6:1;
		u32 reserved:27;

	} __attribute__ ((packed)) raid_levels;

	struct {

		u32 rbld_rate:1;
		u32 cc_rate:1;
		u32 bgi_rate:1;
		u32 recon_rate:1;
		u32 patrol_rate:1;
		u32 alarm_control:1;
		u32 cluster_supported:1;
		u32 bbu:1;
		u32 spanning_allowed:1;
		u32 dedicated_hotspares:1;
		u32 revertible_hotspares:1;
		u32 foreign_config_import:1;
		u32 self_diagnostic:1;
		u32 mixed_redundancy_arr:1;
		u32 global_hot_spares:1;
		u32 reserved:17;

	} __attribute__ ((packed)) adapter_operations;

	struct {

		u32 read_policy:1;
		u32 write_policy:1;
		u32 io_policy:1;
		u32 access_policy:1;
		u32 disk_cache_policy:1;
		u32 reserved:27;

	} __attribute__ ((packed)) ld_operations;

	struct {

		u8 min;
		u8 max;
		u8 reserved[2];

	} __attribute__ ((packed)) stripe_sz_ops;

	struct {

		u32 force_online:1;
		u32 force_offline:1;
		u32 force_rebuild:1;
		u32 reserved:29;

	} __attribute__ ((packed)) pd_operations;

	struct {

		u32 ctrl_supports_sas:1;
		u32 ctrl_supports_sata:1;
		u32 allow_mix_in_encl:1;
		u32 allow_mix_in_ld:1;
		u32 allow_sata_in_cluster:1;
		u32 reserved:27;

	} __attribute__ ((packed)) pd_mix_support;

	
	u8 ecc_bucket_count;
	u8 reserved_2[11];

	
	struct megasas_ctrl_prop properties;

	
	char package_version[0x60];


	
	__le64	    deviceInterfacePortAddr2[8]; 
	u8          reserved3[128];              

	struct {                                
		u16 minPdRaidLevel_0:4;
		u16 maxPdRaidLevel_0:12;

		u16 minPdRaidLevel_1:4;
		u16 maxPdRaidLevel_1:12;

		u16 minPdRaidLevel_5:4;
		u16 maxPdRaidLevel_5:12;

		u16 minPdRaidLevel_1E:4;
		u16 maxPdRaidLevel_1E:12;

		u16 minPdRaidLevel_6:4;
		u16 maxPdRaidLevel_6:12;

		u16 minPdRaidLevel_10:4;
		u16 maxPdRaidLevel_10:12;

		u16 minPdRaidLevel_50:4;
		u16 maxPdRaidLevel_50:12;

		u16 minPdRaidLevel_60:4;
		u16 maxPdRaidLevel_60:12;

		u16 minPdRaidLevel_1E_RLQ0:4;
		u16 maxPdRaidLevel_1E_RLQ0:12;

		u16 minPdRaidLevel_1E0_RLQ0:4;
		u16 maxPdRaidLevel_1E0_RLQ0:12;

		u16 reserved[6];
	} pdsForRaidLevels;

	__le16 maxPds;                          
	__le16 maxDedHSPs;                      
	__le16 maxGlobalHSP;                    
	__le16 ddfSize;                         
	u8  maxLdsPerArray;                     
	u8  partitionsInDDF;                    
	u8  lockKeyBinding;                     
	u8  maxPITsPerLd;                       
	u8  maxViewsPerLd;                      
	u8  maxTargetId;                        
	__le16 maxBvlVdSize;                    

	__le16 maxConfigurableSSCSize;          
	__le16 currentSSCsize;                  

	char    expanderFwVersion[12];          

	__le16 PFKTrialTimeRemaining;           

	__le16 cacheMemorySize;                 

	struct {                                
#if   defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:5;
		u32	activePassive:2;
		u32	supportConfigAutoBalance:1;
		u32	mpio:1;
		u32	supportDataLDonSSCArray:1;
		u32	supportPointInTimeProgress:1;
		u32     supportUnevenSpans:1;
		u32     dedicatedHotSparesLimited:1;
		u32     headlessMode:1;
		u32     supportEmulatedDrives:1;
		u32     supportResetNow:1;
		u32     realTimeScheduler:1;
		u32     supportSSDPatrolRead:1;
		u32     supportPerfTuning:1;
		u32     disableOnlinePFKChange:1;
		u32     supportJBOD:1;
		u32     supportBootTimePFKChange:1;
		u32     supportSetLinkSpeed:1;
		u32     supportEmergencySpares:1;
		u32     supportSuspendResumeBGops:1;
		u32     blockSSDWriteCacheChange:1;
		u32     supportShieldState:1;
		u32     supportLdBBMInfo:1;
		u32     supportLdPIType3:1;
		u32     supportLdPIType2:1;
		u32     supportLdPIType1:1;
		u32     supportPIcontroller:1;
#else
		u32     supportPIcontroller:1;
		u32     supportLdPIType1:1;
		u32     supportLdPIType2:1;
		u32     supportLdPIType3:1;
		u32     supportLdBBMInfo:1;
		u32     supportShieldState:1;
		u32     blockSSDWriteCacheChange:1;
		u32     supportSuspendResumeBGops:1;
		u32     supportEmergencySpares:1;
		u32     supportSetLinkSpeed:1;
		u32     supportBootTimePFKChange:1;
		u32     supportJBOD:1;
		u32     disableOnlinePFKChange:1;
		u32     supportPerfTuning:1;
		u32     supportSSDPatrolRead:1;
		u32     realTimeScheduler:1;

		u32     supportResetNow:1;
		u32     supportEmulatedDrives:1;
		u32     headlessMode:1;
		u32     dedicatedHotSparesLimited:1;


		u32     supportUnevenSpans:1;
		u32	supportPointInTimeProgress:1;
		u32	supportDataLDonSSCArray:1;
		u32	mpio:1;
		u32	supportConfigAutoBalance:1;
		u32	activePassive:2;
		u32     reserved:5;
#endif
	} adapterOperations2;

	u8  driverVersion[32];                  
	u8  maxDAPdCountSpinup60;               
	u8  temperatureROC;                     
	u8  temperatureCtrl;                    
	u8  reserved4;                          
	__le16 maxConfigurablePds;              


	u8  reserved5[2];                       

	
	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:25;
		u32     passive:1;
		u32     premiumFeatureMismatch:1;
		u32     ctrlPropIncompatible:1;
		u32     fwVersionMismatch:1;
		u32     hwIncompatible:1;
		u32     peerIsIncompatible:1;
		u32     peerIsPresent:1;
#else
		u32     peerIsPresent:1;
		u32     peerIsIncompatible:1;
		u32     hwIncompatible:1;
		u32     fwVersionMismatch:1;
		u32     ctrlPropIncompatible:1;
		u32     premiumFeatureMismatch:1;
		u32     passive:1;
		u32     reserved:25;
#endif
	} cluster;

	char clusterId[MEGASAS_CLUSTER_ID_SIZE]; 
	struct {
		u8  maxVFsSupported;            
		u8  numVFsEnabled;              
		u8  requestorId;                
		u8  reserved;                   
	} iov;

	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
		u32     reserved:7;
		u32     useSeqNumJbodFP:1;
		u32     supportExtendedSSCSize:1;
		u32     supportDiskCacheSettingForSysPDs:1;
		u32     supportCPLDUpdate:1;
		u32     supportTTYLogCompression:1;
		u32     discardCacheDuringLDDelete:1;
		u32     supportSecurityonJBOD:1;
		u32     supportCacheBypassModes:1;
		u32     supportDisableSESMonitoring:1;
		u32     supportForceFlash:1;
		u32     supportNVDRAM:1;
		u32     supportDrvActivityLEDSetting:1;
		u32     supportAllowedOpsforDrvRemoval:1;
		u32     supportHOQRebuild:1;
		u32     supportForceTo512e:1;
		u32     supportNVCacheErase:1;
		u32     supportDebugQueue:1;
		u32     supportSwZone:1;
		u32     supportCrashDump:1;
		u32     supportMaxExtLDs:1;
		u32     supportT10RebuildAssist:1;
		u32     supportDisableImmediateIO:1;
		u32     supportThermalPollInterval:1;
		u32     supportPersonalityChange:2;
#else
		u32     supportPersonalityChange:2;
		u32     supportThermalPollInterval:1;
		u32     supportDisableImmediateIO:1;
		u32     supportT10RebuildAssist:1;
		u32	supportMaxExtLDs:1;
		u32	supportCrashDump:1;
		u32     supportSwZone:1;
		u32     supportDebugQueue:1;
		u32     supportNVCacheErase:1;
		u32     supportForceTo512e:1;
		u32     supportHOQRebuild:1;
		u32     supportAllowedOpsforDrvRemoval:1;
		u32     supportDrvActivityLEDSetting:1;
		u32     supportNVDRAM:1;
		u32     supportForceFlash:1;
		u32     supportDisableSESMonitoring:1;
		u32     supportCacheBypassModes:1;
		u32     supportSecurityonJBOD:1;
		u32     discardCacheDuringLDDelete:1;
		u32     supportTTYLogCompression:1;
		u32     supportCPLDUpdate:1;
		u32     supportDiskCacheSettingForSysPDs:1;
		u32     supportExtendedSSCSize:1;
		u32     useSeqNumJbodFP:1;
		u32     reserved:7;
#endif
	} adapterOperations3;

	struct {
#if defined(__BIG_ENDIAN_BITFIELD)
	u8 reserved:7;
	
	u8 cpld_in_flash:1;
#else
	u8 cpld_in_flash:1;
	u8 reserved:7;
#endif
	u8 reserved1[3];
	
	u8 userCodeDefinition[12];
	} cpld;  

	struct {
	#if defined(__BIG_ENDIAN_BITFIELD)
		u16 reserved:2;
		u16 support_nvme_passthru:1;
		u16 support_pl_debug_info:1;
		u16 support_flash_comp_info:1;
		u16 support_host_info:1;
		u16 support_dual_fw_update:1;
		u16 support_ssc_rev3:1;
		u16 fw_swaps_bbu_vpd_info:1;
		u16 support_pd_map_target_id:1;
		u16 support_ses_ctrl_in_multipathcfg:1;
		u16 image_upload_supported:1;
		u16 support_encrypted_mfc:1;
		u16 supported_enc_algo:1;
		u16 support_ibutton_less:1;
		u16 ctrl_info_ext_supported:1;
	#else

		u16 ctrl_info_ext_supported:1;
		u16 support_ibutton_less:1;
		u16 supported_enc_algo:1;
		u16 support_encrypted_mfc:1;
		u16 image_upload_supported:1;
		
		u16 support_ses_ctrl_in_multipathcfg:1;
		
		
		u16 support_pd_map_target_id:1;
		
		u16 fw_swaps_bbu_vpd_info:1;
		u16 support_ssc_rev3:1;
		
		u16 support_dual_fw_update:1;
		
		u16 support_host_info:1;
		
		u16 support_flash_comp_info:1;
		
		u16 support_pl_debug_info:1;
		
		u16 support_nvme_passthru:1;
		
		u16 reserved:2;
	#endif
		} adapter_operations4;
	u8 pad[0x800 - 0x7FE]; 

	u32 size;
	u32 pad1;

	u8 reserved6[64];

	struct {
	#if defined(__BIG_ENDIAN_BITFIELD)
		u32 reserved:19;
		u32 support_pci_lane_margining: 1;
		u32 support_psoc_update:1;
		u32 support_force_personality_change:1;
		u32 support_fde_type_mix:1;
		u32 support_snap_dump:1;
		u32 support_nvme_tm:1;
		u32 support_oce_only:1;
		u32 support_ext_mfg_vpd:1;
		u32 support_pcie:1;
		u32 support_cvhealth_info:1;
		u32 support_profile_change:2;
		u32 mr_config_ext2_supported:1;
	#else
		u32 mr_config_ext2_supported:1;
		u32 support_profile_change:2;
		u32 support_cvhealth_info:1;
		u32 support_pcie:1;
		u32 support_ext_mfg_vpd:1;
		u32 support_oce_only:1;
		u32 support_nvme_tm:1;
		u32 support_snap_dump:1;
		u32 support_fde_type_mix:1;
		u32 support_force_personality_change:1;
		u32 support_psoc_update:1;
		u32 support_pci_lane_margining: 1;
		u32 reserved:19;
	#endif
	} adapter_operations5;

	u32 rsvdForAdptOp[63];

	u8 reserved7[3];

	u8 TaskAbortTO;	
	u8 MaxResetTO;	
	u8 reserved8[3];
} __packed;


#define MEGASAS_MAX_PD_CHANNELS			2
#define MEGASAS_MAX_LD_CHANNELS			2
#define MEGASAS_MAX_CHANNELS			(MEGASAS_MAX_PD_CHANNELS + \
						MEGASAS_MAX_LD_CHANNELS)
#define MEGASAS_MAX_DEV_PER_CHANNEL		128
#define MEGASAS_DEFAULT_INIT_ID			-1
#define MEGASAS_MAX_LUN				8
#define MEGASAS_DEFAULT_CMD_PER_LUN		256
#define MEGASAS_MAX_PD                          (MEGASAS_MAX_PD_CHANNELS * \
						MEGASAS_MAX_DEV_PER_CHANNEL)
#define MEGASAS_MAX_LD_IDS			(MEGASAS_MAX_LD_CHANNELS * \
						MEGASAS_MAX_DEV_PER_CHANNEL)

#define MEGASAS_MAX_SUPPORTED_LD_IDS		240

#define MEGASAS_MAX_SECTORS                    (2*1024)
#define MEGASAS_MAX_SECTORS_IEEE		(2*128)
#define MEGASAS_DBG_LVL				1

#define MEGASAS_FW_BUSY				1


#define OCR_DEBUG    (1 << 0)
#define TM_DEBUG     (1 << 1)
#define LD_PD_DEBUG    (1 << 2)

#define SCAN_PD_CHANNEL	0x1
#define SCAN_VD_CHANNEL	0x2

#define MEGASAS_KDUMP_QUEUE_DEPTH               100
#define MR_LARGE_IO_MIN_SIZE			(32 * 1024)
#define MR_R1_LDIO_PIGGYBACK_DEFAULT		4

enum MR_SCSI_CMD_TYPE {
	READ_WRITE_LDIO = 0,
	NON_READ_WRITE_LDIO = 1,
	READ_WRITE_SYSPDIO = 2,
	NON_READ_WRITE_SYSPDIO = 3,
};

enum DCMD_TIMEOUT_ACTION {
	INITIATE_OCR = 0,
	KILL_ADAPTER = 1,
	IGNORE_TIMEOUT = 2,
};

enum FW_BOOT_CONTEXT {
	PROBE_CONTEXT = 0,
	OCR_CONTEXT = 1,
};


#define IO_FRAME				0
#define PTHRU_FRAME				1


#define MEGASAS_RESET_WAIT_TIME			180
#define MEGASAS_INTERNAL_CMD_WAIT_TIME		180
#define	MEGASAS_RESET_NOTICE_INTERVAL		5
#define MEGASAS_IOCTL_CMD			0
#define MEGASAS_DEFAULT_CMD_TIMEOUT		90
#define MEGASAS_THROTTLE_QUEUE_DEPTH		16
#define MEGASAS_DEFAULT_TM_TIMEOUT		50

#define MEGASAS_INT_CMDS			32
#define MEGASAS_SKINNY_INT_CMDS			5
#define MEGASAS_FUSION_INTERNAL_CMDS		8
#define MEGASAS_FUSION_IOCTL_CMDS		3
#define MEGASAS_MFI_IOCTL_CMDS			27

#define MEGASAS_MAX_MSIX_QUEUES			128

#define IS_DMA64				(sizeof(dma_addr_t) == 8)

#define MFI_XSCALE_OMR0_CHANGE_INTERRUPT		0x00000001

#define MFI_INTR_FLAG_REPLY_MESSAGE			0x00000001
#define MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE		0x00000002
#define MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT	0x00000004

#define MFI_OB_INTR_STATUS_MASK			0x00000002
#define MFI_POLL_TIMEOUT_SECS			60
#define MFI_IO_TIMEOUT_SECS			180
#define MEGASAS_SRIOV_HEARTBEAT_INTERVAL_VF	(5 * HZ)
#define MEGASAS_OCR_SETTLE_TIME_VF		(1000 * 30)
#define MEGASAS_SRIOV_MAX_RESET_TRIES_VF	1
#define MEGASAS_ROUTINE_WAIT_TIME_VF		300
#define MFI_REPLY_1078_MESSAGE_INTERRUPT	0x80000000
#define MFI_REPLY_GEN2_MESSAGE_INTERRUPT	0x00000001
#define MFI_GEN2_ENABLE_INTERRUPT_MASK		(0x00000001 | 0x00000004)
#define MFI_REPLY_SKINNY_MESSAGE_INTERRUPT	0x40000000
#define MFI_SKINNY_ENABLE_INTERRUPT_MASK	(0x00000001)

#define MFI_1068_PCSR_OFFSET			0x84
#define MFI_1068_FW_HANDSHAKE_OFFSET		0x64
#define MFI_1068_FW_READY			0xDDDD0000

#define MR_MAX_REPLY_QUEUES_OFFSET              0X0000001F
#define MR_MAX_REPLY_QUEUES_EXT_OFFSET          0X003FC000
#define MR_MAX_REPLY_QUEUES_EXT_OFFSET_SHIFT    14
#define MR_MAX_MSIX_REG_ARRAY                   16
#define MR_RDPQ_MODE_OFFSET			0X00800000

#define MR_MAX_RAID_MAP_SIZE_OFFSET_SHIFT	16
#define MR_MAX_RAID_MAP_SIZE_MASK		0x1FF
#define MR_MIN_MAP_SIZE				0x10000


#define MR_CAN_HANDLE_SYNC_CACHE_OFFSET		0X01000000

#define MR_ATOMIC_DESCRIPTOR_SUPPORT_OFFSET	(1 << 24)

#define MR_CAN_HANDLE_64_BIT_DMA_OFFSET		(1 << 25)
#define MR_INTR_COALESCING_SUPPORT_OFFSET	(1 << 26)

#define MEGASAS_WATCHDOG_THREAD_INTERVAL	1000
#define MEGASAS_WAIT_FOR_NEXT_DMA_MSECS		20
#define MEGASAS_WATCHDOG_WAIT_COUNT		50

enum MR_ADAPTER_TYPE {
	MFI_SERIES = 1,
	THUNDERBOLT_SERIES = 2,
	INVADER_SERIES = 3,
	VENTURA_SERIES = 4,
	AERO_SERIES = 5,
};



struct megasas_register_set {
	u32	doorbell;                       
	u32	fusion_seq_offset;		
	u32	fusion_host_diag;		
	u32	reserved_01;			

	u32 	inbound_msg_0;			
	u32 	inbound_msg_1;			
	u32 	outbound_msg_0;			
	u32 	outbound_msg_1;			

	u32 	inbound_doorbell;		
	u32 	inbound_intr_status;		
	u32 	inbound_intr_mask;		

	u32 	outbound_doorbell;		
	u32 	outbound_intr_status;		
	u32 	outbound_intr_mask;		

	u32 	reserved_1[2];			

	u32 	inbound_queue_port;		
	u32 	outbound_queue_port;		

	u32	reserved_2[9];			
	u32	reply_post_host_index;		
	u32	reserved_2_2[12];		

	u32 	outbound_doorbell_clear;	

	u32 	reserved_3[3];			

	u32	outbound_scratch_pad_0;		
	u32	outbound_scratch_pad_1;         
	u32	outbound_scratch_pad_2;         
	u32	outbound_scratch_pad_3;         

	u32 	inbound_low_queue_port ;	

	u32 	inbound_high_queue_port ;	

	u32 inbound_single_queue_port;	
	u32	res_6[11];			
	u32	host_diag;
	u32	seq_offset;
	u32 	index_registers[807];		
} __attribute__ ((packed));

struct megasas_sge32 {

	__le32 phys_addr;
	__le32 length;

} __attribute__ ((packed));

struct megasas_sge64 {

	__le64 phys_addr;
	__le32 length;

} __attribute__ ((packed));

struct megasas_sge_skinny {
	__le64 phys_addr;
	__le32 length;
	__le32 flag;
} __packed;

union megasas_sgl {
	DECLARE_FLEX_ARRAY(struct megasas_sge32, sge32);
	DECLARE_FLEX_ARRAY(struct megasas_sge64, sge64);
	DECLARE_FLEX_ARRAY(struct megasas_sge_skinny, sge_skinny);
} __attribute__ ((packed));

struct megasas_header {

	u8 cmd;			
	u8 sense_len;		
	u8 cmd_status;		
	u8 scsi_status;		

	u8 target_id;		
	u8 lun;			
	u8 cdb_len;		
	u8 sge_count;		

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 timeout;		
	__le32 data_xferlen;	

} __attribute__ ((packed));

union megasas_sgl_frame {

	struct megasas_sge32 sge32[8];
	struct megasas_sge64 sge64[5];

} __attribute__ ((packed));

typedef union _MFI_CAPABILITIES {
	struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
	u32     reserved:15;
	u32	support_memdump:1;
	u32	support_fw_exposed_dev_list:1;
	u32	support_nvme_passthru:1;
	u32     support_64bit_mode:1;
	u32 support_pd_map_target_id:1;
	u32     support_qd_throttling:1;
	u32     support_fp_rlbypass:1;
	u32     support_vfid_in_ioframe:1;
	u32     support_ext_io_size:1;
	u32		support_ext_queue_depth:1;
	u32     security_protocol_cmds_fw:1;
	u32     support_core_affinity:1;
	u32     support_ndrive_r1_lb:1;
	u32		support_max_255lds:1;
	u32		support_fastpath_wb:1;
	u32     support_additional_msix:1;
	u32     support_fp_remote_lun:1;
#else
	u32     support_fp_remote_lun:1;
	u32     support_additional_msix:1;
	u32		support_fastpath_wb:1;
	u32		support_max_255lds:1;
	u32     support_ndrive_r1_lb:1;
	u32     support_core_affinity:1;
	u32     security_protocol_cmds_fw:1;
	u32		support_ext_queue_depth:1;
	u32     support_ext_io_size:1;
	u32     support_vfid_in_ioframe:1;
	u32     support_fp_rlbypass:1;
	u32     support_qd_throttling:1;
	u32	support_pd_map_target_id:1;
	u32     support_64bit_mode:1;
	u32	support_nvme_passthru:1;
	u32	support_fw_exposed_dev_list:1;
	u32	support_memdump:1;
	u32     reserved:15;
#endif
	} mfi_capabilities;
	__le32		reg;
} MFI_CAPABILITIES;

struct megasas_init_frame {

	u8 cmd;			
	u8 reserved_0;		
	u8 cmd_status;		

	u8 reserved_1;		
	MFI_CAPABILITIES driver_operations; 

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 replyqueue_mask;		
	__le32 data_xfer_len;	

	__le32 queue_info_new_phys_addr_lo;	
	__le32 queue_info_new_phys_addr_hi;	
	__le32 queue_info_old_phys_addr_lo;	
	__le32 queue_info_old_phys_addr_hi;	
	__le32 reserved_4[2];	
	__le32 system_info_lo;      
	__le32 system_info_hi;      
	__le32 reserved_5[2];	

} __attribute__ ((packed));

struct megasas_init_queue_info {

	__le32 init_flags;		
	__le32 reply_queue_entries;	

	__le32 reply_queue_start_phys_addr_lo;	
	__le32 reply_queue_start_phys_addr_hi;	
	__le32 producer_index_phys_addr_lo;	
	__le32 producer_index_phys_addr_hi;	
	__le32 consumer_index_phys_addr_lo;	
	__le32 consumer_index_phys_addr_hi;	

} __attribute__ ((packed));

struct megasas_io_frame {

	u8 cmd;			
	u8 sense_len;		
	u8 cmd_status;		
	u8 scsi_status;		

	u8 target_id;		
	u8 access_byte;		
	u8 reserved_0;		
	u8 sge_count;		

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 timeout;		
	__le32 lba_count;	

	__le32 sense_buf_phys_addr_lo;	
	__le32 sense_buf_phys_addr_hi;	

	__le32 start_lba_lo;	
	__le32 start_lba_hi;	

	union megasas_sgl sgl;	

} __attribute__ ((packed));

struct megasas_pthru_frame {

	u8 cmd;			
	u8 sense_len;		
	u8 cmd_status;		
	u8 scsi_status;		

	u8 target_id;		
	u8 lun;			
	u8 cdb_len;		
	u8 sge_count;		

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 timeout;		
	__le32 data_xfer_len;	

	__le32 sense_buf_phys_addr_lo;	
	__le32 sense_buf_phys_addr_hi;	

	u8 cdb[16];		
	union megasas_sgl sgl;	

} __attribute__ ((packed));

struct megasas_dcmd_frame {

	u8 cmd;			
	u8 reserved_0;		
	u8 cmd_status;		
	u8 reserved_1[4];	
	u8 sge_count;		

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 timeout;		

	__le32 data_xfer_len;	
	__le32 opcode;		

	union {			
		u8 b[12];
		__le16 s[6];
		__le32 w[3];
	} mbox;

	union megasas_sgl sgl;	

} __attribute__ ((packed));

struct megasas_abort_frame {

	u8 cmd;			
	u8 reserved_0;		
	u8 cmd_status;		

	u8 reserved_1;		
	__le32 reserved_2;	

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 reserved_3;	
	__le32 reserved_4;	

	__le32 abort_context;	
	__le32 pad_1;		

	__le32 abort_mfi_phys_addr_lo;	
	__le32 abort_mfi_phys_addr_hi;	

	__le32 reserved_5[6];	

} __attribute__ ((packed));

struct megasas_smp_frame {

	u8 cmd;			
	u8 reserved_1;		
	u8 cmd_status;		
	u8 connection_status;	

	u8 reserved_2[3];	
	u8 sge_count;		

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 timeout;		

	__le32 data_xfer_len;	
	__le64 sas_addr;	

	union {
		struct megasas_sge32 sge32[2];	
		struct megasas_sge64 sge64[2];	
	} sgl;

} __attribute__ ((packed));

struct megasas_stp_frame {

	u8 cmd;			
	u8 reserved_1;		
	u8 cmd_status;		
	u8 reserved_2;		

	u8 target_id;		
	u8 reserved_3[2];	
	u8 sge_count;		

	__le32 context;		
	__le32 pad_0;		

	__le16 flags;		
	__le16 timeout;		

	__le32 data_xfer_len;	

	__le16 fis[10];		
	__le32 stp_flags;

	union {
		struct megasas_sge32 sge32[2];	
		struct megasas_sge64 sge64[2];	
	} sgl;

} __attribute__ ((packed));

union megasas_frame {

	struct megasas_header hdr;
	struct megasas_init_frame init;
	struct megasas_io_frame io;
	struct megasas_pthru_frame pthru;
	struct megasas_dcmd_frame dcmd;
	struct megasas_abort_frame abort;
	struct megasas_smp_frame smp;
	struct megasas_stp_frame stp;

	u8 raw_bytes[64];
};


struct MR_PRIV_DEVICE {
	bool is_tm_capable;
	bool tm_busy;
	atomic_t sdev_priv_busy;
	atomic_t r1_ldio_hint;
	u8 interface_type;
	u8 task_abort_tmo;
	u8 target_reset_tmo;
};
struct megasas_cmd;

union megasas_evt_class_locale {

	struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u16 locale;
		u8 reserved;
		s8 class;
#else
		s8 class;
		u8 reserved;
		u16 locale;
#endif
	} __attribute__ ((packed)) members;

	u32 word;

} __attribute__ ((packed));

struct megasas_evt_log_info {
	__le32 newest_seq_num;
	__le32 oldest_seq_num;
	__le32 clear_seq_num;
	__le32 shutdown_seq_num;
	__le32 boot_seq_num;

} __attribute__ ((packed));

struct megasas_progress {

	__le16 progress;
	__le16 elapsed_seconds;

} __attribute__ ((packed));

struct megasas_evtarg_ld {

	u16 target_id;
	u8 ld_index;
	u8 reserved;

} __attribute__ ((packed));

struct megasas_evtarg_pd {
	u16 device_id;
	u8 encl_index;
	u8 slot_number;

} __attribute__ ((packed));

struct megasas_evt_detail {

	__le32 seq_num;
	__le32 time_stamp;
	__le32 code;
	union megasas_evt_class_locale cl;
	u8 arg_type;
	u8 reserved1[15];

	union {
		struct {
			struct megasas_evtarg_pd pd;
			u8 cdb_length;
			u8 sense_length;
			u8 reserved[2];
			u8 cdb[16];
			u8 sense[64];
		} __attribute__ ((packed)) cdbSense;

		struct megasas_evtarg_ld ld;

		struct {
			struct megasas_evtarg_ld ld;
			__le64 count;
		} __attribute__ ((packed)) ld_count;

		struct {
			__le64 lba;
			struct megasas_evtarg_ld ld;
		} __attribute__ ((packed)) ld_lba;

		struct {
			struct megasas_evtarg_ld ld;
			__le32 prevOwner;
			__le32 newOwner;
		} __attribute__ ((packed)) ld_owner;

		struct {
			u64 ld_lba;
			u64 pd_lba;
			struct megasas_evtarg_ld ld;
			struct megasas_evtarg_pd pd;
		} __attribute__ ((packed)) ld_lba_pd_lba;

		struct {
			struct megasas_evtarg_ld ld;
			struct megasas_progress prog;
		} __attribute__ ((packed)) ld_prog;

		struct {
			struct megasas_evtarg_ld ld;
			u32 prev_state;
			u32 new_state;
		} __attribute__ ((packed)) ld_state;

		struct {
			u64 strip;
			struct megasas_evtarg_ld ld;
		} __attribute__ ((packed)) ld_strip;

		struct megasas_evtarg_pd pd;

		struct {
			struct megasas_evtarg_pd pd;
			u32 err;
		} __attribute__ ((packed)) pd_err;

		struct {
			u64 lba;
			struct megasas_evtarg_pd pd;
		} __attribute__ ((packed)) pd_lba;

		struct {
			u64 lba;
			struct megasas_evtarg_pd pd;
			struct megasas_evtarg_ld ld;
		} __attribute__ ((packed)) pd_lba_ld;

		struct {
			struct megasas_evtarg_pd pd;
			struct megasas_progress prog;
		} __attribute__ ((packed)) pd_prog;

		struct {
			struct megasas_evtarg_pd pd;
			u32 prevState;
			u32 newState;
		} __attribute__ ((packed)) pd_state;

		struct {
			u16 vendorId;
			__le16 deviceId;
			u16 subVendorId;
			u16 subDeviceId;
		} __attribute__ ((packed)) pci;

		u32 rate;
		char str[96];

		struct {
			u32 rtc;
			u32 elapsedSeconds;
		} __attribute__ ((packed)) time;

		struct {
			u32 ecar;
			u32 elog;
			char str[64];
		} __attribute__ ((packed)) ecc;

		u8 b[96];
		__le16 s[48];
		__le32 w[24];
		__le64 d[12];
	} args;

	char description[128];

} __attribute__ ((packed));

struct megasas_aen_event {
	struct delayed_work hotplug_work;
	struct megasas_instance *instance;
};

struct megasas_irq_context {
	char name[MEGASAS_MSIX_NAME_LEN];
	struct megasas_instance *instance;
	u32 MSIxIndex;
	u32 os_irq;
	struct irq_poll irqpoll;
	bool irq_poll_scheduled;
	bool irq_line_enable;
	atomic_t   in_used;
};

struct MR_DRV_SYSTEM_INFO {
	u8	infoVersion;
	u8	systemIdLength;
	u16	reserved0;
	u8	systemId[64];
	u8	reserved[1980];
};

enum MR_PD_TYPE {
	UNKNOWN_DRIVE = 0,
	PARALLEL_SCSI = 1,
	SAS_PD = 2,
	SATA_PD = 3,
	FC_PD = 4,
	NVME_PD = 5,
};


#define MEGASAS_SATA_QD	32
#define MEGASAS_SAS_QD 256
#define MEGASAS_DEFAULT_PD_QD	64
#define MEGASAS_NVME_QD        64

#define MR_DEFAULT_NVME_PAGE_SIZE	4096
#define MR_DEFAULT_NVME_PAGE_SHIFT	12
#define MR_DEFAULT_NVME_MDTS_KB		128
#define MR_NVME_PAGE_SIZE_MASK		0x000000FF


#define MR_HIGH_IOPS_QUEUE_COUNT	8
#define MR_DEVICE_HIGH_IOPS_DEPTH	8
#define MR_HIGH_IOPS_BATCH_COUNT	16

enum MR_PERF_MODE {
	MR_BALANCED_PERF_MODE		= 0,
	MR_IOPS_PERF_MODE		= 1,
	MR_LATENCY_PERF_MODE		= 2,
};

#define MEGASAS_PERF_MODE_2STR(mode) \
		((mode) == MR_BALANCED_PERF_MODE ? "Balanced" : \
		 (mode) == MR_IOPS_PERF_MODE ? "IOPS" : \
		 (mode) == MR_LATENCY_PERF_MODE ? "Latency" : \
		 "Unknown")

enum MEGASAS_LD_TARGET_ID_STATUS {
	LD_TARGET_ID_INITIAL,
	LD_TARGET_ID_ACTIVE,
	LD_TARGET_ID_DELETED,
};

#define MEGASAS_TARGET_ID(sdev)						\
	(((sdev->channel % 2) * MEGASAS_MAX_DEV_PER_CHANNEL) + sdev->id)

struct megasas_instance {

	unsigned int *reply_map;
	__le32 *producer;
	dma_addr_t producer_h;
	__le32 *consumer;
	dma_addr_t consumer_h;
	struct MR_DRV_SYSTEM_INFO *system_info_buf;
	dma_addr_t system_info_h;
	struct MR_LD_VF_AFFILIATION *vf_affiliation;
	dma_addr_t vf_affiliation_h;
	struct MR_LD_VF_AFFILIATION_111 *vf_affiliation_111;
	dma_addr_t vf_affiliation_111_h;
	struct MR_CTRL_HB_HOST_MEM *hb_host_mem;
	dma_addr_t hb_host_mem_h;
	struct MR_PD_INFO *pd_info;
	dma_addr_t pd_info_h;
	struct MR_TARGET_PROPERTIES *tgt_prop;
	dma_addr_t tgt_prop_h;

	__le32 *reply_queue;
	dma_addr_t reply_queue_h;

	u32 *crash_dump_buf;
	dma_addr_t crash_dump_h;

	struct MR_PD_LIST *pd_list_buf;
	dma_addr_t pd_list_buf_h;

	struct megasas_ctrl_info *ctrl_info_buf;
	dma_addr_t ctrl_info_buf_h;

	struct MR_LD_LIST *ld_list_buf;
	dma_addr_t ld_list_buf_h;

	struct MR_LD_TARGETID_LIST *ld_targetid_list_buf;
	dma_addr_t ld_targetid_list_buf_h;

	struct MR_HOST_DEVICE_LIST *host_device_list_buf;
	dma_addr_t host_device_list_buf_h;

	struct MR_SNAPDUMP_PROPERTIES *snapdump_prop;
	dma_addr_t snapdump_prop_h;

	void *crash_buf[MAX_CRASH_DUMP_SIZE];
	unsigned int    fw_crash_buffer_size;
	unsigned int    fw_crash_state;
	unsigned int    fw_crash_buffer_offset;
	u32 drv_buf_index;
	u32 drv_buf_alloc;
	u32 crash_dump_fw_support;
	u32 crash_dump_drv_support;
	u32 crash_dump_app_support;
	u32 secure_jbod_support;
	u32 support_morethan256jbod; 
	bool use_seqnum_jbod_fp;   
	bool smp_affinity_enable;
	struct mutex crashdump_lock;

	struct megasas_register_set __iomem *reg_set;
	u32 __iomem *reply_post_host_index_addr[MR_MAX_MSIX_REG_ARRAY];
	struct megasas_pd_list          pd_list[MEGASAS_MAX_PD];
	struct megasas_pd_list          local_pd_list[MEGASAS_MAX_PD];
	u8 ld_ids[MEGASAS_MAX_LD_IDS];
	u8 ld_tgtid_status[MEGASAS_MAX_LD_IDS];
	u8 ld_ids_prev[MEGASAS_MAX_LD_IDS];
	u8 ld_ids_from_raidmap[MEGASAS_MAX_LD_IDS];
	s8 init_id;

	u16 max_num_sge;
	u16 max_fw_cmds;
	u16 max_mpt_cmds;
	u16 max_mfi_cmds;
	u16 max_scsi_cmds;
	u16 ldio_threshold;
	u16 cur_can_queue;
	u32 max_sectors_per_req;
	bool msix_load_balance;
	struct megasas_aen_event *ev;

	struct megasas_cmd **cmd_list;
	struct list_head cmd_pool;
	
	spinlock_t mfi_pool_lock;
	
	spinlock_t hba_lock;
	
	spinlock_t stream_lock;
	spinlock_t completion_lock;
	struct dma_pool *frame_dma_pool;
	struct dma_pool *sense_dma_pool;

	struct megasas_evt_detail *evt_detail;
	dma_addr_t evt_detail_h;
	struct megasas_cmd *aen_cmd;
	struct semaphore ioctl_sem;

	struct Scsi_Host *host;

	wait_queue_head_t int_cmd_wait_q;
	wait_queue_head_t abort_cmd_wait_q;

	struct pci_dev *pdev;
	u32 unique_id;
	u32 fw_support_ieee;
	u32 threshold_reply_count;

	atomic_t fw_outstanding;
	atomic_t ldio_outstanding;
	atomic_t fw_reset_no_pci_access;
	atomic64_t total_io_count;
	atomic64_t high_iops_outstanding;

	struct megasas_instance_template *instancet;
	struct tasklet_struct isr_tasklet;
	struct work_struct work_init;
	struct delayed_work fw_fault_work;
	struct workqueue_struct *fw_fault_work_q;
	char fault_handler_work_q_name[48];

	u8 flag;
	u8 unload;
	u8 flag_ieee;
	u8 issuepend_done;
	u8 disableOnlineCtrlReset;
	u8 UnevenSpanSupport;

	u8 supportmax256vd;
	u8 pd_list_not_supported;
	u16 fw_supported_vd_count;
	u16 fw_supported_pd_count;

	u16 drv_supported_vd_count;
	u16 drv_supported_pd_count;

	atomic_t adprecovery;
	unsigned long last_time;
	u32 mfiStatus;
	u32 last_seq_num;

	struct list_head internal_reset_pending_q;

	
	void *ctrl_context;
	unsigned int msix_vectors;
	struct megasas_irq_context irq_context[MEGASAS_MAX_MSIX_QUEUES];
	u64 map_id;
	u64 pd_seq_map_id;
	struct megasas_cmd *map_update_cmd;
	struct megasas_cmd *jbod_seq_cmd;
	unsigned long bar;
	long reset_flags;
	struct mutex reset_mutex;
	struct timer_list sriov_heartbeat_timer;
	char skip_heartbeat_timer_del;
	u8 requestorId;
	char PlasmaFW111;
	char clusterId[MEGASAS_CLUSTER_ID_SIZE];
	u8 peerIsPresent;
	u8 passive;
	u16 throttlequeuedepth;
	u8 mask_interrupts;
	u16 max_chain_frame_sz;
	u8 is_imr;
	u8 is_rdpq;
	bool dev_handle;
	bool fw_sync_cache_support;
	u32 mfi_frame_size;
	bool msix_combined;
	u16 max_raid_mapsize;
	
	u8  r1_ldio_hint_default;
	u32 nvme_page_size;
	u8 adapter_type;
	bool consistent_mask_64bit;
	bool support_nvme_passthru;
	bool enable_sdev_max_qd;
	u8 task_abort_tmo;
	u8 max_reset_tmo;
	u8 snapdump_wait_time;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
	struct dentry *raidmap_dump;
#endif
	u8 enable_fw_dev_list;
	bool atomic_desc_support;
	bool support_seqnum_jbod_fp;
	bool support_pci_lane_margining;
	u8  low_latency_index_start;
	int perf_mode;
	int iopoll_q_count;
};

struct MR_LD_VF_MAP {
	u32 size;
	union MR_LD_REF ref;
	u8 ldVfCount;
	u8 reserved[6];
	u8 policy[1];
};

struct MR_LD_VF_AFFILIATION {
	u32 size;
	u8 ldCount;
	u8 vfCount;
	u8 thisVf;
	u8 reserved[9];
	struct MR_LD_VF_MAP map[1];
};


#define IOV_111_OFFSET 0x7CE
#define MAX_VIRTUAL_FUNCTIONS 8
#define MR_LD_ACCESS_HIDDEN 15

struct IOV_111 {
	u8 maxVFsSupported;
	u8 numVFsEnabled;
	u8 requestorId;
	u8 reserved[5];
};

struct MR_LD_VF_MAP_111 {
	u8 targetId;
	u8 reserved[3];
	u8 policy[MAX_VIRTUAL_FUNCTIONS];
};

struct MR_LD_VF_AFFILIATION_111 {
	u8 vdCount;
	u8 vfCount;
	u8 thisVf;
	u8 reserved[5];
	struct MR_LD_VF_MAP_111 map[MAX_LOGICAL_DRIVES];
};

struct MR_CTRL_HB_HOST_MEM {
	struct {
		u32 fwCounter;	
		struct {
			u32 debugmode:1; 
			u32 reserved:31;
		} debug;
		u32 reserved_fw[6];
		u32 driverCounter; 
		u32 reserved_driver[7];
	} HB;
	u8 pad[0x400-0x40];
};

enum {
	MEGASAS_HBA_OPERATIONAL			= 0,
	MEGASAS_ADPRESET_SM_INFAULT		= 1,
	MEGASAS_ADPRESET_SM_FW_RESET_SUCCESS	= 2,
	MEGASAS_ADPRESET_SM_OPERATIONAL		= 3,
	MEGASAS_HW_CRITICAL_ERROR		= 4,
	MEGASAS_ADPRESET_SM_POLLING		= 5,
	MEGASAS_ADPRESET_INPROG_SIGN		= 0xDEADDEAD,
};

struct megasas_instance_template {
	void (*fire_cmd)(struct megasas_instance *, dma_addr_t, \
		u32, struct megasas_register_set __iomem *);

	void (*enable_intr)(struct megasas_instance *);
	void (*disable_intr)(struct megasas_instance *);

	int (*clear_intr)(struct megasas_instance *);

	u32 (*read_fw_status_reg)(struct megasas_instance *);
	int (*adp_reset)(struct megasas_instance *, \
		struct megasas_register_set __iomem *);
	int (*check_reset)(struct megasas_instance *, \
		struct megasas_register_set __iomem *);
	irqreturn_t (*service_isr)(int irq, void *devp);
	void (*tasklet)(unsigned long);
	u32 (*init_adapter)(struct megasas_instance *);
	u32 (*build_and_issue_cmd) (struct megasas_instance *,
				    struct scsi_cmnd *);
	void (*issue_dcmd)(struct megasas_instance *instance,
			    struct megasas_cmd *cmd);
};

#define MEGASAS_IS_LOGICAL(sdev)					\
	((sdev->channel < MEGASAS_MAX_PD_CHANNELS) ? 0 : 1)

#define MEGASAS_IS_LUN_VALID(sdev)					\
	(((sdev)->lun == 0) ? 1 : 0)

#define MEGASAS_DEV_INDEX(scp)						\
	(((scp->device->channel % 2) * MEGASAS_MAX_DEV_PER_CHANNEL) +	\
	scp->device->id)

#define MEGASAS_PD_INDEX(scp)						\
	((scp->device->channel * MEGASAS_MAX_DEV_PER_CHANNEL) +		\
	scp->device->id)

struct megasas_cmd {

	union megasas_frame *frame;
	dma_addr_t frame_phys_addr;
	u8 *sense;
	dma_addr_t sense_phys_addr;

	u32 index;
	u8 sync_cmd;
	u8 cmd_status_drv;
	u8 abort_aen;
	u8 retry_for_fw_reset;


	struct list_head list;
	struct scsi_cmnd *scmd;
	u8 flags;

	struct megasas_instance *instance;
	union {
		struct {
			u16 smid;
			u16 resvd;
		} context;
		u32 frame_count;
	};
};

struct megasas_cmd_priv {
	void	*cmd_priv;
	u8	status;
};

static inline struct megasas_cmd_priv *megasas_priv(struct scsi_cmnd *cmd)
{
	return scsi_cmd_priv(cmd);
}

#define MAX_MGMT_ADAPTERS		1024
#define MAX_IOCTL_SGE			16

struct megasas_iocpacket {

	u16 host_no;
	u16 __pad1;
	u32 sgl_off;
	u32 sge_count;
	u32 sense_off;
	u32 sense_len;
	union {
		u8 raw[128];
		struct megasas_header hdr;
	} frame;

	struct iovec sgl[MAX_IOCTL_SGE];

} __attribute__ ((packed));

struct megasas_aen {
	u16 host_no;
	u16 __pad1;
	u32 seq_num;
	u32 class_locale_word;
} __attribute__ ((packed));

struct compat_megasas_iocpacket {
	u16 host_no;
	u16 __pad1;
	u32 sgl_off;
	u32 sge_count;
	u32 sense_off;
	u32 sense_len;
	union {
		u8 raw[128];
		struct megasas_header hdr;
	} frame;
	struct compat_iovec sgl[MAX_IOCTL_SGE];
} __attribute__ ((packed));

#define MEGASAS_IOC_FIRMWARE32	_IOWR('M', 1, struct compat_megasas_iocpacket)

#define MEGASAS_IOC_FIRMWARE	_IOWR('M', 1, struct megasas_iocpacket)
#define MEGASAS_IOC_GET_AEN	_IOW('M', 3, struct megasas_aen)

struct megasas_mgmt_info {

	u16 count;
	struct megasas_instance *instance[MAX_MGMT_ADAPTERS];
	int max_index;
};

enum MEGASAS_OCR_CAUSE {
	FW_FAULT_OCR			= 0,
	SCSIIO_TIMEOUT_OCR		= 1,
	MFI_IO_TIMEOUT_OCR		= 2,
};

enum DCMD_RETURN_STATUS {
	DCMD_SUCCESS    = 0x00,
	DCMD_TIMEOUT    = 0x01,
	DCMD_FAILED     = 0x02,
	DCMD_BUSY       = 0x03,
	DCMD_INIT       = 0xff,
};

u8
MR_BuildRaidContext(struct megasas_instance *instance,
		    struct IO_REQUEST_INFO *io_info,
		    struct RAID_CONTEXT *pRAID_Context,
		    struct MR_DRV_RAID_MAP_ALL *map, u8 **raidLUN);
u16 MR_TargetIdToLdGet(u32 ldTgtId, struct MR_DRV_RAID_MAP_ALL *map);
struct MR_LD_RAID *MR_LdRaidGet(u32 ld, struct MR_DRV_RAID_MAP_ALL *map);
u16 MR_ArPdGet(u32 ar, u32 arm, struct MR_DRV_RAID_MAP_ALL *map);
u16 MR_LdSpanArrayGet(u32 ld, u32 span, struct MR_DRV_RAID_MAP_ALL *map);
__le16 MR_PdDevHandleGet(u32 pd, struct MR_DRV_RAID_MAP_ALL *map);
u16 MR_GetLDTgtId(u32 ld, struct MR_DRV_RAID_MAP_ALL *map);

__le16 get_updated_dev_handle(struct megasas_instance *instance,
			      struct LD_LOAD_BALANCE_INFO *lbInfo,
			      struct IO_REQUEST_INFO *in_info,
			      struct MR_DRV_RAID_MAP_ALL *drv_map);
void mr_update_load_balance_params(struct MR_DRV_RAID_MAP_ALL *map,
	struct LD_LOAD_BALANCE_INFO *lbInfo);
int megasas_get_ctrl_info(struct megasas_instance *instance);

int
megasas_sync_pd_seq_num(struct megasas_instance *instance, bool pend);
void megasas_set_dynamic_target_properties(struct scsi_device *sdev,
					   bool is_target_prop);
int megasas_get_target_prop(struct megasas_instance *instance,
			    struct scsi_device *sdev);
void megasas_get_snapdump_properties(struct megasas_instance *instance);

int megasas_set_crash_dump_params(struct megasas_instance *instance,
	u8 crash_buf_state);
void megasas_free_host_crash_buffer(struct megasas_instance *instance);

void megasas_return_cmd_fusion(struct megasas_instance *instance,
	struct megasas_cmd_fusion *cmd);
int megasas_issue_blocked_cmd(struct megasas_instance *instance,
	struct megasas_cmd *cmd, int timeout);
void __megasas_return_cmd(struct megasas_instance *instance,
	struct megasas_cmd *cmd);

void megasas_return_mfi_mpt_pthr(struct megasas_instance *instance,
	struct megasas_cmd *cmd_mfi, struct megasas_cmd_fusion *cmd_fusion);
int megasas_cmd_type(struct scsi_cmnd *cmd);
void megasas_setup_jbod_map(struct megasas_instance *instance);

void megasas_update_sdev_properties(struct scsi_device *sdev);
int megasas_reset_fusion(struct Scsi_Host *shost, int reason);
int megasas_task_abort_fusion(struct scsi_cmnd *scmd);
int megasas_reset_target_fusion(struct scsi_cmnd *scmd);
u32 mega_mod64(u64 dividend, u32 divisor);
int megasas_alloc_fusion_context(struct megasas_instance *instance);
void megasas_free_fusion_context(struct megasas_instance *instance);
int megasas_fusion_start_watchdog(struct megasas_instance *instance);
void megasas_fusion_stop_watchdog(struct megasas_instance *instance);

void megasas_set_dma_settings(struct megasas_instance *instance,
			      struct megasas_dcmd_frame *dcmd,
			      dma_addr_t dma_addr, u32 dma_len);
int megasas_adp_reset_wait_for_ready(struct megasas_instance *instance,
				     bool do_adp_reset,
				     int ocr_context);
int megasas_irqpoll(struct irq_poll *irqpoll, int budget);
void megasas_dump_fusion_io(struct scsi_cmnd *scmd);
u32 megasas_readl(struct megasas_instance *instance,
		  const volatile void __iomem *addr);
struct megasas_cmd *megasas_get_cmd(struct megasas_instance *instance);
void megasas_return_cmd(struct megasas_instance *instance,
			struct megasas_cmd *cmd);
int megasas_issue_polled(struct megasas_instance *instance,
			 struct megasas_cmd *cmd);
void megaraid_sas_kill_hba(struct megasas_instance *instance);
void megasas_check_and_restore_queue_depth(struct megasas_instance *instance);
void megasas_start_timer(struct megasas_instance *instance);
int megasas_sriov_start_heartbeat(struct megasas_instance *instance,
				  int initial);
int megasas_alloc_cmds(struct megasas_instance *instance);
void megasas_free_cmds(struct megasas_instance *instance);

void megasas_init_debugfs(void);
void megasas_exit_debugfs(void);
void megasas_setup_debugfs(struct megasas_instance *instance);
void megasas_destroy_debugfs(struct megasas_instance *instance);
int megasas_blk_mq_poll(struct Scsi_Host *shost, unsigned int queue_num);

#endif				
