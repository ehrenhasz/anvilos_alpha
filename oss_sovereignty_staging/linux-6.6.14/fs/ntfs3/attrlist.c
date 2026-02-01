
 

#include <linux/fs.h>

#include "debug.h"
#include "ntfs.h"
#include "ntfs_fs.h"

 
static inline bool al_is_valid_le(const struct ntfs_inode *ni,
				  struct ATTR_LIST_ENTRY *le)
{
	if (!le || !ni->attr_list.le || !ni->attr_list.size)
		return false;

	return PtrOffset(ni->attr_list.le, le) + le16_to_cpu(le->size) <=
	       ni->attr_list.size;
}

void al_destroy(struct ntfs_inode *ni)
{
	run_close(&ni->attr_list.run);
	kfree(ni->attr_list.le);
	ni->attr_list.le = NULL;
	ni->attr_list.size = 0;
	ni->attr_list.dirty = false;
}

 
int ntfs_load_attr_list(struct ntfs_inode *ni, struct ATTRIB *attr)
{
	int err;
	size_t lsize;
	void *le = NULL;

	if (ni->attr_list.size)
		return 0;

	if (!attr->non_res) {
		lsize = le32_to_cpu(attr->res.data_size);
		 
		le = kvmalloc(al_aligned(lsize), GFP_KERNEL);
		if (!le) {
			err = -ENOMEM;
			goto out;
		}
		memcpy(le, resident_data(attr), lsize);
	} else if (attr->nres.svcn) {
		err = -EINVAL;
		goto out;
	} else {
		u16 run_off = le16_to_cpu(attr->nres.run_off);

		lsize = le64_to_cpu(attr->nres.data_size);

		run_init(&ni->attr_list.run);

		if (run_off > le32_to_cpu(attr->size)) {
			err = -EINVAL;
			goto out;
		}

		err = run_unpack_ex(&ni->attr_list.run, ni->mi.sbi, ni->mi.rno,
				    0, le64_to_cpu(attr->nres.evcn), 0,
				    Add2Ptr(attr, run_off),
				    le32_to_cpu(attr->size) - run_off);
		if (err < 0)
			goto out;

		 
		le = kvmalloc(al_aligned(lsize), GFP_KERNEL);
		if (!le) {
			err = -ENOMEM;
			goto out;
		}

		err = ntfs_read_run_nb(ni->mi.sbi, &ni->attr_list.run, 0, le,
				       lsize, NULL);
		if (err)
			goto out;
	}

	ni->attr_list.size = lsize;
	ni->attr_list.le = le;

	return 0;

out:
	ni->attr_list.le = le;
	al_destroy(ni);

	return err;
}

 
struct ATTR_LIST_ENTRY *al_enumerate(struct ntfs_inode *ni,
				     struct ATTR_LIST_ENTRY *le)
{
	size_t off;
	u16 sz;

	if (!le) {
		le = ni->attr_list.le;
	} else {
		sz = le16_to_cpu(le->size);
		if (sz < sizeof(struct ATTR_LIST_ENTRY)) {
			 
			return NULL;
		}
		le = Add2Ptr(le, sz);
	}

	 
	off = PtrOffset(ni->attr_list.le, le);
	if (off + sizeof(struct ATTR_LIST_ENTRY) > ni->attr_list.size) {
		 
		return NULL;
	}

	sz = le16_to_cpu(le->size);

	 
	if (sz < sizeof(struct ATTR_LIST_ENTRY) ||
	    off + sz > ni->attr_list.size ||
	    sz < le->name_off + le->name_len * sizeof(short)) {
		return NULL;
	}

	return le;
}

 
struct ATTR_LIST_ENTRY *al_find_le(struct ntfs_inode *ni,
				   struct ATTR_LIST_ENTRY *le,
				   const struct ATTRIB *attr)
{
	CLST svcn = attr_svcn(attr);

	return al_find_ex(ni, le, attr->type, attr_name(attr), attr->name_len,
			  &svcn);
}

 
struct ATTR_LIST_ENTRY *al_find_ex(struct ntfs_inode *ni,
				   struct ATTR_LIST_ENTRY *le,
				   enum ATTR_TYPE type, const __le16 *name,
				   u8 name_len, const CLST *vcn)
{
	struct ATTR_LIST_ENTRY *ret = NULL;
	u32 type_in = le32_to_cpu(type);

	while ((le = al_enumerate(ni, le))) {
		u64 le_vcn;
		int diff = le32_to_cpu(le->type) - type_in;

		 
		if (diff < 0)
			continue;

		if (diff > 0)
			return ret;

		if (le->name_len != name_len)
			continue;

		le_vcn = le64_to_cpu(le->vcn);
		if (!le_vcn) {
			 
			diff = ntfs_cmp_names(le_name(le), name_len, name,
					      name_len, ni->mi.sbi->upcase,
					      true);
			if (diff < 0)
				continue;

			if (diff > 0)
				return ret;
		}

		if (!vcn)
			return le;

		if (*vcn == le_vcn)
			return le;

		if (*vcn < le_vcn)
			return ret;

		ret = le;
	}

	return ret;
}

 
static struct ATTR_LIST_ENTRY *al_find_le_to_insert(struct ntfs_inode *ni,
						    enum ATTR_TYPE type,
						    const __le16 *name,
						    u8 name_len, CLST vcn)
{
	struct ATTR_LIST_ENTRY *le = NULL, *prev;
	u32 type_in = le32_to_cpu(type);

	 
	while ((le = al_enumerate(ni, prev = le))) {
		int diff = le32_to_cpu(le->type) - type_in;

		if (diff < 0)
			continue;

		if (diff > 0)
			return le;

		if (!le->vcn) {
			 
			diff = ntfs_cmp_names(le_name(le), le->name_len, name,
					      name_len, ni->mi.sbi->upcase,
					      true);
			if (diff < 0)
				continue;

			if (diff > 0)
				return le;
		}

		if (le64_to_cpu(le->vcn) >= vcn)
			return le;
	}

	return prev ? Add2Ptr(prev, le16_to_cpu(prev->size)) : ni->attr_list.le;
}

 
int al_add_le(struct ntfs_inode *ni, enum ATTR_TYPE type, const __le16 *name,
	      u8 name_len, CLST svcn, __le16 id, const struct MFT_REF *ref,
	      struct ATTR_LIST_ENTRY **new_le)
{
	int err;
	struct ATTRIB *attr;
	struct ATTR_LIST_ENTRY *le;
	size_t off;
	u16 sz;
	size_t asize, new_asize, old_size;
	u64 new_size;
	typeof(ni->attr_list) *al = &ni->attr_list;

	 
	sz = le_size(name_len);
	old_size = al->size;
	new_size = old_size + sz;
	asize = al_aligned(old_size);
	new_asize = al_aligned(new_size);

	 
	le = al_find_le_to_insert(ni, type, name, name_len, svcn);
	off = PtrOffset(al->le, le);

