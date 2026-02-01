
 
#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/iomap.h>
#include <linux/swap.h>

 

struct iomap_swapfile_info {
	struct iomap iomap;		 
	struct swap_info_struct *sis;
	uint64_t lowest_ppage;		 
	uint64_t highest_ppage;		 
	unsigned long nr_pages;		 
	int nr_extents;			 
	struct file *file;
};

 
static int iomap_swapfile_add_extent(struct iomap_swapfile_info *isi)
{
	struct iomap *iomap = &isi->iomap;
	unsigned long nr_pages;
	unsigned long max_pages;
	uint64_t first_ppage;
	uint64_t first_ppage_reported;
	uint64_t next_ppage;
	int error;

	if (unlikely(isi->nr_pages >= isi->sis->max))
		return 0;
	max_pages = isi->sis->max - isi->nr_pages;

	 
	first_ppage = ALIGN(iomap->addr, PAGE_SIZE) >> PAGE_SHIFT;
	next_ppage = ALIGN_DOWN(iomap->addr + iomap->length, PAGE_SIZE) >>
			PAGE_SHIFT;

	 
	if (first_ppage >= next_ppage)
		return 0;
	nr_pages = next_ppage - first_ppage;
	nr_pages = min(nr_pages, max_pages);

	 
	first_ppage_reported = first_ppage;
	if (iomap->offset == 0)
		first_ppage_reported++;
	if (isi->lowest_ppage > first_ppage_reported)
		isi->lowest_ppage = first_ppage_reported;
	if (isi->highest_ppage < (next_ppage - 1))
		isi->highest_ppage = next_ppage - 1;

	 
	error = add_swap_extent(isi->sis, isi->nr_pages, nr_pages, first_ppage);
	if (error < 0)
		return error;
	isi->nr_extents += error;
	isi->nr_pages += nr_pages;
	return 0;
}

static int iomap_swapfile_fail(struct iomap_swapfile_info *isi, const char *str)
{
	char *buf, *p = ERR_PTR(-ENOMEM);

	buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (buf)
		p = file_path(isi->file, buf, PATH_MAX);
	pr_err("swapon: file %s %s\n", IS_ERR(p) ? "<unknown>" : p, str);
	kfree(buf);
	return -EINVAL;
}

 
static loff_t iomap_swapfile_iter(const struct iomap_iter *iter,
		struct iomap *iomap, struct iomap_swapfile_info *isi)
{
	switch (iomap->type) {
	case IOMAP_MAPPED:
	case IOMAP_UNWRITTEN:
		 
		break;
	case IOMAP_INLINE:
		 
		return iomap_swapfile_fail(isi, "is inline");
	default:
		return iomap_swapfile_fail(isi, "has unallocated extents");
	}

	 
	if (iomap->flags & IOMAP_F_DIRTY)
		return iomap_swapfile_fail(isi, "is not committed");
	if (iomap->flags & IOMAP_F_SHARED)
		return iomap_swapfile_fail(isi, "has shared extents");

	 
	if (iomap->bdev != isi->sis->bdev)
		return iomap_swapfile_fail(isi, "outside the main device");

	if (isi->iomap.length == 0) {
		 
		memcpy(&isi->iomap, iomap, sizeof(isi->iomap));
	} else if (isi->iomap.addr + isi->iomap.length == iomap->addr) {
		 
		isi->iomap.length += iomap->length;
	} else {
		 
		int error = iomap_swapfile_add_extent(isi);
		if (error)
			return error;
		memcpy(&isi->iomap, iomap, sizeof(isi->iomap));
	}
	return iomap_length(iter);
}

 
int iomap_swapfile_activate(struct swap_info_struct *sis,
		struct file *swap_file, sector_t *pagespan,
		const struct iomap_ops *ops)
{
	struct inode *inode = swap_file->f_mapping->host;
	struct iomap_iter iter = {
		.inode	= inode,
		.pos	= 0,
		.len	= ALIGN_DOWN(i_size_read(inode), PAGE_SIZE),
		.flags	= IOMAP_REPORT,
	};
	struct iomap_swapfile_info isi = {
		.sis = sis,
		.lowest_ppage = (sector_t)-1ULL,
		.file = swap_file,
	};
	int ret;

	 
	ret = vfs_fsync(swap_file, 1);
	if (ret)
		return ret;

	while ((ret = iomap_iter(&iter, ops)) > 0)
		iter.processed = iomap_swapfile_iter(&iter, &iter.iomap, &isi);
	if (ret < 0)
		return ret;

	if (isi.iomap.length) {
		ret = iomap_swapfile_add_extent(&isi);
		if (ret)
			return ret;
	}

	 
	if (isi.nr_pages == 0) {
		pr_warn("swapon: Cannot find a single usable page in file.\n");
		return -EINVAL;
	}

	*pagespan = 1 + isi.highest_ppage - isi.lowest_ppage;
	sis->max = isi.nr_pages;
	sis->pages = isi.nr_pages - 1;
	sis->highest_bit = isi.nr_pages - 1;
	return isi.nr_extents;
}
EXPORT_SYMBOL_GPL(iomap_swapfile_activate);
