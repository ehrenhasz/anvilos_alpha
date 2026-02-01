
 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/pagemap.h>
#include <linux/msdos_partition.h>
#include <asm/unaligned.h>

#include <scsi/scsicam.h>

 
unsigned char *scsi_bios_ptable(struct block_device *dev)
{
	struct address_space *mapping = bdev_whole(dev)->bd_inode->i_mapping;
	unsigned char *res = NULL;
	struct folio *folio;

	folio = read_mapping_folio(mapping, 0, NULL);
	if (IS_ERR(folio))
		return NULL;

	res = kmemdup(folio_address(folio) + 0x1be, 66, GFP_KERNEL);
	folio_put(folio);
	return res;
}
EXPORT_SYMBOL(scsi_bios_ptable);

 
bool scsi_partsize(struct block_device *bdev, sector_t capacity, int geom[3])
{
	int cyl, ext_cyl, end_head, end_cyl, end_sector;
	unsigned int logical_end, physical_end, ext_physical_end;
	struct msdos_partition *p, *largest = NULL;
	void *buf;
	int ret = false;

	buf = scsi_bios_ptable(bdev);
	if (!buf)
		return false;

	if (*(unsigned short *) (buf + 64) == 0xAA55) {
		int largest_cyl = -1, i;

		for (i = 0, p = buf; i < 4; i++, p++) {
			if (!p->sys_ind)
				continue;
#ifdef DEBUG
			printk("scsicam_bios_param : partition %d has system \n",
			       i);
#endif
			cyl = p->cyl + ((p->sector & 0xc0) << 2);
			if (cyl > largest_cyl) {
				largest_cyl = cyl;
				largest = p;
			}
		}
	}
	if (largest) {
		end_cyl = largest->end_cyl + ((largest->end_sector & 0xc0) << 2);
		end_head = largest->end_head;
		end_sector = largest->end_sector & 0x3f;

		if (end_head + 1 == 0 || end_sector == 0)
			goto out_free_buf;

#ifdef DEBUG
		printk("scsicam_bios_param : end at h = %d, c = %d, s = %d\n",
		       end_head, end_cyl, end_sector);
#endif

		physical_end = end_cyl * (end_head + 1) * end_sector +
		    end_head * end_sector + end_sector;

		 
		logical_end = get_unaligned_le32(&largest->start_sect)
		    + get_unaligned_le32(&largest->nr_sects);

		 
		ext_cyl = (logical_end - (end_head * end_sector + end_sector))
		    / (end_head + 1) / end_sector;
		ext_physical_end = ext_cyl * (end_head + 1) * end_sector +
		    end_head * end_sector + end_sector;

#ifdef DEBUG
		printk("scsicam_bios_param : logical_end=%d physical_end=%d ext_physical_end=%d ext_cyl=%d\n"
		  ,logical_end, physical_end, ext_physical_end, ext_cyl);
#endif

		if (logical_end == physical_end ||
		    (end_cyl == 1023 && ext_physical_end == logical_end)) {
			geom[0] = end_head + 1;
			geom[1] = end_sector;
			geom[2] = (unsigned long)capacity /
				((end_head + 1) * end_sector);
			ret = true;
			goto out_free_buf;
		}
#ifdef DEBUG
		printk("scsicam_bios_param : logical (%u) != physical (%u)\n",
		       logical_end, physical_end);
#endif
	}

out_free_buf:
	kfree(buf);
	return ret;
}
EXPORT_SYMBOL(scsi_partsize);

 

static int setsize(unsigned long capacity, unsigned int *cyls, unsigned int *hds,
		   unsigned int *secs)
{
	unsigned int rv = 0;
	unsigned long heads, sectors, cylinders, temp;

	cylinders = 1024L;	 
	sectors = 62L;		 

	temp = cylinders * sectors;	 
	heads = capacity / temp;	 
	if (capacity % temp) {	 
		heads++;	 
		temp = cylinders * heads;	 
		sectors = capacity / temp;	 
		if (capacity % temp) {	 
			sectors++;	 
			temp = heads * sectors;		 
			cylinders = capacity / temp;	 
		}
	}
	if (cylinders == 0)
		rv = (unsigned) -1;	 

	*cyls = (unsigned int) cylinders;	 
	*secs = (unsigned int) sectors;
	*hds = (unsigned int) heads;
	return (rv);
}

 
int scsicam_bios_param(struct block_device *bdev, sector_t capacity, int *ip)
{
	u64 capacity64 = capacity;	 
	int ret = 0;

	 
	if (scsi_partsize(bdev, capacity, ip))
		return 0;

	if (capacity64 < (1ULL << 32)) {
		 
		ret = setsize((unsigned long)capacity, (unsigned int *)ip + 2,
		       (unsigned int *)ip + 0, (unsigned int *)ip + 1);
	}

	 
	if (ret || ip[0] > 255 || ip[1] > 63) {
		if ((capacity >> 11) > 65534) {
			ip[0] = 255;
			ip[1] = 63;
		} else {
			ip[0] = 64;
			ip[1] = 32;
		}

		if (capacity > 65535*63*255)
			ip[2] = 65535;
		else
			ip[2] = (unsigned long)capacity / (ip[0] * ip[1]);
	}

	return 0;
}
EXPORT_SYMBOL(scsicam_bios_param);
