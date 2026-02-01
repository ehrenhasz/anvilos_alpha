
 

#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/blkdev.h>

#include "dir.h"
#include "aops.h"
#include "attrib.h"
#include "mft.h"
#include "debug.h"
#include "ntfs.h"

 
ntfschar I30[5] = { cpu_to_le16('$'), cpu_to_le16('I'),
		cpu_to_le16('3'),	cpu_to_le16('0'), 0 };

 
MFT_REF ntfs_lookup_inode_by_name(ntfs_inode *dir_ni, const ntfschar *uname,
		const int uname_len, ntfs_name **res)
{
	ntfs_volume *vol = dir_ni->vol;
	struct super_block *sb = vol->sb;
	MFT_RECORD *m;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie;
	INDEX_ALLOCATION *ia;
	u8 *index_end;
	u64 mref;
	ntfs_attr_search_ctx *ctx;
	int err, rc;
	VCN vcn, old_vcn;
	struct address_space *ia_mapping;
	struct page *page;
	u8 *kaddr;
	ntfs_name *name = NULL;

	BUG_ON(!S_ISDIR(VFS_I(dir_ni)->i_mode));
	BUG_ON(NInoAttr(dir_ni));
	 
	m = map_mft_record(dir_ni);
	if (IS_ERR(m)) {
		ntfs_error(sb, "map_mft_record() failed with error code %ld.",
				-PTR_ERR(m));
		return ERR_MREF(PTR_ERR(m));
	}
	ctx = ntfs_attr_get_search_ctx(dir_ni, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	 
	err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, CASE_SENSITIVE, 0, NULL,
			0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT) {
			ntfs_error(sb, "Index root attribute missing in "
					"directory inode 0x%lx.",
					dir_ni->mft_no);
			err = -EIO;
		}
		goto err_out;
	}
	 
	ir = (INDEX_ROOT*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset));
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	 
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	 
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		 
		if ((u8*)ie < (u8*)ctx->mrec || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end)
			goto dir_err_out;
		 
		if (ie->flags & INDEX_ENTRY_END)
			break;
		 
		if (ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len)) {
found_it:
			 
			if (ie->key.file_name.file_name_type == FILE_NAME_DOS) {
				if (!name) {
					name = kmalloc(sizeof(ntfs_name),
							GFP_NOFS);
					if (!name) {
						err = -ENOMEM;
						goto err_out;
					}
				}
				name->mref = le64_to_cpu(
						ie->data.dir.indexed_file);
				name->type = FILE_NAME_DOS;
				name->len = 0;
				*res = name;
			} else {
				kfree(name);
				*res = NULL;
			}
			mref = le64_to_cpu(ie->data.dir.indexed_file);
			ntfs_attr_put_search_ctx(ctx);
			unmap_mft_record(dir_ni);
			return mref;
		}
		 
		if (!NVolCaseSensitive(vol) &&
				ie->key.file_name.file_name_type &&
				ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				IGNORE_CASE, vol->upcase, vol->upcase_len)) {
			int name_size = sizeof(ntfs_name);
			u8 type = ie->key.file_name.file_name_type;
			u8 len = ie->key.file_name.file_name_length;

			 
			if (name) {
				ntfs_error(sb, "Found already allocated name "
						"in phase 1. Please run chkdsk "
						"and if that doesn't find any "
						"errors please report you saw "
						"this message to "
						"linux-ntfs-dev@lists."
						"sourceforge.net.");
				goto dir_err_out;
			}

			if (type != FILE_NAME_DOS)
				name_size += len * sizeof(ntfschar);
			name = kmalloc(name_size, GFP_NOFS);
			if (!name) {
				err = -ENOMEM;
				goto err_out;
			}
			name->mref = le64_to_cpu(ie->data.dir.indexed_file);
			name->type = type;
			if (type != FILE_NAME_DOS) {
				name->len = len;
				memcpy(name->name, ie->key.file_name.file_name,
						len * sizeof(ntfschar));
			} else
				name->len = 0;
			*res = name;
		}
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
		 
		if (rc == -1)
			break;
		 
		if (rc)
			continue;
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		 
		goto found_it;
	}
	 
	if (!(ie->flags & INDEX_ENTRY_NODE)) {
		if (name) {
			ntfs_attr_put_search_ctx(ctx);
			unmap_mft_record(dir_ni);
			return name->mref;
		}
		ntfs_debug("Entry not found.");
		err = -ENOENT;
		goto err_out;
	}  
	 
	if (!NInoIndexAllocPresent(dir_ni)) {
		ntfs_error(sb, "No index allocation attribute but index entry "
				"requires one. Directory inode 0x%lx is "
				"corrupt or driver bug.", dir_ni->mft_no);
		goto err_out;
	}
	 
	vcn = sle64_to_cpup((sle64*)((u8*)ie + le16_to_cpu(ie->length) - 8));
	ia_mapping = VFS_I(dir_ni)->i_mapping;
	 
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(dir_ni);
	m = NULL;
	ctx = NULL;
