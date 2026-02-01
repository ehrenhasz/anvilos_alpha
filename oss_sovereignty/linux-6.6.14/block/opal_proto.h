 
 
#include <linux/types.h>

#ifndef _OPAL_PROTO_H
#define _OPAL_PROTO_H

 
enum {
	TCG_SECP_00 = 0,
	TCG_SECP_01,
};

 
enum opal_response_token {
	OPAL_DTA_TOKENID_BYTESTRING = 0xe0,
	OPAL_DTA_TOKENID_SINT = 0xe1,
	OPAL_DTA_TOKENID_UINT = 0xe2,
	OPAL_DTA_TOKENID_TOKEN = 0xe3,  
	OPAL_DTA_TOKENID_INVALID = 0X0
};

#define DTAERROR_NO_METHOD_STATUS 0x89
#define GENERIC_HOST_SESSION_NUM 0x41
#define FIRST_TPER_SESSION_NUM	4096

#define TPER_SYNC_SUPPORTED 0x01
 
#define LOCKING_SUPPORTED_MASK 0x01
#define LOCKING_ENABLED_MASK 0x02
#define LOCKED_MASK 0x04
#define MBR_ENABLED_MASK 0x10
#define MBR_DONE_MASK 0x20

#define TINY_ATOM_DATA_MASK 0x3F
#define TINY_ATOM_SIGNED 0x40

#define SHORT_ATOM_ID 0x80
#define SHORT_ATOM_BYTESTRING 0x20
#define SHORT_ATOM_SIGNED 0x10
#define SHORT_ATOM_LEN_MASK 0xF

#define MEDIUM_ATOM_ID 0xC0
#define MEDIUM_ATOM_BYTESTRING 0x10
#define MEDIUM_ATOM_SIGNED 0x8
#define MEDIUM_ATOM_LEN_MASK 0x7

#define LONG_ATOM_ID 0xe0
#define LONG_ATOM_BYTESTRING 0x2
#define LONG_ATOM_SIGNED 0x1

 
#define TINY_ATOM_BYTE   0x7F
#define SHORT_ATOM_BYTE  0xBF
#define MEDIUM_ATOM_BYTE 0xDF
#define LONG_ATOM_BYTE   0xE3

#define OPAL_INVAL_PARAM 12
#define OPAL_MANUFACTURED_INACTIVE 0x08
#define OPAL_DISCOVERY_COMID 0x0001

#define LOCKING_RANGE_NON_GLOBAL 0x03
 
#define OPAL_METHOD_LENGTH 8
#define OPAL_MSID_KEYLEN 15
#define OPAL_UID_LENGTH_HALF 4

 
#define OPAL_BOOLEAN_AND 0
#define OPAL_BOOLEAN_OR  1
#define OPAL_BOOLEAN_NOT 2

 
enum opal_uid {
	 
	OPAL_SMUID_UID,
	OPAL_THISSP_UID,
	OPAL_ADMINSP_UID,
	OPAL_LOCKINGSP_UID,
	OPAL_ENTERPRISE_LOCKINGSP_UID,
	OPAL_ANYBODY_UID,
	OPAL_SID_UID,
	OPAL_ADMIN1_UID,
	OPAL_USER1_UID,
	OPAL_USER2_UID,
	OPAL_PSID_UID,
	OPAL_ENTERPRISE_BANDMASTER0_UID,
	OPAL_ENTERPRISE_ERASEMASTER_UID,
	 
	OPAL_TABLE_TABLE,
	OPAL_LOCKINGRANGE_GLOBAL,
	OPAL_LOCKINGRANGE_ACE_START_TO_KEY,
	OPAL_LOCKINGRANGE_ACE_RDLOCKED,
	OPAL_LOCKINGRANGE_ACE_WRLOCKED,
	OPAL_MBRCONTROL,
	OPAL_MBR,
	OPAL_AUTHORITY_TABLE,
	OPAL_C_PIN_TABLE,
	OPAL_LOCKING_INFO_TABLE,
	OPAL_ENTERPRISE_LOCKING_INFO_TABLE,
	OPAL_DATASTORE,
	 
	OPAL_C_PIN_MSID,
	OPAL_C_PIN_SID,
	OPAL_C_PIN_ADMIN1,
	 
	OPAL_HALF_UID_AUTHORITY_OBJ_REF,
	OPAL_HALF_UID_BOOLEAN_ACE,
	 
	OPAL_UID_HEXFF,
};

 
enum opal_method {
	OPAL_PROPERTIES,
	OPAL_STARTSESSION,
	OPAL_REVERT,
	OPAL_ACTIVATE,
	OPAL_EGET,
	OPAL_ESET,
	OPAL_NEXT,
	OPAL_EAUTHENTICATE,
	OPAL_GETACL,
	OPAL_GENKEY,
	OPAL_REVERTSP,
	OPAL_GET,
	OPAL_SET,
	OPAL_AUTHENTICATE,
	OPAL_RANDOM,
	OPAL_ERASE,
};

enum opal_token {
	 
	OPAL_TRUE = 0x01,
	OPAL_FALSE = 0x00,
	OPAL_BOOLEAN_EXPR = 0x03,
	 
	OPAL_TABLE = 0x00,
	OPAL_STARTROW = 0x01,
	OPAL_ENDROW = 0x02,
	OPAL_STARTCOLUMN = 0x03,
	OPAL_ENDCOLUMN = 0x04,
	OPAL_VALUES = 0x01,
	 
