 
 


#ifndef _LINUX_NTFS3_NTFS_H
#define _LINUX_NTFS3_NTFS_H

#include <linux/blkdev.h>
#include <linux/build_bug.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>

#include "debug.h"

 

 
#define NTFS3_CHECK_FREE_CLST

#define NTFS_NAME_LEN 255

 
#define NTFS_LINK_MAX 4000

 


#define NTFS_LZNT_MAX_CLUSTER	4096
#define NTFS_LZNT_CUNIT		4
#define NTFS_LZNT_CLUSTERS	(1u<<NTFS_LZNT_CUNIT)

struct GUID {
	__le32 Data1;
	__le16 Data2;
	__le16 Data3;
	u8 Data4[8];
};

 
struct cpu_str {
	u8 len;
	u8 unused;
	u16 name[10];
};

struct le_str {
	u8 len;
	u8 unused;
	__le16 name[];
};

static_assert(SECTOR_SHIFT == 9);

#ifdef CONFIG_NTFS3_64BIT_CLUSTER
typedef u64 CLST;
static_assert(sizeof(size_t) == 8);
#else
typedef u32 CLST;
#endif

#define SPARSE_LCN64   ((u64)-1)
#define SPARSE_LCN     ((CLST)-1)
#define RESIDENT_LCN   ((CLST)-2)
#define COMPRESSED_LCN ((CLST)-3)

#define COMPRESSION_UNIT     4
#define COMPRESS_MAX_CLUSTER 0x1000

enum RECORD_NUM {
	MFT_REC_MFT		= 0,
	MFT_REC_MIRR		= 1,
	MFT_REC_LOG		= 2,
	MFT_REC_VOL		= 3,
	MFT_REC_ATTR		= 4,
	MFT_REC_ROOT		= 5,
	MFT_REC_BITMAP		= 6,
	MFT_REC_BOOT		= 7,
	MFT_REC_BADCLUST	= 8,
	MFT_REC_SECURE		= 9,
	MFT_REC_UPCASE		= 10,
	MFT_REC_EXTEND		= 11,
	MFT_REC_RESERVED	= 12,
	MFT_REC_FREE		= 16,
	MFT_REC_USER		= 24,
};

enum ATTR_TYPE {
	ATTR_ZERO		= cpu_to_le32(0x00),
	ATTR_STD		= cpu_to_le32(0x10),
	ATTR_LIST		= cpu_to_le32(0x20),
	ATTR_NAME		= cpu_to_le32(0x30),
	ATTR_ID			= cpu_to_le32(0x40),
	ATTR_SECURE		= cpu_to_le32(0x50),
	ATTR_LABEL		= cpu_to_le32(0x60),
	ATTR_VOL_INFO		= cpu_to_le32(0x70),
	ATTR_DATA		= cpu_to_le32(0x80),
	ATTR_ROOT		= cpu_to_le32(0x90),
	ATTR_ALLOC		= cpu_to_le32(0xA0),
	ATTR_BITMAP		= cpu_to_le32(0xB0),
	ATTR_REPARSE		= cpu_to_le32(0xC0),
	ATTR_EA_INFO		= cpu_to_le32(0xD0),
	ATTR_EA			= cpu_to_le32(0xE0),
	ATTR_PROPERTYSET	= cpu_to_le32(0xF0),
	ATTR_LOGGED_UTILITY_STREAM = cpu_to_le32(0x100),
	ATTR_END		= cpu_to_le32(0xFFFFFFFF)
};

static_assert(sizeof(enum ATTR_TYPE) == 4);

enum FILE_ATTRIBUTE {
	FILE_ATTRIBUTE_READONLY		= cpu_to_le32(0x00000001),
	FILE_ATTRIBUTE_HIDDEN		= cpu_to_le32(0x00000002),
	FILE_ATTRIBUTE_SYSTEM		= cpu_to_le32(0x00000004),
	FILE_ATTRIBUTE_ARCHIVE		= cpu_to_le32(0x00000020),
	FILE_ATTRIBUTE_DEVICE		= cpu_to_le32(0x00000040),
	FILE_ATTRIBUTE_TEMPORARY	= cpu_to_le32(0x00000100),
	FILE_ATTRIBUTE_SPARSE_FILE	= cpu_to_le32(0x00000200),
	FILE_ATTRIBUTE_REPARSE_POINT	= cpu_to_le32(0x00000400),
	FILE_ATTRIBUTE_COMPRESSED	= cpu_to_le32(0x00000800),
	FILE_ATTRIBUTE_OFFLINE		= cpu_to_le32(0x00001000),
	FILE_ATTRIBUTE_NOT_CONTENT_INDEXED = cpu_to_le32(0x00002000),
	FILE_ATTRIBUTE_ENCRYPTED	= cpu_to_le32(0x00004000),
	FILE_ATTRIBUTE_VALID_FLAGS	= cpu_to_le32(0x00007fb7),
	FILE_ATTRIBUTE_DIRECTORY	= cpu_to_le32(0x10000000),
	FILE_ATTRIBUTE_INDEX		= cpu_to_le32(0x20000000)
};