descend_into_child_node:
	 
	page = ntfs_map_page(ia_mapping, vcn <<
			dir_ni->itype.index.vcn_size_bits >> PAGE_SHIFT);
	if (IS_ERR(page)) {
		ntfs_error(sb, "Failed to map directory index page, error %ld.",
				-PTR_ERR(page));
		err = PTR_ERR(page);
		goto err_out;
	}
	lock_page(page);
	kaddr = (u8*)page_address(page);
fast_descend_into_child_node:
	 
	ia = (INDEX_ALLOCATION*)(kaddr + ((vcn <<
			dir_ni->itype.index.vcn_size_bits) & ~PAGE_MASK));
	 
	if ((u8*)ia < kaddr || (u8*)ia > kaddr + PAGE_SIZE) {
		ntfs_error(sb, "Out of bounds check failed. Corrupt directory "
				"inode 0x%lx or driver bug.", dir_ni->mft_no);
		goto unm_err_out;
	}
	 
	if (unlikely(!ntfs_is_indx_record(ia->magic))) {
		ntfs_error(sb, "Directory index record with vcn 0x%llx is "
				"corrupt.  Corrupt inode 0x%lx.  Run chkdsk.",
				(unsigned long long)vcn, dir_ni->mft_no);
		goto unm_err_out;
	}
	if (sle64_to_cpu(ia->index_block_vcn) != vcn) {
		ntfs_error(sb, "Actual VCN (0x%llx) of index buffer is "
				"different from expected VCN (0x%llx). "
				"Directory inode 0x%lx is corrupt or driver "
				"bug.", (unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)vcn, dir_ni->mft_no);
		goto unm_err_out;
	}
	if (le32_to_cpu(ia->index.allocated_size) + 0x18 !=
			dir_ni->itype.index.block_size) {
		ntfs_error(sb, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%lx has a size (%u) differing from the "
				"directory specified size (%u). Directory "
				"inode is corrupt or driver bug.",
				(unsigned long long)vcn, dir_ni->mft_no,
				le32_to_cpu(ia->index.allocated_size) + 0x18,
				dir_ni->itype.index.block_size);
		goto unm_err_out;
	}
	index_end = (u8*)ia + dir_ni->itype.index.block_size;
	if (index_end > kaddr + PAGE_SIZE) {
		ntfs_error(sb, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%lx crosses page boundary. Impossible! "
				"Cannot access! This is probably a bug in the "
				"driver.", (unsigned long long)vcn,
				dir_ni->mft_no);
		goto unm_err_out;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + dir_ni->itype.index.block_size) {
		ntfs_error(sb, "Size of index buffer (VCN 0x%llx) of directory "
				"inode 0x%lx exceeds maximum size.",
				(unsigned long long)vcn, dir_ni->mft_no);
		goto unm_err_out;
	}
	 
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	 
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		 
		if ((u8*)ie < (u8*)ia || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end) {
			ntfs_error(sb, "Index entry out of bounds in "
					"directory inode 0x%lx.",
					dir_ni->mft_no);
			goto unm_err_out;
		}
		 
		if (ie->flags & INDEX_ENTRY_END)
			break;
		 
		if (ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len)) {
found_it2:
			 
			if (ie->key.file_name.file_name_type == FILE_NAME_DOS) {
				if (!name) {
					name = kmalloc(sizeof(ntfs_name),
							GFP_NOFS);
					if (!name) {
						err = -ENOMEM;
						goto unm_err_out;
					}
				}
				name->mref = le64_to_cpu(
						ie->data.dir.indexed_file);
				name->type = FILE_NAME_DOS;
				name->len = 0;
				*res = name;
			} else {
				kfree(name);
				*res = NULL;
			}
			mref = le64_to_cpu(ie->data.dir.indexed_file);
			unlock_page(page);
			ntfs_unmap_page(page);
			return mref;
		}
		 
		if (!NVolCaseSensitive(vol) &&
				ie->key.file_name.file_name_type &&
				ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length,
				IGNORE_CASE, vol->upcase, vol->upcase_len)) {
			int name_size = sizeof(ntfs_name);
			u8 type = ie->key.file_name.file_name_type;
			u8 len = ie->key.file_name.file_name_length;

			 
			if (name) {
				ntfs_error(sb, "Found already allocated name "
						"in phase 2. Please run chkdsk "
						"and if that doesn't find any "
						"errors please report you saw "
						"this message to "
						"linux-ntfs-dev@lists."
						"sourceforge.net.");
				unlock_page(page);
				ntfs_unmap_page(page);
				goto dir_err_out;
			}

			if (type != FILE_NAME_DOS)
				name_size += len * sizeof(ntfschar);
			name = kmalloc(name_size, GFP_NOFS);
			if (!name) {
				err = -ENOMEM;
				goto unm_err_out;
			}
			name->mref = le64_to_cpu(ie->data.dir.indexed_file);
			name->type = type;
			if (type != FILE_NAME_DOS) {
				name->len = len;
				memcpy(name->name, ie->key.file_name.file_name,
						len * sizeof(ntfschar));
			} else
				name->len = 0;
			*res = name;
		}
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
		 
		if (rc == -1)
			break;
		 
		if (rc)
			continue;
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		 
		goto found_it2;
	}
	 
	if (ie->flags & INDEX_ENTRY_NODE) {
		if ((ia->index.flags & NODE_MASK) == LEAF_NODE) {
			ntfs_error(sb, "Index entry with child node found in "
					"a leaf node in directory inode 0x%lx.",
					dir_ni->mft_no);
			goto unm_err_out;
		}
		 
		old_vcn = vcn;
		vcn = sle64_to_cpup((sle64*)((u8*)ie +
				le16_to_cpu(ie->length) - 8));
		if (vcn >= 0) {
			 
			if (old_vcn << vol->cluster_size_bits >>
					PAGE_SHIFT == vcn <<
					vol->cluster_size_bits >>
					PAGE_SHIFT)
				goto fast_descend_into_child_node;
			unlock_page(page);
			ntfs_unmap_page(page);
			goto descend_into_child_node;
		}
		ntfs_error(sb, "Negative child node vcn in directory inode "
				"0x%lx.", dir_ni->mft_no);
		goto unm_err_out;
	}
	 
	if (name) {
		unlock_page(page);
		ntfs_unmap_page(page);
		return name->mref;
	}
	ntfs_debug("Entry not found.");
	err = -ENOENT;