	OPAL_TABLE_UID = 0x00,
	OPAL_TABLE_NAME = 0x01,
	OPAL_TABLE_COMMON = 0x02,
	OPAL_TABLE_TEMPLATE = 0x03,
	OPAL_TABLE_KIND = 0x04,
	OPAL_TABLE_COLUMN = 0x05,
	OPAL_TABLE_COLUMNS = 0x06,
	OPAL_TABLE_ROWS = 0x07,
	OPAL_TABLE_ROWS_FREE = 0x08,
	OPAL_TABLE_ROW_BYTES = 0x09,
	OPAL_TABLE_LASTID = 0x0A,
	OPAL_TABLE_MIN = 0x0B,
	OPAL_TABLE_MAX = 0x0C,
	 
	OPAL_PIN = 0x03,
	 
	OPAL_RANGESTART = 0x03,
	OPAL_RANGELENGTH = 0x04,
	OPAL_READLOCKENABLED = 0x05,
	OPAL_WRITELOCKENABLED = 0x06,
	OPAL_READLOCKED = 0x07,
	OPAL_WRITELOCKED = 0x08,
	OPAL_ACTIVEKEY = 0x0A,
	 
	OPAL_LIFECYCLE = 0x06,
	 
	OPAL_MAXRANGES = 0x04,
	 
	OPAL_MBRENABLE = 0x01,
	OPAL_MBRDONE = 0x02,
	 
	OPAL_HOSTPROPERTIES = 0x00,
	 
	OPAL_STARTLIST = 0xf0,
	OPAL_ENDLIST = 0xf1,
	OPAL_STARTNAME = 0xf2,
	OPAL_ENDNAME = 0xf3,
	OPAL_CALL = 0xf8,
	OPAL_ENDOFDATA = 0xf9,
	OPAL_ENDOFSESSION = 0xfa,
	OPAL_STARTTRANSACTON = 0xfb,
	OPAL_ENDTRANSACTON = 0xfC,
	OPAL_EMPTYATOM = 0xff,
	OPAL_WHERE = 0x00,
};

 
enum opal_lockingstate {
	OPAL_LOCKING_READWRITE = 0x01,
	OPAL_LOCKING_READONLY = 0x02,
	OPAL_LOCKING_LOCKED = 0x03,
};

enum opal_parameter {
	OPAL_SUM_SET_LIST = 0x060000,
};

enum opal_revertlsp {
	OPAL_KEEP_GLOBAL_RANGE_KEY = 0x060000,
};

 

 
struct opal_compacket {
	__be32 reserved0;
	u8 extendedComID[4];
	__be32 outstandingData;
	__be32 minTransfer;
	__be32 length;
};

 
struct opal_packet {
	__be32 tsn;
	__be32 hsn;
	__be32 seq_number;
	__be16 reserved0;
	__be16 ack_type;
	__be32 acknowledgment;
	__be32 length;
};

 
struct opal_data_subpacket {
	u8 reserved0[6];
	__be16 kind;
	__be32 length;
};

 
struct opal_header {
	struct opal_compacket cp;
	struct opal_packet pkt;
	struct opal_data_subpacket subpkt;
};

#define FC_TPER       0x0001
#define FC_LOCKING    0x0002
#define FC_GEOMETRY   0x0003
#define FC_ENTERPRISE 0x0100
#define FC_DATASTORE  0x0202
#define FC_SINGLEUSER 0x0201
#define FC_OPALV100   0x0200
#define FC_OPALV200   0x0203

 
struct d0_header {
	__be32 length;  
	__be32 revision;  
	__be32 reserved01;
	__be32 reserved02;
	 
	u8 ignored[32];
};

 
struct d0_tper_features {
	 
	u8 supported_features;
	 
	u8 reserved01[3];
	__be32 reserved02;
	__be32 reserved03;
};

 
struct d0_locking_features {
	 
	u8 supported_features;
	 
	u8 reserved01[3];
	__be32 reserved02;
	__be32 reserved03;
};

 
struct d0_geometry_features {
	 
	u8 header[4];
	 
	u8 reserved01;
	u8 reserved02[7];
	__be32 logical_block_size;
	__be64 alignment_granularity;
	__be64 lowest_aligned_lba;
};

 
struct d0_enterprise_ssc {
	__be16 baseComID;
	__be16 numComIDs;
	 
	u8 range_crossing;
	u8 reserved01;
	__be16 reserved02;
	__be32 reserved03;
	__be32 reserved04;
};

 
struct d0_opal_v100 {
	__be16 baseComID;
	__be16 numComIDs;
};

 
struct d0_single_user_mode {
	__be32 num_locking_objects;
	 
	u8 reserved01;
	u8 reserved02;
	__be16 reserved03;
	__be32 reserved04;
};

 
struct d0_datastore_table {
	__be16 reserved01;
	__be16 max_tables;
	__be32 max_size_tables;
	__be32 table_size_alignment;
};

 
struct d0_opal_v200 {
	__be16 baseComID;
	__be16 numComIDs;
	 
	u8 range_crossing;
	 
	u8 num_locking_admin_auth[2];
	 
	u8 num_locking_user_auth[2];
	u8 initialPIN;
	u8 revertedPIN;
	u8 reserved01;
	__be32 reserved02;
};

 
struct d0_features {
	__be16 code;
	 
	u8 r_version;
	u8 length;
	u8 features[];
};

#endif  