static_assert(sizeof(enum FILE_ATTRIBUTE) == 4);

extern const struct cpu_str NAME_MFT;
extern const struct cpu_str NAME_MIRROR;
extern const struct cpu_str NAME_LOGFILE;
extern const struct cpu_str NAME_VOLUME;
extern const struct cpu_str NAME_ATTRDEF;
extern const struct cpu_str NAME_ROOT;
extern const struct cpu_str NAME_BITMAP;
extern const struct cpu_str NAME_BOOT;
extern const struct cpu_str NAME_BADCLUS;
extern const struct cpu_str NAME_QUOTA;
extern const struct cpu_str NAME_SECURE;
extern const struct cpu_str NAME_UPCASE;
extern const struct cpu_str NAME_EXTEND;
extern const struct cpu_str NAME_OBJID;
extern const struct cpu_str NAME_REPARSE;
extern const struct cpu_str NAME_USNJRNL;

extern const __le16 I30_NAME[4];
extern const __le16 SII_NAME[4];
extern const __le16 SDH_NAME[4];
extern const __le16 SO_NAME[2];
extern const __le16 SQ_NAME[2];
extern const __le16 SR_NAME[2];

extern const __le16 BAD_NAME[4];
extern const __le16 SDS_NAME[4];
extern const __le16 WOF_NAME[17];	 

 
struct MFT_REF {
	__le32 low;	
	__le16 high;	
	__le16 seq;	
};

static_assert(sizeof(__le64) == sizeof(struct MFT_REF));

static inline CLST ino_get(const struct MFT_REF *ref)
{
#ifdef CONFIG_NTFS3_64BIT_CLUSTER
	return le32_to_cpu(ref->low) | ((u64)le16_to_cpu(ref->high) << 32);
#else
	return le32_to_cpu(ref->low);
#endif
}

struct NTFS_BOOT {
	u8 jump_code[3];	
	u8 system_id[8];	

	
	
	
	u8 bytes_per_sector[2];	

	u8 sectors_per_clusters;
	u8 unused1[7];
	u8 media_type;		
	u8 unused2[2];
	__le16 sct_per_track;	
	__le16 heads;		
	__le32 hidden_sectors;	
	u8 unused3[4];
	u8 bios_drive_num;	
	u8 unused4;
	u8 signature_ex;	
	u8 unused5;
	__le64 sectors_per_volume;
	__le64 mft_clst;	
	__le64 mft2_clst;	
	s8 record_size;		
	u8 unused6[3];
	s8 index_size;		
	u8 unused7[3];
	__le64 serial_num;	
	__le32 check_sum;	
				

	u8 boot_code[0x200 - 0x50 - 2 - 4]; 
	u8 boot_magic[2];	
};

static_assert(sizeof(struct NTFS_BOOT) == 0x200);

enum NTFS_SIGNATURE {
	NTFS_FILE_SIGNATURE = cpu_to_le32(0x454C4946), 
	NTFS_INDX_SIGNATURE = cpu_to_le32(0x58444E49), 
	NTFS_CHKD_SIGNATURE = cpu_to_le32(0x444B4843), 
	NTFS_RSTR_SIGNATURE = cpu_to_le32(0x52545352), 
	NTFS_RCRD_SIGNATURE = cpu_to_le32(0x44524352), 
	NTFS_BAAD_SIGNATURE = cpu_to_le32(0x44414142), 
	NTFS_HOLE_SIGNATURE = cpu_to_le32(0x454C4F48), 
	NTFS_FFFF_SIGNATURE = cpu_to_le32(0xffffffff),
};

static_assert(sizeof(enum NTFS_SIGNATURE) == 4);

 
struct NTFS_RECORD_HEADER {
	 
	enum NTFS_SIGNATURE sign; 
	__le16 fix_off;		
	__le16 fix_num;		
	__le64 lsn;		
};

static_assert(sizeof(struct NTFS_RECORD_HEADER) == 0x10);