unm_err_out:
	unlock_page(page);
	ntfs_unmap_page(page);
err_out:
	if (!err)
		err = -EIO;
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(dir_ni);
	if (name) {
		kfree(name);
		*res = NULL;
	}
	return ERR_MREF(err);
dir_err_out:
	ntfs_error(sb, "Corrupt directory.  Aborting lookup.");
	goto err_out;
}

#if 0

 
 
 
 

 
u64 ntfs_lookup_inode_by_name(ntfs_inode *dir_ni, const ntfschar *uname,
		const int uname_len)
{
	ntfs_volume *vol = dir_ni->vol;
	struct super_block *sb = vol->sb;
	MFT_RECORD *m;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie;
	INDEX_ALLOCATION *ia;
	u8 *index_end;
	u64 mref;
	ntfs_attr_search_ctx *ctx;
	int err, rc;
	IGNORE_CASE_BOOL ic;
	VCN vcn, old_vcn;
	struct address_space *ia_mapping;
	struct page *page;
	u8 *kaddr;

	 
	m = map_mft_record(dir_ni);
	if (IS_ERR(m)) {
		ntfs_error(sb, "map_mft_record() failed with error code %ld.",
				-PTR_ERR(m));
		return ERR_MREF(PTR_ERR(m));
	}
	ctx = ntfs_attr_get_search_ctx(dir_ni, m);
	if (!ctx) {
		err = -ENOMEM;
		goto err_out;
	}
	 
	err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, CASE_SENSITIVE, 0, NULL,
			0, ctx);
	if (unlikely(err)) {
		if (err == -ENOENT) {
			ntfs_error(sb, "Index root attribute missing in "
					"directory inode 0x%lx.",
					dir_ni->mft_no);
			err = -EIO;
		}
		goto err_out;
	}
	 
	ir = (INDEX_ROOT*)((u8*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset));
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	 
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	 
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		 
		if ((u8*)ie < (u8*)ctx->mrec || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end)
			goto dir_err_out;
		 
		if (ie->flags & INDEX_ENTRY_END)
			break;
		 
		ic = ie->key.file_name.file_name_type ? IGNORE_CASE :
				CASE_SENSITIVE;
		 
		if (ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, ic,
				vol->upcase, vol->upcase_len)) {
found_it:
			mref = le64_to_cpu(ie->data.dir.indexed_file);
			ntfs_attr_put_search_ctx(ctx);
			unmap_mft_record(dir_ni);
			return mref;
		}
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
		 
		if (rc == -1)
			break;
		 
		if (rc)
			continue;
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		 
		goto found_it;
	}
	 
	if (!(ie->flags & INDEX_ENTRY_NODE)) {
		 
		err = -ENOENT;
		goto err_out;
	}  
	 
	if (!NInoIndexAllocPresent(dir_ni)) {
		ntfs_error(sb, "No index allocation attribute but index entry "
				"requires one. Directory inode 0x%lx is "
				"corrupt or driver bug.", dir_ni->mft_no);
		goto err_out;
	}
	 
	vcn = sle64_to_cpup((u8*)ie + le16_to_cpu(ie->length) - 8);
	ia_mapping = VFS_I(dir_ni)->i_mapping;
	 
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(dir_ni);
	m = NULL;
	ctx = NULL;