	if (new_size > asize) {
		void *ptr = kmalloc(new_asize, GFP_NOFS);

		if (!ptr)
			return -ENOMEM;

		memcpy(ptr, al->le, off);
		memcpy(Add2Ptr(ptr, off + sz), le, old_size - off);
		le = Add2Ptr(ptr, off);
		kfree(al->le);
		al->le = ptr;
	} else {
		memmove(Add2Ptr(le, sz), le, old_size - off);
	}
	*new_le = le;

	al->size = new_size;

	le->type = type;
	le->size = cpu_to_le16(sz);
	le->name_len = name_len;
	le->name_off = offsetof(struct ATTR_LIST_ENTRY, name);
	le->vcn = cpu_to_le64(svcn);
	le->ref = *ref;
	le->id = id;
	memcpy(le->name, name, sizeof(short) * name_len);

	err = attr_set_size(ni, ATTR_LIST, NULL, 0, &al->run, new_size,
			    &new_size, true, &attr);
	if (err) {
		 
		memmove(le, Add2Ptr(le, sz), old_size - off);
		al->size = old_size;
		return err;
	}

	al->dirty = true;

	if (attr && attr->non_res) {
		err = ntfs_sb_write_run(ni->mi.sbi, &al->run, 0, al->le,
					al->size, 0);
		if (err)
			return err;
		al->dirty = false;
	}

	return 0;
}

 
bool al_remove_le(struct ntfs_inode *ni, struct ATTR_LIST_ENTRY *le)
{
	u16 size;
	size_t off;
	typeof(ni->attr_list) *al = &ni->attr_list;

	if (!al_is_valid_le(ni, le))
		return false;

	 
	size = le16_to_cpu(le->size);
	off = PtrOffset(al->le, le);

	memmove(le, Add2Ptr(le, size), al->size - (off + size));

	al->size -= size;
	al->dirty = true;

	return true;
}

 
bool al_delete_le(struct ntfs_inode *ni, enum ATTR_TYPE type, CLST vcn,
		  const __le16 *name, u8 name_len, const struct MFT_REF *ref)
{
	u16 size;
	struct ATTR_LIST_ENTRY *le;
	size_t off;
	typeof(ni->attr_list) *al = &ni->attr_list;

	 
	le = al_find_ex(ni, NULL, type, name, name_len, &vcn);
	if (!le)
		return false;

	off = PtrOffset(al->le, le);

next:
	if (off >= al->size)
		return false;
	if (le->type != type)
		return false;
	if (le->name_len != name_len)
		return false;
	if (name_len && ntfs_cmp_names(le_name(le), name_len, name, name_len,
				       ni->mi.sbi->upcase, true))
		return false;
	if (le64_to_cpu(le->vcn) != vcn)
		return false;

	 
	if (ref && memcmp(ref, &le->ref, sizeof(*ref))) {
		off += le16_to_cpu(le->size);
		le = Add2Ptr(al->le, off);
		goto next;
	}

	 
	size = le16_to_cpu(le->size);
	 
	memmove(le, Add2Ptr(le, size), al->size - (off + size));

	al->size -= size;
	al->dirty = true;

	return true;
}

int al_update(struct ntfs_inode *ni, int sync)
{
	int err;
	struct ATTRIB *attr;
	typeof(ni->attr_list) *al = &ni->attr_list;

	if (!al->dirty || !al->size)
		return 0;

	 
	err = attr_set_size(ni, ATTR_LIST, NULL, 0, &al->run, al->size, NULL,
			    false, &attr);
	if (err)
		goto out;

	if (!attr->non_res) {
		memcpy(resident_data(attr), al->le, al->size);
	} else {
		err = ntfs_sb_write_run(ni->mi.sbi, &al->run, 0, al->le,
					al->size, sync);
		if (err)
			goto out;

		attr->nres.valid_size = attr->nres.data_size;
	}

	ni->mi.dirty = true;
	al->dirty = false;

out:
	return err;
}