static inline int is_baad(const struct NTFS_RECORD_HEADER *hdr)
{
	return hdr->sign == NTFS_BAAD_SIGNATURE;
}

 
enum RECORD_FLAG {
	RECORD_FLAG_IN_USE	= cpu_to_le16(0x0001),
	RECORD_FLAG_DIR		= cpu_to_le16(0x0002),
	RECORD_FLAG_SYSTEM	= cpu_to_le16(0x0004),
	RECORD_FLAG_INDEX	= cpu_to_le16(0x0008),
};

 
struct MFT_REC {
	struct NTFS_RECORD_HEADER rhdr; 

	__le16 seq;		
	__le16 hard_links;	
	__le16 attr_off;	
	__le16 flags;		
	__le32 used;		
	__le32 total;		

	struct MFT_REF parent_ref; 
	__le16 next_attr_id;	

	__le16 res;		
	__le32 mft_record;	
	__le16 fixups[];	
};

#define MFTRECORD_FIXUP_OFFSET_1 offsetof(struct MFT_REC, res)
#define MFTRECORD_FIXUP_OFFSET_3 offsetof(struct MFT_REC, fixups)
 
#define MFTRECORD_FIXUP_OFFSET  MFTRECORD_FIXUP_OFFSET_1

static_assert(MFTRECORD_FIXUP_OFFSET_1 == 0x2A);
static_assert(MFTRECORD_FIXUP_OFFSET_3 == 0x30);

static inline bool is_rec_base(const struct MFT_REC *rec)
{
	const struct MFT_REF *r = &rec->parent_ref;

	return !r->low && !r->high && !r->seq;
}

static inline bool is_mft_rec5(const struct MFT_REC *rec)
{
	return le16_to_cpu(rec->rhdr.fix_off) >=
	       offsetof(struct MFT_REC, fixups);
}

static inline bool is_rec_inuse(const struct MFT_REC *rec)
{
	return rec->flags & RECORD_FLAG_IN_USE;
}

static inline bool clear_rec_inuse(struct MFT_REC *rec)
{
	return rec->flags &= ~RECORD_FLAG_IN_USE;
}

 
#define RESIDENT_FLAG_INDEXED 0x01

struct ATTR_RESIDENT {
	__le32 data_size;	
	__le16 data_off;	
	u8 flags;		
	u8 res;			
}; 

struct ATTR_NONRESIDENT {
	__le64 svcn;		
	__le64 evcn;		
	__le16 run_off;		
	
	
	
	
	
	
	
        
        
	
	
	
	u8 c_unit;		
	u8 res1[5];		
	__le64 alloc_size;	
				
	__le64 data_size;	
	__le64 valid_size;	
	__le64 total_size;	
				
				

}; 

 
#define ATTR_FLAG_COMPRESSED	  cpu_to_le16(0x0001)
#define ATTR_FLAG_COMPRESSED_MASK cpu_to_le16(0x00FF)
#define ATTR_FLAG_ENCRYPTED	  cpu_to_le16(0x4000)
#define ATTR_FLAG_SPARSED	  cpu_to_le16(0x8000)

struct ATTRIB {
	enum ATTR_TYPE type;	
	__le32 size;		
	u8 non_res;		
	u8 name_len;		
	__le16 name_off;	
	__le16 flags;		
	__le16 id;		

	union {
		struct ATTR_RESIDENT res;     
		struct ATTR_NONRESIDENT nres; 
	};
};

 
#define SIZEOF_RESIDENT			0x18
#define SIZEOF_NONRESIDENT_EX		0x48
#define SIZEOF_NONRESIDENT		0x40

#define SIZEOF_RESIDENT_LE		cpu_to_le16(0x18)
#define SIZEOF_NONRESIDENT_EX_LE	cpu_to_le16(0x48)
#define SIZEOF_NONRESIDENT_LE		cpu_to_le16(0x40)

static inline u64 attr_ondisk_size(const struct ATTRIB *attr)
{
	return attr->non_res ? ((attr->flags &
				 (ATTR_FLAG_COMPRESSED | ATTR_FLAG_SPARSED)) ?
					le64_to_cpu(attr->nres.total_size) :
					le64_to_cpu(attr->nres.alloc_size))
			     : ALIGN(le32_to_cpu(attr->res.data_size), 8);
}

static inline u64 attr_size(const struct ATTRIB *attr)
{
	return attr->non_res ? le64_to_cpu(attr->nres.data_size) :
			       le32_to_cpu(attr->res.data_size);
}

static inline bool is_attr_encrypted(const struct ATTRIB *attr)
{
	return attr->flags & ATTR_FLAG_ENCRYPTED;
}

static inline bool is_attr_sparsed(const struct ATTRIB *attr)
{
	return attr->flags & ATTR_FLAG_SPARSED;
}

static inline bool is_attr_compressed(const struct ATTRIB *attr)
{
	return attr->flags & ATTR_FLAG_COMPRESSED;
}