descend_into_child_node:
	 
	page = ntfs_map_page(ia_mapping, vcn <<
			dir_ni->itype.index.vcn_size_bits >> PAGE_SHIFT);
	if (IS_ERR(page)) {
		ntfs_error(sb, "Failed to map directory index page, error %ld.",
				-PTR_ERR(page));
		err = PTR_ERR(page);
		goto err_out;
	}
	lock_page(page);
	kaddr = (u8*)page_address(page);
fast_descend_into_child_node:
	 
	ia = (INDEX_ALLOCATION*)(kaddr + ((vcn <<
			dir_ni->itype.index.vcn_size_bits) & ~PAGE_MASK));
	 
	if ((u8*)ia < kaddr || (u8*)ia > kaddr + PAGE_SIZE) {
		ntfs_error(sb, "Out of bounds check failed. Corrupt directory "
				"inode 0x%lx or driver bug.", dir_ni->mft_no);
		goto unm_err_out;
	}
	 
	if (unlikely(!ntfs_is_indx_record(ia->magic))) {
		ntfs_error(sb, "Directory index record with vcn 0x%llx is "
				"corrupt.  Corrupt inode 0x%lx.  Run chkdsk.",
				(unsigned long long)vcn, dir_ni->mft_no);
		goto unm_err_out;
	}
	if (sle64_to_cpu(ia->index_block_vcn) != vcn) {
		ntfs_error(sb, "Actual VCN (0x%llx) of index buffer is "
				"different from expected VCN (0x%llx). "
				"Directory inode 0x%lx is corrupt or driver "
				"bug.", (unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)vcn, dir_ni->mft_no);
		goto unm_err_out;
	}
	if (le32_to_cpu(ia->index.allocated_size) + 0x18 !=
			dir_ni->itype.index.block_size) {
		ntfs_error(sb, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%lx has a size (%u) differing from the "
				"directory specified size (%u). Directory "
				"inode is corrupt or driver bug.",
				(unsigned long long)vcn, dir_ni->mft_no,
				le32_to_cpu(ia->index.allocated_size) + 0x18,
				dir_ni->itype.index.block_size);
		goto unm_err_out;
	}
	index_end = (u8*)ia + dir_ni->itype.index.block_size;
	if (index_end > kaddr + PAGE_SIZE) {
		ntfs_error(sb, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%lx crosses page boundary. Impossible! "
				"Cannot access! This is probably a bug in the "
				"driver.", (unsigned long long)vcn,
				dir_ni->mft_no);
		goto unm_err_out;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + dir_ni->itype.index.block_size) {
		ntfs_error(sb, "Size of index buffer (VCN 0x%llx) of directory "
				"inode 0x%lx exceeds maximum size.",
				(unsigned long long)vcn, dir_ni->mft_no);
		goto unm_err_out;
	}
	 
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	 
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		 
		if ((u8*)ie < (u8*)ia || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end) {
			ntfs_error(sb, "Index entry out of bounds in "
					"directory inode 0x%lx.",
					dir_ni->mft_no);
			goto unm_err_out;
		}
		 
		if (ie->flags & INDEX_ENTRY_END)
			break;
		 
		ic = ie->key.file_name.file_name_type ? IGNORE_CASE :
				CASE_SENSITIVE;
		 
		if (ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, ic,
				vol->upcase, vol->upcase_len)) {
found_it2:
			mref = le64_to_cpu(ie->data.dir.indexed_file);
			unlock_page(page);
			ntfs_unmap_page(page);
			return mref;
		}
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
		 
		if (rc == -1)
			break;
		 
		if (rc)
			continue;
		 
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.file_name.file_name,
				ie->key.file_name.file_name_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		 
		goto found_it2;
	}
	 
	if (ie->flags & INDEX_ENTRY_NODE) {
		if ((ia->index.flags & NODE_MASK) == LEAF_NODE) {
			ntfs_error(sb, "Index entry with child node found in "
					"a leaf node in directory inode 0x%lx.",
					dir_ni->mft_no);
			goto unm_err_out;
		}
		 
		old_vcn = vcn;
		vcn = sle64_to_cpup((u8*)ie + le16_to_cpu(ie->length) - 8);
		if (vcn >= 0) {
			 
			if (old_vcn << vol->cluster_size_bits >>
					PAGE_SHIFT == vcn <<
					vol->cluster_size_bits >>
					PAGE_SHIFT)
				goto fast_descend_into_child_node;
			unlock_page(page);
			ntfs_unmap_page(page);
			goto descend_into_child_node;
		}
		ntfs_error(sb, "Negative child node vcn in directory inode "
				"0x%lx.", dir_ni->mft_no);
		goto unm_err_out;
	}
	 
	ntfs_debug("Entry not found.");
	err = -ENOENT;
