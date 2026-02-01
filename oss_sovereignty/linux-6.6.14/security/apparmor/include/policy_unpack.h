 
 

#ifndef __POLICY_INTERFACE_H
#define __POLICY_INTERFACE_H

#include <linux/list.h>
#include <linux/kref.h>
#include <linux/dcache.h>
#include <linux/workqueue.h>


struct aa_load_ent {
	struct list_head list;
	struct aa_profile *new;
	struct aa_profile *old;
	struct aa_profile *rename;
	const char *ns_name;
};

void aa_load_ent_free(struct aa_load_ent *ent);
struct aa_load_ent *aa_load_ent_alloc(void);

#define PACKED_FLAG_HAT		1
#define PACKED_FLAG_DEBUG1	2
#define PACKED_FLAG_DEBUG2	4

#define PACKED_MODE_ENFORCE	0
#define PACKED_MODE_COMPLAIN	1
#define PACKED_MODE_KILL	2
#define PACKED_MODE_UNCONFINED	3
#define PACKED_MODE_USER	4

struct aa_ns;

enum {
	AAFS_LOADDATA_ABI = 0,
	AAFS_LOADDATA_REVISION,
	AAFS_LOADDATA_HASH,
	AAFS_LOADDATA_DATA,
	AAFS_LOADDATA_COMPRESSED_SIZE,
	AAFS_LOADDATA_DIR,		 
	AAFS_LOADDATA_NDENTS		 
};

 

enum aa_code {
	AA_U8,
	AA_U16,
	AA_U32,
	AA_U64,
	AA_NAME,		 
	AA_STRING,
	AA_BLOB,
	AA_STRUCT,
	AA_STRUCTEND,
	AA_LIST,
	AA_LISTEND,
	AA_ARRAY,
	AA_ARRAYEND,
};

 
struct aa_ext {
	void *start;
	void *end;
	void *pos;		 
	u32 version;
};

 
struct aa_loaddata {
	struct kref count;
	struct list_head list;
	struct work_struct work;
	struct dentry *dents[AAFS_LOADDATA_NDENTS];
	struct aa_ns *ns;
	char *name;
	size_t size;			 
	size_t compressed_size;		 
	long revision;			 
	int abi;
	unsigned char *hash;

	 
	char *data;
};

int aa_unpack(struct aa_loaddata *udata, struct list_head *lh, const char **ns);

 
static inline struct aa_loaddata *
__aa_get_loaddata(struct aa_loaddata *data)
{
	if (data && kref_get_unless_zero(&(data->count)))
		return data;

	return NULL;
}

 
static inline struct aa_loaddata *
aa_get_loaddata(struct aa_loaddata *data)
{
	struct aa_loaddata *tmp = __aa_get_loaddata(data);

	AA_BUG(data && !tmp);

	return tmp;
}

void __aa_loaddata_update(struct aa_loaddata *data, long revision);
bool aa_rawdata_eq(struct aa_loaddata *l, struct aa_loaddata *r);
void aa_loaddata_kref(struct kref *kref);
struct aa_loaddata *aa_loaddata_alloc(size_t size);
static inline void aa_put_loaddata(struct aa_loaddata *data)
{
	if (data)
		kref_put(&data->count, aa_loaddata_kref);
}

#if IS_ENABLED(CONFIG_KUNIT)
bool aa_inbounds(struct aa_ext *e, size_t size);
size_t aa_unpack_u16_chunk(struct aa_ext *e, char **chunk);
bool aa_unpack_X(struct aa_ext *e, enum aa_code code);
bool aa_unpack_nameX(struct aa_ext *e, enum aa_code code, const char *name);
bool aa_unpack_u32(struct aa_ext *e, u32 *data, const char *name);
bool aa_unpack_u64(struct aa_ext *e, u64 *data, const char *name);
bool aa_unpack_array(struct aa_ext *e, const char *name, u16 *size);
size_t aa_unpack_blob(struct aa_ext *e, char **blob, const char *name);
int aa_unpack_str(struct aa_ext *e, const char **string, const char *name);
int aa_unpack_strdup(struct aa_ext *e, char **string, const char *name);
#endif

#endif  