static inline bool is_attr_ext(const struct ATTRIB *attr)
{
	return attr->flags & (ATTR_FLAG_SPARSED | ATTR_FLAG_COMPRESSED);
}

static inline bool is_attr_indexed(const struct ATTRIB *attr)
{
	return !attr->non_res && (attr->res.flags & RESIDENT_FLAG_INDEXED);
}

static inline __le16 const *attr_name(const struct ATTRIB *attr)
{
	return Add2Ptr(attr, le16_to_cpu(attr->name_off));
}

static inline u64 attr_svcn(const struct ATTRIB *attr)
{
	return attr->non_res ? le64_to_cpu(attr->nres.svcn) : 0;
}

static_assert(sizeof(struct ATTRIB) == 0x48);
static_assert(sizeof(((struct ATTRIB *)NULL)->res) == 0x08);
static_assert(sizeof(((struct ATTRIB *)NULL)->nres) == 0x38);

static inline void *resident_data_ex(const struct ATTRIB *attr, u32 datasize)
{
	u32 asize, rsize;
	u16 off;

	if (attr->non_res)
		return NULL;

	asize = le32_to_cpu(attr->size);
	off = le16_to_cpu(attr->res.data_off);

	if (asize < datasize + off)
		return NULL;

	rsize = le32_to_cpu(attr->res.data_size);
	if (rsize < datasize)
		return NULL;

	return Add2Ptr(attr, off);
}

static inline void *resident_data(const struct ATTRIB *attr)
{
	return Add2Ptr(attr, le16_to_cpu(attr->res.data_off));
}

static inline void *attr_run(const struct ATTRIB *attr)
{
	return Add2Ptr(attr, le16_to_cpu(attr->nres.run_off));
}

 
struct ATTR_STD_INFO {
	__le64 cr_time;		
	__le64 m_time;		
	__le64 c_time;		
	__le64 a_time;		
	enum FILE_ATTRIBUTE fa;	
	__le32 max_ver_num;	
	__le32 ver_num;		
	__le32 class_id;	
};

static_assert(sizeof(struct ATTR_STD_INFO) == 0x30);

#define SECURITY_ID_INVALID 0x00000000
#define SECURITY_ID_FIRST 0x00000100

struct ATTR_STD_INFO5 {
	__le64 cr_time;		
	__le64 m_time;		
	__le64 c_time;		
	__le64 a_time;		
	enum FILE_ATTRIBUTE fa;	
	__le32 max_ver_num;	
	__le32 ver_num;		
	__le32 class_id;	

	__le32 owner_id;	
	__le32 security_id;	
	__le64 quota_charge;	
	__le64 usn;		
				
				
};

static_assert(sizeof(struct ATTR_STD_INFO5) == 0x48);

 
struct ATTR_LIST_ENTRY {
	enum ATTR_TYPE type;	
	__le16 size;		
	u8 name_len;		
	u8 name_off;		
	__le64 vcn;		
	struct MFT_REF ref;	
	__le16 id;		
	__le16 name[3];		

}; 

static_assert(sizeof(struct ATTR_LIST_ENTRY) == 0x20);

static inline u32 le_size(u8 name_len)
{
	return ALIGN(offsetof(struct ATTR_LIST_ENTRY, name) +
		     name_len * sizeof(short), 8);
}

 
static inline int le_cmp(const struct ATTR_LIST_ENTRY *le,
			 const struct ATTRIB *attr)
{
	return le->type != attr->type || le->name_len != attr->name_len ||
	       (!le->name_len &&
		memcmp(Add2Ptr(le, le->name_off),
		       Add2Ptr(attr, le16_to_cpu(attr->name_off)),
		       le->name_len * sizeof(short)));
}

static inline __le16 const *le_name(const struct ATTR_LIST_ENTRY *le)
{
	return Add2Ptr(le, le->name_off);
}

 
#define FILE_NAME_POSIX   0
#define FILE_NAME_UNICODE 1
#define FILE_NAME_DOS	  2
#define FILE_NAME_UNICODE_AND_DOS (FILE_NAME_DOS | FILE_NAME_UNICODE)

 
struct NTFS_DUP_INFO {
	__le64 cr_time;		
	__le64 m_time;		
	__le64 c_time;		
	__le64 a_time;		
	__le64 alloc_size;	
	__le64 data_size;	
	enum FILE_ATTRIBUTE fa;	
	__le16 ea_size;		
	__le16 reparse;		

}; 

struct ATTR_FILE_NAME {
	struct MFT_REF home;	
	struct NTFS_DUP_INFO dup;
	u8 name_len;		
	u8 type;		
	__le16 name[];		
};