unm_err_out:
	unlock_page(page);
	ntfs_unmap_page(page);
err_out:
	if (!err)
		err = -EIO;
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(dir_ni);
	return ERR_MREF(err);
dir_err_out:
	ntfs_error(sb, "Corrupt directory. Aborting lookup.");
	goto err_out;
}

#endif

 
static inline int ntfs_filldir(ntfs_volume *vol,
		ntfs_inode *ndir, struct page *ia_page, INDEX_ENTRY *ie,
		u8 *name, struct dir_context *actor)
{
	unsigned long mref;
	int name_len;
	unsigned dt_type;
	FILE_NAME_TYPE_FLAGS name_type;

	name_type = ie->key.file_name.file_name_type;
	if (name_type == FILE_NAME_DOS) {
		ntfs_debug("Skipping DOS name space entry.");
		return 0;
	}
	if (MREF_LE(ie->data.dir.indexed_file) == FILE_root) {
		ntfs_debug("Skipping root directory self reference entry.");
		return 0;
	}
	if (MREF_LE(ie->data.dir.indexed_file) < FILE_first_user &&
			!NVolShowSystemFiles(vol)) {
		ntfs_debug("Skipping system file.");
		return 0;
	}
	name_len = ntfs_ucstonls(vol, (ntfschar*)&ie->key.file_name.file_name,
			ie->key.file_name.file_name_length, &name,
			NTFS_MAX_NAME_LEN * NLS_MAX_CHARSET_SIZE + 1);
	if (name_len <= 0) {
		ntfs_warning(vol->sb, "Skipping unrepresentable inode 0x%llx.",
				(long long)MREF_LE(ie->data.dir.indexed_file));
		return 0;
	}
	if (ie->key.file_name.file_attributes &
			FILE_ATTR_DUP_FILE_NAME_INDEX_PRESENT)
		dt_type = DT_DIR;
	else
		dt_type = DT_REG;
	mref = MREF_LE(ie->data.dir.indexed_file);
	 
	if (ia_page)
		unlock_page(ia_page);
	ntfs_debug("Calling filldir for %s with len %i, fpos 0x%llx, inode "
			"0x%lx, DT_%s.", name, name_len, actor->pos, mref,
			dt_type == DT_DIR ? "DIR" : "REG");
	if (!dir_emit(actor, name, name_len, mref, dt_type))
		return 1;
	 
	if (ia_page)
		lock_page(ia_page);
	return 0;
}

 
static int ntfs_readdir(struct file *file, struct dir_context *actor)
{
	s64 ia_pos, ia_start, prev_ia_pos, bmp_pos;
	loff_t i_size;
	struct inode *bmp_vi, *vdir = file_inode(file);
	struct super_block *sb = vdir->i_sb;
	ntfs_inode *ndir = NTFS_I(vdir);
	ntfs_volume *vol = NTFS_SB(sb);
	MFT_RECORD *m;
	INDEX_ROOT *ir = NULL;
	INDEX_ENTRY *ie;
	INDEX_ALLOCATION *ia;
	u8 *name = NULL;
	int rc, err, ir_pos, cur_bmp_pos;
	struct address_space *ia_mapping, *bmp_mapping;
	struct page *bmp_page = NULL, *ia_page = NULL;
	u8 *kaddr, *bmp, *index_end;
	ntfs_attr_search_ctx *ctx;

	ntfs_debug("Entering for inode 0x%lx, fpos 0x%llx.",
			vdir->i_ino, actor->pos);
	rc = err = 0;
	 
	i_size = i_size_read(vdir);
	if (actor->pos >= i_size + vol->mft_record_size)
		return 0;
	 
	if (!dir_emit_dots(file, actor))
		return 0;
	m = NULL;
	ctx = NULL;
	 
	name = kmalloc(NTFS_MAX_NAME_LEN * NLS_MAX_CHARSET_SIZE + 1, GFP_NOFS);
	if (unlikely(!name)) {
		err = -ENOMEM;
		goto err_out;
	}
	 
	if (actor->pos >= vol->mft_record_size)
		goto skip_index_root;
	 
	m = map_mft_record(ndir);
	if (IS_ERR(m)) {
		err = PTR_ERR(m);
		m = NULL;
		goto err_out;
	}
	ctx = ntfs_attr_get_search_ctx(ndir, m);
	if (unlikely(!ctx)) {
		err = -ENOMEM;
		goto err_out;
	}
	 
	ir_pos = (s64)actor->pos;
	 
	err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, CASE_SENSITIVE, 0, NULL,
			0, ctx);
	if (unlikely(err)) {
		ntfs_error(sb, "Index root attribute missing in directory "
				"inode 0x%lx.", vdir->i_ino);
		goto err_out;
	}
	 
	rc = le32_to_cpu(ctx->attr->data.resident.value_length);
	ir = kmalloc(rc, GFP_NOFS);
	if (unlikely(!ir)) {
		err = -ENOMEM;
		goto err_out;
	}
	 
	memcpy(ir, (u8*)ctx->attr +
			le16_to_cpu(ctx->attr->data.resident.value_offset), rc);
	ntfs_attr_put_search_ctx(ctx);
	unmap_mft_record(ndir);
	ctx = NULL;
	m = NULL;
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	 
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	 
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		ntfs_debug("In index root, offset 0x%zx.", (u8*)ie - (u8*)ir);
		 
		if (unlikely((u8*)ie < (u8*)ir || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end))
			goto err_out;
		 
		if (ie->flags & INDEX_ENTRY_END)
			break;
		 
		if (ir_pos > (u8*)ie - (u8*)ir)
			continue;
		 
		actor->pos = (u8*)ie - (u8*)ir;
		 
		rc = ntfs_filldir(vol, ndir, NULL, ie, name, actor);
		if (rc) {
			kfree(ir);
			goto abort;
		}
	}
	 
	kfree(ir);
	ir = NULL;
	 
	if (!NInoIndexAllocPresent(ndir))
		goto EOD;
	 
	actor->pos = vol->mft_record_size;