static_assert(sizeof(((struct ATTR_FILE_NAME *)NULL)->dup) == 0x38);
static_assert(offsetof(struct ATTR_FILE_NAME, name) == 0x42);
#define SIZEOF_ATTRIBUTE_FILENAME     0x44
#define SIZEOF_ATTRIBUTE_FILENAME_MAX (0x42 + 255 * 2)

static inline struct ATTRIB *attr_from_name(struct ATTR_FILE_NAME *fname)
{
	return (struct ATTRIB *)((char *)fname - SIZEOF_RESIDENT);
}

static inline u16 fname_full_size(const struct ATTR_FILE_NAME *fname)
{
	 
	return offsetof(struct ATTR_FILE_NAME, name) +
	       fname->name_len * sizeof(short);
}

static inline u8 paired_name(u8 type)
{
	if (type == FILE_NAME_UNICODE)
		return FILE_NAME_DOS;
	if (type == FILE_NAME_DOS)
		return FILE_NAME_UNICODE;
	return FILE_NAME_POSIX;
}

 
#define NTFS_IE_HAS_SUBNODES	cpu_to_le16(1)
#define NTFS_IE_LAST		cpu_to_le16(2)

 
struct NTFS_DE {
	union {
		struct MFT_REF ref; 
		struct {
			__le16 data_off;  
			__le16 data_size; 
			__le32 res;	  
		} view;
	};
	__le16 size;		
	__le16 key_size;	
	__le16 flags;		
	__le16 res;		

	
	
	
	

	
	
	
	
	
};

static_assert(sizeof(struct NTFS_DE) == 0x10);

static inline void de_set_vbn_le(struct NTFS_DE *e, __le64 vcn)
{
	__le64 *v = Add2Ptr(e, le16_to_cpu(e->size) - sizeof(__le64));

	*v = vcn;
}

static inline void de_set_vbn(struct NTFS_DE *e, CLST vcn)
{
	__le64 *v = Add2Ptr(e, le16_to_cpu(e->size) - sizeof(__le64));

	*v = cpu_to_le64(vcn);
}

static inline __le64 de_get_vbn_le(const struct NTFS_DE *e)
{
	return *(__le64 *)Add2Ptr(e, le16_to_cpu(e->size) - sizeof(__le64));
}

static inline CLST de_get_vbn(const struct NTFS_DE *e)
{
	__le64 *v = Add2Ptr(e, le16_to_cpu(e->size) - sizeof(__le64));

	return le64_to_cpu(*v);
}

static inline struct NTFS_DE *de_get_next(const struct NTFS_DE *e)
{
	return Add2Ptr(e, le16_to_cpu(e->size));
}

static inline struct ATTR_FILE_NAME *de_get_fname(const struct NTFS_DE *e)
{
	return le16_to_cpu(e->key_size) >= SIZEOF_ATTRIBUTE_FILENAME ?
		       Add2Ptr(e, sizeof(struct NTFS_DE)) :
		       NULL;
}

static inline bool de_is_last(const struct NTFS_DE *e)
{
	return e->flags & NTFS_IE_LAST;
}

static inline bool de_has_vcn(const struct NTFS_DE *e)
{
	return e->flags & NTFS_IE_HAS_SUBNODES;
}

static inline bool de_has_vcn_ex(const struct NTFS_DE *e)
{
	return (e->flags & NTFS_IE_HAS_SUBNODES) &&
	       (u64)(-1) != *((u64 *)Add2Ptr(e, le16_to_cpu(e->size) -
							sizeof(__le64)));
}

#define MAX_BYTES_PER_NAME_ENTRY \
	ALIGN(sizeof(struct NTFS_DE) + \
	      offsetof(struct ATTR_FILE_NAME, name) + \
	      NTFS_NAME_LEN * sizeof(short), 8)

struct INDEX_HDR {
	__le32 de_off;	
			
	__le32 used;	
			
	__le32 total;	
	u8 flags;	
	u8 res[3];

	
	
	
};

static_assert(sizeof(struct INDEX_HDR) == 0x10);

static inline struct NTFS_DE *hdr_first_de(const struct INDEX_HDR *hdr)
{
	u32 de_off = le32_to_cpu(hdr->de_off);
	u32 used = le32_to_cpu(hdr->used);
	struct NTFS_DE *e;
	u16 esize;

	if (de_off >= used || de_off + sizeof(struct NTFS_DE) > used )
		return NULL;

	e = Add2Ptr(hdr, de_off);
	esize = le16_to_cpu(e->size);
	if (esize < sizeof(struct NTFS_DE) || de_off + esize > used)
		return NULL;

	return e;
}