skip_index_root:
	kaddr = NULL;
	prev_ia_pos = -1LL;
	 
	ia_pos = (s64)actor->pos - vol->mft_record_size;
	ia_mapping = vdir->i_mapping;
	ntfs_debug("Inode 0x%lx, getting index bitmap.", vdir->i_ino);
	bmp_vi = ntfs_attr_iget(vdir, AT_BITMAP, I30, 4);
	if (IS_ERR(bmp_vi)) {
		ntfs_error(sb, "Failed to get bitmap attribute.");
		err = PTR_ERR(bmp_vi);
		goto err_out;
	}
	bmp_mapping = bmp_vi->i_mapping;
	 
	bmp_pos = ia_pos >> ndir->itype.index.block_size_bits;
	if (unlikely(bmp_pos >> 3 >= i_size_read(bmp_vi))) {
		ntfs_error(sb, "Current index allocation position exceeds "
				"index bitmap size.");
		goto iput_err_out;
	}
	 
	cur_bmp_pos = bmp_pos & ((PAGE_SIZE * 8) - 1);
	bmp_pos &= ~(u64)((PAGE_SIZE * 8) - 1);
get_next_bmp_page:
	ntfs_debug("Reading bitmap with page index 0x%llx, bit ofs 0x%llx",
			(unsigned long long)bmp_pos >> (3 + PAGE_SHIFT),
			(unsigned long long)bmp_pos &
			(unsigned long long)((PAGE_SIZE * 8) - 1));
	bmp_page = ntfs_map_page(bmp_mapping,
			bmp_pos >> (3 + PAGE_SHIFT));
	if (IS_ERR(bmp_page)) {
		ntfs_error(sb, "Reading index bitmap failed.");
		err = PTR_ERR(bmp_page);
		bmp_page = NULL;
		goto iput_err_out;
	}
	bmp = (u8*)page_address(bmp_page);
	 
	while (!(bmp[cur_bmp_pos >> 3] & (1 << (cur_bmp_pos & 7)))) {
find_next_index_buffer:
		cur_bmp_pos++;
		 
		if (unlikely((cur_bmp_pos >> 3) >= PAGE_SIZE)) {
			ntfs_unmap_page(bmp_page);
			bmp_pos += PAGE_SIZE * 8;
			cur_bmp_pos = 0;
			goto get_next_bmp_page;
		}
		 
		if (unlikely(((bmp_pos + cur_bmp_pos) >> 3) >= i_size))
			goto unm_EOD;
		ia_pos = (bmp_pos + cur_bmp_pos) <<
				ndir->itype.index.block_size_bits;
	}
	ntfs_debug("Handling index buffer 0x%llx.",
			(unsigned long long)bmp_pos + cur_bmp_pos);
	 
	if ((prev_ia_pos & (s64)PAGE_MASK) !=
			(ia_pos & (s64)PAGE_MASK)) {
		prev_ia_pos = ia_pos;
		if (likely(ia_page != NULL)) {
			unlock_page(ia_page);
			ntfs_unmap_page(ia_page);
		}
		 
		ia_page = ntfs_map_page(ia_mapping, ia_pos >> PAGE_SHIFT);
		if (IS_ERR(ia_page)) {
			ntfs_error(sb, "Reading index allocation data failed.");
			err = PTR_ERR(ia_page);
			ia_page = NULL;
			goto err_out;
		}
		lock_page(ia_page);
		kaddr = (u8*)page_address(ia_page);
	}
	 
	ia = (INDEX_ALLOCATION*)(kaddr + (ia_pos & ~PAGE_MASK &
					  ~(s64)(ndir->itype.index.block_size - 1)));
	 
	if (unlikely((u8*)ia < kaddr || (u8*)ia > kaddr + PAGE_SIZE)) {
		ntfs_error(sb, "Out of bounds check failed. Corrupt directory "
				"inode 0x%lx or driver bug.", vdir->i_ino);
		goto err_out;
	}
	 
	if (unlikely(!ntfs_is_indx_record(ia->magic))) {
		ntfs_error(sb, "Directory index record with vcn 0x%llx is "
				"corrupt.  Corrupt inode 0x%lx.  Run chkdsk.",
				(unsigned long long)ia_pos >>
				ndir->itype.index.vcn_size_bits, vdir->i_ino);
		goto err_out;
	}
	if (unlikely(sle64_to_cpu(ia->index_block_vcn) != (ia_pos &
			~(s64)(ndir->itype.index.block_size - 1)) >>
			ndir->itype.index.vcn_size_bits)) {
		ntfs_error(sb, "Actual VCN (0x%llx) of index buffer is "
				"different from expected VCN (0x%llx). "
				"Directory inode 0x%lx is corrupt or driver "
				"bug. ", (unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)ia_pos >>
				ndir->itype.index.vcn_size_bits, vdir->i_ino);
		goto err_out;
	}
	if (unlikely(le32_to_cpu(ia->index.allocated_size) + 0x18 !=
			ndir->itype.index.block_size)) {
		ntfs_error(sb, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%lx has a size (%u) differing from the "
				"directory specified size (%u). Directory "
				"inode is corrupt or driver bug.",
				(unsigned long long)ia_pos >>
				ndir->itype.index.vcn_size_bits, vdir->i_ino,
				le32_to_cpu(ia->index.allocated_size) + 0x18,
				ndir->itype.index.block_size);
		goto err_out;
	}
	index_end = (u8*)ia + ndir->itype.index.block_size;
	if (unlikely(index_end > kaddr + PAGE_SIZE)) {
		ntfs_error(sb, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%lx crosses page boundary. Impossible! "
				"Cannot access! This is probably a bug in the "
				"driver.", (unsigned long long)ia_pos >>
				ndir->itype.index.vcn_size_bits, vdir->i_ino);
		goto err_out;
	}
	ia_start = ia_pos & ~(s64)(ndir->itype.index.block_size - 1);
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (unlikely(index_end > (u8*)ia + ndir->itype.index.block_size)) {
		ntfs_error(sb, "Size of index buffer (VCN 0x%llx) of directory "
				"inode 0x%lx exceeds maximum size.",
				(unsigned long long)ia_pos >>
				ndir->itype.index.vcn_size_bits, vdir->i_ino);
		goto err_out;
	}
	 
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	 
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		ntfs_debug("In index allocation, offset 0x%llx.",
				(unsigned long long)ia_start +
				(unsigned long long)((u8*)ie - (u8*)ia));
		 
		if (unlikely((u8*)ie < (u8*)ia || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end))
			goto err_out;
		 
		if (ie->flags & INDEX_ENTRY_END)
			break;
		 
		if (ia_pos - ia_start > (u8*)ie - (u8*)ia)
			continue;
		 
		actor->pos = (u8*)ie - (u8*)ia +
				(sle64_to_cpu(ia->index_block_vcn) <<
				ndir->itype.index.vcn_size_bits) +
				vol->mft_record_size;
		 
		rc = ntfs_filldir(vol, ndir, ia_page, ie, name, actor);
		if (rc) {
			 
			ntfs_unmap_page(ia_page);
			ntfs_unmap_page(bmp_page);
			iput(bmp_vi);
			goto abort;
		}
	}
	goto find_next_index_buffer;