static inline struct NTFS_DE *hdr_next_de(const struct INDEX_HDR *hdr,
					  const struct NTFS_DE *e)
{
	size_t off = PtrOffset(hdr, e);
	u32 used = le32_to_cpu(hdr->used);
	u16 esize;

	if (off >= used)
		return NULL;

	esize = le16_to_cpu(e->size);

	if (esize < sizeof(struct NTFS_DE) ||
	    off + esize + sizeof(struct NTFS_DE) > used)
		return NULL;

	return Add2Ptr(e, esize);
}

static inline bool hdr_has_subnode(const struct INDEX_HDR *hdr)
{
	return hdr->flags & 1;
}

struct INDEX_BUFFER {
	struct NTFS_RECORD_HEADER rhdr; 
	__le64 vbn; 
	struct INDEX_HDR ihdr; 
};

static_assert(sizeof(struct INDEX_BUFFER) == 0x28);

static inline bool ib_is_empty(const struct INDEX_BUFFER *ib)
{
	const struct NTFS_DE *first = hdr_first_de(&ib->ihdr);

	return !first || de_is_last(first);
}

static inline bool ib_is_leaf(const struct INDEX_BUFFER *ib)
{
	return !(ib->ihdr.flags & 1);
}

 
enum COLLATION_RULE {
	NTFS_COLLATION_TYPE_BINARY	= cpu_to_le32(0),
	
	NTFS_COLLATION_TYPE_FILENAME	= cpu_to_le32(0x01),
	
	NTFS_COLLATION_TYPE_UINT	= cpu_to_le32(0x10),
	
	NTFS_COLLATION_TYPE_SID		= cpu_to_le32(0x11),
	
	NTFS_COLLATION_TYPE_SECURITY_HASH = cpu_to_le32(0x12),
	
	NTFS_COLLATION_TYPE_UINTS	= cpu_to_le32(0x13)
};

static_assert(sizeof(enum COLLATION_RULE) == 4);


struct INDEX_ROOT {
	enum ATTR_TYPE type;	
	enum COLLATION_RULE rule; 
	__le32 index_block_size;
	u8 index_block_clst;	
	u8 res[3];
	struct INDEX_HDR ihdr;	
};

static_assert(sizeof(struct INDEX_ROOT) == 0x20);
static_assert(offsetof(struct INDEX_ROOT, ihdr) == 0x10);

#define VOLUME_FLAG_DIRTY	    cpu_to_le16(0x0001)
#define VOLUME_FLAG_RESIZE_LOG_FILE cpu_to_le16(0x0002)

struct VOLUME_INFO {
	__le64 res1;	
	u8 major_ver;	
	u8 minor_ver;	
	__le16 flags;	

}; 

#define SIZEOF_ATTRIBUTE_VOLUME_INFO 0xc

#define NTFS_LABEL_MAX_LENGTH		(0x100 / sizeof(short))
#define NTFS_ATTR_INDEXABLE		cpu_to_le32(0x00000002)
#define NTFS_ATTR_DUPALLOWED		cpu_to_le32(0x00000004)
#define NTFS_ATTR_MUST_BE_INDEXED	cpu_to_le32(0x00000010)
#define NTFS_ATTR_MUST_BE_NAMED		cpu_to_le32(0x00000020)
#define NTFS_ATTR_MUST_BE_RESIDENT	cpu_to_le32(0x00000040)
#define NTFS_ATTR_LOG_ALWAYS		cpu_to_le32(0x00000080)

 
struct ATTR_DEF_ENTRY {
	__le16 name[0x40];	
	enum ATTR_TYPE type;	
	__le32 res;		
	enum COLLATION_RULE rule; 
	__le32 flags;		
	__le64 min_sz;		
	__le64 max_sz;		
};

static_assert(sizeof(struct ATTR_DEF_ENTRY) == 0xa0);

 
struct OBJECT_ID {
	struct GUID ObjId;	

	
	
	struct GUID BirthVolumeId; 

	
	
	
	
	struct GUID BirthObjectId; 

	
	
	
	
	struct GUID DomainId;	
};

static_assert(sizeof(struct OBJECT_ID) == 0x40);

 
struct NTFS_DE_O {
	struct NTFS_DE de;
	struct GUID ObjId;	
	struct MFT_REF ref;	

	
	
	struct GUID BirthVolumeId; 

	
	
	
	
	
	struct GUID BirthObjectId; 

	
	
	
	
	struct GUID BirthDomainId; 
};

static_assert(sizeof(struct NTFS_DE_O) == 0x58);

 
struct NTFS_DE_Q {
	struct NTFS_DE de;
	__le32 owner_id;	

	 
	__le32 Version;		
	__le32 Flags;		
	__le64 BytesUsed;	
	__le64 ChangeTime;	
	__le64 WarningLimit;	
	__le64 HardLimit;	
	__le64 ExceededTime;	

	
}__packed; 

static_assert(sizeof(struct NTFS_DE_Q) == 0x44);

#define SecurityDescriptorsBlockSize 0x40000 
#define SecurityDescriptorMaxSize    0x20000 
#define Log2OfSecurityDescriptorsBlockSize 18

struct SECURITY_KEY {
	__le32 hash; 
	__le32 sec_id; 
};

 
struct SECURITY_HDR {
	struct SECURITY_KEY key;	
	__le64 off;			
	__le32 size;			
	 
} __packed;

static_assert(sizeof(struct SECURITY_HDR) == 0x14);

 
struct NTFS_DE_SII {
	struct NTFS_DE de;
	__le32 sec_id;			
	struct SECURITY_HDR sec_hdr;	
} __packed;

static_assert(offsetof(struct NTFS_DE_SII, sec_hdr) == 0x14);
static_assert(sizeof(struct NTFS_DE_SII) == 0x28);

 
struct NTFS_DE_SDH {
	struct NTFS_DE de;
	struct SECURITY_KEY key;	
	struct SECURITY_HDR sec_hdr;	
	__le16 magic[2];		
};

#define SIZEOF_SDH_DIRENTRY 0x30

struct REPARSE_KEY {
	__le32 ReparseTag;		
	struct MFT_REF ref;		
}; 

static_assert(offsetof(struct REPARSE_KEY, ref) == 0x04);
#define SIZEOF_REPARSE_KEY 0x0C

 
struct NTFS_DE_R {
	struct NTFS_DE de;
	struct REPARSE_KEY key;		
	u32 zero;			
}; 

static_assert(sizeof(struct NTFS_DE_R) == 0x20);

 
#define WOF_CURRENT_VERSION		cpu_to_le32(1)
 
#define WOF_PROVIDER_WIM		cpu_to_le32(1)
 
#define WOF_PROVIDER_SYSTEM		cpu_to_le32(2)
 
#define WOF_PROVIDER_CURRENT_VERSION	cpu_to_le32(1)

#define WOF_COMPRESSION_XPRESS4K	cpu_to_le32(0) 
#define WOF_COMPRESSION_LZX32K		cpu_to_le32(1) 
#define WOF_COMPRESSION_XPRESS8K	cpu_to_le32(2) 
#define WOF_COMPRESSION_XPRESS16K	cpu_to_le32(3) 

 
struct REPARSE_POINT {
	__le32 ReparseTag;	
	__le16 ReparseDataLength;
	__le16 Reserved;

	struct GUID Guid;	

	
	
	
};

static_assert(sizeof(struct REPARSE_POINT) == 0x18);

 
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE	(16 * 1024)

 
#define IO_REPARSE_TAG_RESERVED_RANGE		1

 

 
#define IsReparseTagMicrosoft(_tag)	(((_tag)&IO_REPARSE_TAG_MICROSOFT))

 
#define IsReparseTagNameSurrogate(_tag)	(((_tag)&IO_REPARSE_TAG_NAME_SURROGATE))

 
#define IO_REPARSE_TAG_VALID_VALUES	0xF000FFFF

 
#define IsReparseTagValid(_tag)						       \
	(!((_tag) & ~IO_REPARSE_TAG_VALID_VALUES) &&			       \
	 ((_tag) > IO_REPARSE_TAG_RESERVED_RANGE))

 