unm_EOD:
	if (ia_page) {
		unlock_page(ia_page);
		ntfs_unmap_page(ia_page);
	}
	ntfs_unmap_page(bmp_page);
	iput(bmp_vi);
EOD:
	 
	actor->pos = i_size + vol->mft_record_size;
abort:
	kfree(name);
	return 0;
err_out:
	if (bmp_page) {
		ntfs_unmap_page(bmp_page);
iput_err_out:
		iput(bmp_vi);
	}
	if (ia_page) {
		unlock_page(ia_page);
		ntfs_unmap_page(ia_page);
	}
	kfree(ir);
	kfree(name);
	if (ctx)
		ntfs_attr_put_search_ctx(ctx);
	if (m)
		unmap_mft_record(ndir);
	if (!err)
		err = -EIO;
	ntfs_debug("Failed. Returning error code %i.", -err);
	return err;
}

 
static int ntfs_dir_open(struct inode *vi, struct file *filp)
{
	if (sizeof(unsigned long) < 8) {
		if (i_size_read(vi) > MAX_LFS_FILESIZE)
			return -EFBIG;
	}
	return 0;
}

#ifdef NTFS_RW

 
static int ntfs_dir_fsync(struct file *filp, loff_t start, loff_t end,
			  int datasync)
{
	struct inode *bmp_vi, *vi = filp->f_mapping->host;
	int err, ret;
	ntfs_attr na;

	ntfs_debug("Entering for inode 0x%lx.", vi->i_ino);

	err = file_write_and_wait_range(filp, start, end);
	if (err)
		return err;
	inode_lock(vi);

	BUG_ON(!S_ISDIR(vi->i_mode));
	 
	na.mft_no = vi->i_ino;
	na.type = AT_BITMAP;
	na.name = I30;
	na.name_len = 4;
	bmp_vi = ilookup5(vi->i_sb, vi->i_ino, ntfs_test_inode, &na);
	if (bmp_vi) {
 		write_inode_now(bmp_vi, !datasync);
		iput(bmp_vi);
	}
	ret = __ntfs_write_inode(vi, 1);
	write_inode_now(vi, !datasync);
	err = sync_blockdev(vi->i_sb->s_bdev);
	if (unlikely(err && !ret))
		ret = err;
	if (likely(!ret))
		ntfs_debug("Done.");
	else
		ntfs_warning(vi->i_sb, "Failed to f%ssync inode 0x%lx.  Error "
				"%u.", datasync ? "data" : "", vi->i_ino, -ret);
	inode_unlock(vi);
	return ret;
}

#endif  

WRAP_DIR_ITER(ntfs_readdir) 
const struct file_operations ntfs_dir_ops = {
	.llseek		= generic_file_llseek,	 
	.read		= generic_read_dir,	 
	.iterate_shared	= shared_ntfs_readdir,	 
#ifdef NTFS_RW
	.fsync		= ntfs_dir_fsync,	 
#endif  
	 			 
	.open		= ntfs_dir_open,	 
};