enum IO_REPARSE_TAG {
	IO_REPARSE_TAG_SYMBOLIC_LINK	= cpu_to_le32(0),
	IO_REPARSE_TAG_NAME_SURROGATE	= cpu_to_le32(0x20000000),
	IO_REPARSE_TAG_MICROSOFT	= cpu_to_le32(0x80000000),
	IO_REPARSE_TAG_MOUNT_POINT	= cpu_to_le32(0xA0000003),
	IO_REPARSE_TAG_SYMLINK		= cpu_to_le32(0xA000000C),
	IO_REPARSE_TAG_HSM		= cpu_to_le32(0xC0000004),
	IO_REPARSE_TAG_SIS		= cpu_to_le32(0x80000007),
	IO_REPARSE_TAG_DEDUP		= cpu_to_le32(0x80000013),
	IO_REPARSE_TAG_COMPRESS		= cpu_to_le32(0x80000017),

	 

	 
	IO_REPARSE_TAG_DFS	= cpu_to_le32(0x8000000A),

	 
	IO_REPARSE_TAG_FILTER_MANAGER	= cpu_to_le32(0x8000000B),

	 

	 
	IO_REPARSE_TAG_IFSTEST_CONGRUENT = cpu_to_le32(0x00000009),

	 
	IO_REPARSE_TAG_ARKIVIO	= cpu_to_le32(0x0000000C),

	 
	IO_REPARSE_TAG_SOLUTIONSOFT	= cpu_to_le32(0x2000000D),

	 
	IO_REPARSE_TAG_COMMVAULT	= cpu_to_le32(0x0000000E),

	 
	IO_REPARSE_TAG_CLOUD	= cpu_to_le32(0x9000001A),
	IO_REPARSE_TAG_CLOUD_1	= cpu_to_le32(0x9000101A),
	IO_REPARSE_TAG_CLOUD_2	= cpu_to_le32(0x9000201A),
	IO_REPARSE_TAG_CLOUD_3	= cpu_to_le32(0x9000301A),
	IO_REPARSE_TAG_CLOUD_4	= cpu_to_le32(0x9000401A),
	IO_REPARSE_TAG_CLOUD_5	= cpu_to_le32(0x9000501A),
	IO_REPARSE_TAG_CLOUD_6	= cpu_to_le32(0x9000601A),
	IO_REPARSE_TAG_CLOUD_7	= cpu_to_le32(0x9000701A),
	IO_REPARSE_TAG_CLOUD_8	= cpu_to_le32(0x9000801A),
	IO_REPARSE_TAG_CLOUD_9	= cpu_to_le32(0x9000901A),
	IO_REPARSE_TAG_CLOUD_A	= cpu_to_le32(0x9000A01A),
	IO_REPARSE_TAG_CLOUD_B	= cpu_to_le32(0x9000B01A),
	IO_REPARSE_TAG_CLOUD_C	= cpu_to_le32(0x9000C01A),
	IO_REPARSE_TAG_CLOUD_D	= cpu_to_le32(0x9000D01A),
	IO_REPARSE_TAG_CLOUD_E	= cpu_to_le32(0x9000E01A),
	IO_REPARSE_TAG_CLOUD_F	= cpu_to_le32(0x9000F01A),

};

#define SYMLINK_FLAG_RELATIVE		1

 
struct REPARSE_DATA_BUFFER {
	__le32 ReparseTag;		
	__le16 ReparseDataLength;	
	__le16 Reserved;

	union {
		 
		struct {
			__le16 SubstituteNameOffset; 
			__le16 SubstituteNameLength; 
			__le16 PrintNameOffset;      
			__le16 PrintNameLength;      
			__le16 PathBuffer[];	     
		} MountPointReparseBuffer;

		 
		struct {
			__le16 SubstituteNameOffset; 
			__le16 SubstituteNameLength; 
			__le16 PrintNameOffset;      
			__le16 PrintNameLength;      
			
			__le32 Flags;		     
			__le16 PathBuffer[];	     
		} SymbolicLinkReparseBuffer;

		 
		struct {
			__le32 WofVersion;  
			 
			__le32 WofProvider; 
			__le32 ProviderVer; 
			__le32 CompressionFormat; 
		} CompressReparseBuffer;

		struct {
			u8 DataBuffer[1];   
		} GenericReparseBuffer;
	};
};

 

#define FILE_NEED_EA 0x80 
 
struct EA_INFO {
	__le16 size_pack;	
	__le16 count;		
	__le32 size;		
};

static_assert(sizeof(struct EA_INFO) == 8);

 
struct EA_FULL {
	__le32 size;		
	u8 flags;		
	u8 name_len;		
	__le16 elength;		
	u8 name[];		
};

static_assert(offsetof(struct EA_FULL, name) == 8);

#define ACL_REVISION	2
#define ACL_REVISION_DS 4

#define SE_SELF_RELATIVE cpu_to_le16(0x8000)

struct SECURITY_DESCRIPTOR_RELATIVE {
	u8 Revision;
	u8 Sbz1;
	__le16 Control;
	__le32 Owner;
	__le32 Group;
	__le32 Sacl;
	__le32 Dacl;
};
static_assert(sizeof(struct SECURITY_DESCRIPTOR_RELATIVE) == 0x14);

struct ACE_HEADER {
	u8 AceType;
	u8 AceFlags;
	__le16 AceSize;
};
static_assert(sizeof(struct ACE_HEADER) == 4);

struct ACL {
	u8 AclRevision;
	u8 Sbz1;
	__le16 AclSize;
	__le16 AceCount;
	__le16 Sbz2;
};
static_assert(sizeof(struct ACL) == 8);

struct SID {
	u8 Revision;
	u8 SubAuthorityCount;
	u8 IdentifierAuthority[6];
	__le32 SubAuthority[];
};
static_assert(offsetof(struct SID, SubAuthority) == 8);

#endif  

