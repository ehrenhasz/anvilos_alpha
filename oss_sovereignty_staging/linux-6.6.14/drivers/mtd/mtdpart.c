
 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/kmod.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "mtdcore.h"

 

static inline void free_partition(struct mtd_info *mtd)
{
	kfree(mtd->name);
	kfree(mtd);
}

void release_mtd_partition(struct mtd_info *mtd)
{
	WARN_ON(!list_empty(&mtd->part.node));
	free_partition(mtd);
}

static struct mtd_info *allocate_partition(struct mtd_info *parent,
					   const struct mtd_partition *part,
					   int partno, uint64_t cur_offset)
{
	struct mtd_info *master = mtd_get_master(parent);
	int wr_alignment = (parent->flags & MTD_NO_ERASE) ?
			   master->writesize : master->erasesize;
	u64 parent_size = mtd_is_partition(parent) ?
			  parent->part.size : parent->size;
	struct mtd_info *child;
	u32 remainder;
	char *name;
	u64 tmp;

	 
	child = kzalloc(sizeof(*child), GFP_KERNEL);
	name = kstrdup(part->name, GFP_KERNEL);
	if (!name || !child) {
		printk(KERN_ERR"memory allocation error while creating partitions for \"%s\"\n",
		       parent->name);
		kfree(name);
		kfree(child);
		return ERR_PTR(-ENOMEM);
	}

	 
	child->type = parent->type;
	child->part.flags = parent->flags & ~part->mask_flags;
	child->part.flags |= part->add_flags;
	child->flags = child->part.flags;
	child->part.size = part->size;
	child->writesize = parent->writesize;
	child->writebufsize = parent->writebufsize;
	child->oobsize = parent->oobsize;
	child->oobavail = parent->oobavail;
	child->subpage_sft = parent->subpage_sft;

	child->name = name;
	child->owner = parent->owner;

	 
	child->dev.parent = IS_ENABLED(CONFIG_MTD_PARTITIONED_MASTER) || mtd_is_partition(parent) ?
			    &parent->dev : parent->dev.parent;
	child->dev.of_node = part->of_node;
	child->parent = parent;
	child->part.offset = part->offset;
	INIT_LIST_HEAD(&child->partitions);

	if (child->part.offset == MTDPART_OFS_APPEND)
		child->part.offset = cur_offset;
	if (child->part.offset == MTDPART_OFS_NXTBLK) {
		tmp = cur_offset;
		child->part.offset = cur_offset;
		remainder = do_div(tmp, wr_alignment);
		if (remainder) {
			child->part.offset += wr_alignment - remainder;
			printk(KERN_NOTICE "Moving partition %d: "
			       "0x%012llx -> 0x%012llx\n", partno,
			       (unsigned long long)cur_offset,
			       child->part.offset);
		}
	}
	if (child->part.offset == MTDPART_OFS_RETAIN) {
		child->part.offset = cur_offset;
		if (parent_size - child->part.offset >= child->part.size) {
			child->part.size = parent_size - child->part.offset -
					   child->part.size;
		} else {
			printk(KERN_ERR "mtd partition \"%s\" doesn't have enough space: %#llx < %#llx, disabled\n",
				part->name, parent_size - child->part.offset,
				child->part.size);
			 
			goto out_register;
		}
	}
	if (child->part.size == MTDPART_SIZ_FULL)
		child->part.size = parent_size - child->part.offset;

	printk(KERN_NOTICE "0x%012llx-0x%012llx : \"%s\"\n",
	       child->part.offset, child->part.offset + child->part.size,
	       child->name);

	 
	if (child->part.offset >= parent_size) {
		 
		child->part.offset = 0;
		child->part.size = 0;

		 
		child->erasesize = parent->erasesize;
		printk(KERN_ERR"mtd: partition \"%s\" is out of reach -- disabled\n",
			part->name);
		goto out_register;
	}
	if (child->part.offset + child->part.size > parent->size) {
		child->part.size = parent_size - child->part.offset;
		printk(KERN_WARNING"mtd: partition \"%s\" extends beyond the end of device \"%s\" -- size truncated to %#llx\n",
			part->name, parent->name, child->part.size);
	}

	if (parent->numeraseregions > 1) {
		 
		int i, max = parent->numeraseregions;
		u64 end = child->part.offset + child->part.size;
		struct mtd_erase_region_info *regions = parent->eraseregions;

		 
		for (i = 0; i < max && regions[i].offset <= child->part.offset;
		     i++)
			;
		 
		if (i > 0)
			i--;

		 
		for (; i < max && regions[i].offset < end; i++) {
			if (child->erasesize < regions[i].erasesize)
				child->erasesize = regions[i].erasesize;
		}
		BUG_ON(child->erasesize == 0);
	} else {
		 
		child->erasesize = master->erasesize;
	}

	 
	if (!(child->flags & MTD_NO_ERASE))
		wr_alignment = child->erasesize;

	tmp = mtd_get_master_ofs(child, 0);
	remainder = do_div(tmp, wr_alignment);
	if ((child->flags & MTD_WRITEABLE) && remainder) {
		 
		 
		child->flags &= ~MTD_WRITEABLE;
		printk(KERN_WARNING"mtd: partition \"%s\" doesn't start on an erase/write block boundary -- force read-only\n",
			part->name);
	}

	tmp = mtd_get_master_ofs(child, 0) + child->part.size;
	remainder = do_div(tmp, wr_alignment);
	if ((child->flags & MTD_WRITEABLE) && remainder) {
		child->flags &= ~MTD_WRITEABLE;
		printk(KERN_WARNING"mtd: partition \"%s\" doesn't end on an erase/write block -- force read-only\n",
			part->name);
	}

	child->size = child->part.size;
	child->ecc_step_size = parent->ecc_step_size;
	child->ecc_strength = parent->ecc_strength;
	child->bitflip_threshold = parent->bitflip_threshold;

	if (master->_block_isbad) {
		uint64_t offs = 0;

		while (offs < child->part.size) {
			if (mtd_block_isreserved(child, offs))
				child->ecc_stats.bbtblocks++;
			else if (mtd_block_isbad(child, offs))
				child->ecc_stats.badblocks++;
			offs += child->erasesize;
		}
	}

out_register:
	return child;
}

static ssize_t offset_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct mtd_info *mtd = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%lld\n", mtd->part.offset);
}
static DEVICE_ATTR_RO(offset);	 

static const struct attribute *mtd_partition_attrs[] = {
	&dev_attr_offset.attr,
	NULL
};

static int mtd_add_partition_attrs(struct mtd_info *new)
{
	int ret = sysfs_create_files(&new->dev.kobj, mtd_partition_attrs);
	if (ret)
		printk(KERN_WARNING
		       "mtd: failed to create partition attrs, err=%d\n", ret);
	return ret;
}

int mtd_add_partition(struct mtd_info *parent, const char *name,
		      long long offset, long long length)
{
	struct mtd_info *master = mtd_get_master(parent);
	u64 parent_size = mtd_is_partition(parent) ?
			  parent->part.size : parent->size;
	struct mtd_partition part;
	struct mtd_info *child;
	int ret = 0;

	 
	if (offset == MTDPART_OFS_APPEND ||
	    offset == MTDPART_OFS_NXTBLK)
		return -EINVAL;

	if (length == MTDPART_SIZ_FULL)
		length = parent_size - offset;

	if (length <= 0)
		return -EINVAL;

	memset(&part, 0, sizeof(part));
	part.name = name;
	part.size = length;
	part.offset = offset;

	child = allocate_partition(parent, &part, -1, offset);
	if (IS_ERR(child))
		return PTR_ERR(child);

	mutex_lock(&master->master.partitions_lock);
	list_add_tail(&child->part.node, &parent->partitions);
	mutex_unlock(&master->master.partitions_lock);

	ret = add_mtd_device(child);
	if (ret)
		goto err_remove_part;

	mtd_add_partition_attrs(child);

	return 0;

err_remove_part:
	mutex_lock(&master->master.partitions_lock);
	list_del(&child->part.node);
	mutex_unlock(&master->master.partitions_lock);

	free_partition(child);

	return ret;
}
EXPORT_SYMBOL_GPL(mtd_add_partition);

 
static int __mtd_del_partition(struct mtd_info *mtd)
{
	struct mtd_info *child, *next;
	int err;

	list_for_each_entry_safe(child, next, &mtd->partitions, part.node) {
		err = __mtd_del_partition(child);
		if (err)
			return err;
	}

	sysfs_remove_files(&mtd->dev.kobj, mtd_partition_attrs);

	list_del_init(&mtd->part.node);
	err = del_mtd_device(mtd);
	if (err)
		return err;

	return 0;
}

 
static int __del_mtd_partitions(struct mtd_info *mtd)
{
	struct mtd_info *child, *next;
	int ret, err = 0;

	list_for_each_entry_safe(child, next, &mtd->partitions, part.node) {
		if (mtd_has_partitions(child))
			__del_mtd_partitions(child);

		pr_info("Deleting %s MTD partition\n", child->name);
		list_del_init(&child->part.node);
		ret = del_mtd_device(child);
		if (ret < 0) {
			pr_err("Error when deleting partition \"%s\" (%d)\n",
			       child->name, ret);
			err = ret;
			continue;
		}
	}

	return err;
}

int del_mtd_partitions(struct mtd_info *mtd)
{
	struct mtd_info *master = mtd_get_master(mtd);
	int ret;

	pr_info("Deleting MTD partitions on \"%s\":\n", mtd->name);

	mutex_lock(&master->master.partitions_lock);
	ret = __del_mtd_partitions(mtd);
	mutex_unlock(&master->master.partitions_lock);

	return ret;
}

int mtd_del_partition(struct mtd_info *mtd, int partno)
{
	struct mtd_info *child, *master = mtd_get_master(mtd);
	int ret = -EINVAL;

	mutex_lock(&master->master.partitions_lock);
	list_for_each_entry(child, &mtd->partitions, part.node) {
		if (child->index == partno) {
			ret = __mtd_del_partition(child);
			break;
		}
	}
	mutex_unlock(&master->master.partitions_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mtd_del_partition);

 

int add_mtd_partitions(struct mtd_info *parent,
		       const struct mtd_partition *parts,
		       int nbparts)
{
	struct mtd_info *child, *master = mtd_get_master(parent);
	uint64_t cur_offset = 0;
	int i, ret;

	printk(KERN_NOTICE "Creating %d MTD partitions on \"%s\":\n",
	       nbparts, parent->name);

	for (i = 0; i < nbparts; i++) {
		child = allocate_partition(parent, parts + i, i, cur_offset);
		if (IS_ERR(child)) {
			ret = PTR_ERR(child);
			goto err_del_partitions;
		}

		mutex_lock(&master->master.partitions_lock);
		list_add_tail(&child->part.node, &parent->partitions);
		mutex_unlock(&master->master.partitions_lock);

		ret = add_mtd_device(child);
		if (ret) {
			mutex_lock(&master->master.partitions_lock);
			list_del(&child->part.node);
			mutex_unlock(&master->master.partitions_lock);

			free_partition(child);
			goto err_del_partitions;
		}

		mtd_add_partition_attrs(child);

		 
		parse_mtd_partitions(child, parts[i].types, NULL);

		cur_offset = child->part.offset + child->part.size;
	}

	return 0;

err_del_partitions:
	del_mtd_partitions(master);

	return ret;
}

static DEFINE_SPINLOCK(part_parser_lock);
static LIST_HEAD(part_parsers);

static struct mtd_part_parser *mtd_part_parser_get(const char *name)
{
	struct mtd_part_parser *p, *ret = NULL;

	spin_lock(&part_parser_lock);

	list_for_each_entry(p, &part_parsers, list)
		if (!strcmp(p->name, name) && try_module_get(p->owner)) {
			ret = p;
			break;
		}

	spin_unlock(&part_parser_lock);

	return ret;
}

static inline void mtd_part_parser_put(const struct mtd_part_parser *p)
{
	module_put(p->owner);
}

 
static void mtd_part_parser_cleanup_default(const struct mtd_partition *pparts,
					    int nr_parts)
{
	kfree(pparts);
}

int __register_mtd_parser(struct mtd_part_parser *p, struct module *owner)
{
	p->owner = owner;

	if (!p->cleanup)
		p->cleanup = &mtd_part_parser_cleanup_default;

	spin_lock(&part_parser_lock);
	list_add(&p->list, &part_parsers);
	spin_unlock(&part_parser_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(__register_mtd_parser);

void deregister_mtd_parser(struct mtd_part_parser *p)
{
	spin_lock(&part_parser_lock);
	list_del(&p->list);
	spin_unlock(&part_parser_lock);
}
EXPORT_SYMBOL_GPL(deregister_mtd_parser);

 
static const char * const default_mtd_part_types[] = {
	"cmdlinepart",
	"ofpart",
	NULL
};

 
static const char * const default_subpartition_types[] = {
	"ofpart",
	NULL
};

static int mtd_part_do_parse(struct mtd_part_parser *parser,
			     struct mtd_info *master,
			     struct mtd_partitions *pparts,
			     struct mtd_part_parser_data *data)
{
	int ret;

	ret = (*parser->parse_fn)(master, &pparts->parts, data);
	pr_debug("%s: parser %s: %i\n", master->name, parser->name, ret);
	if (ret <= 0)
		return ret;

	pr_notice("%d %s partitions found on MTD device %s\n", ret,
		  parser->name, master->name);

	pparts->nr_parts = ret;
	pparts->parser = parser;

	return ret;
}

 
static struct mtd_part_parser *mtd_part_get_compatible_parser(const char *compat)
{
	struct mtd_part_parser *p, *ret = NULL;

	spin_lock(&part_parser_lock);

	list_for_each_entry(p, &part_parsers, list) {
		const struct of_device_id *matches;

		matches = p->of_match_table;
		if (!matches)
			continue;

		for (; matches->compatible[0]; matches++) {
			if (!strcmp(matches->compatible, compat) &&
			    try_module_get(p->owner)) {
				ret = p;
				break;
			}
		}

		if (ret)
			break;
	}

	spin_unlock(&part_parser_lock);

	return ret;
}

static int mtd_part_of_parse(struct mtd_info *master,
			     struct mtd_partitions *pparts)
{
	struct mtd_part_parser *parser;
	struct device_node *np;
	struct device_node *child;
	struct property *prop;
	struct device *dev;
	const char *compat;
	const char *fixed = "fixed-partitions";
	int ret, err = 0;

	dev = &master->dev;
	 
	if (!IS_ENABLED(CONFIG_MTD_PARTITIONED_MASTER) && !mtd_is_partition(master))
		dev = master->dev.parent;

	np = mtd_get_of_node(master);
	if (mtd_is_partition(master))
		of_node_get(np);
	else
		np = of_get_child_by_name(np, "partitions");

	 
	for_each_child_of_node(np, child)
		if (of_device_is_compatible(child, "nvmem-cells"))
			of_node_set_flag(child, OF_POPULATED);

	of_property_for_each_string(np, "compatible", prop, compat) {
		parser = mtd_part_get_compatible_parser(compat);
		if (!parser)
			continue;
		ret = mtd_part_do_parse(parser, master, pparts, NULL);
		if (ret > 0) {
			of_platform_populate(np, NULL, NULL, dev);
			of_node_put(np);
			return ret;
		}
		mtd_part_parser_put(parser);
		if (ret < 0 && !err)
			err = ret;
	}
	of_platform_populate(np, NULL, NULL, dev);
	of_node_put(np);

	 
	parser = mtd_part_parser_get(fixed);
	if (!parser && !request_module("%s", fixed))
		parser = mtd_part_parser_get(fixed);
	if (parser) {
		ret = mtd_part_do_parse(parser, master, pparts, NULL);
		if (ret > 0)
			return ret;
		mtd_part_parser_put(parser);
		if (ret < 0 && !err)
			err = ret;
	}

	return err;
}

 
int parse_mtd_partitions(struct mtd_info *master, const char *const *types,
			 struct mtd_part_parser_data *data)
{
	struct mtd_partitions pparts = { };
	struct mtd_part_parser *parser;
	int ret, err = 0;

	if (!types)
		types = mtd_is_partition(master) ? default_subpartition_types :
			default_mtd_part_types;

	for ( ; *types; types++) {
		 
		if (!strcmp(*types, "ofpart")) {
			ret = mtd_part_of_parse(master, &pparts);
		} else {
			pr_debug("%s: parsing partitions %s\n", master->name,
				 *types);
			parser = mtd_part_parser_get(*types);
			if (!parser && !request_module("%s", *types))
				parser = mtd_part_parser_get(*types);
			pr_debug("%s: got parser %s\n", master->name,
				parser ? parser->name : NULL);
			if (!parser)
				continue;
			ret = mtd_part_do_parse(parser, master, &pparts, data);
			if (ret <= 0)
				mtd_part_parser_put(parser);
		}
		 
		if (ret > 0) {
			err = add_mtd_partitions(master, pparts.parts,
						 pparts.nr_parts);
			mtd_part_parser_cleanup(&pparts);
			return err ? err : pparts.nr_parts;
		}
		 
		if (ret < 0 && !err)
			err = ret;
	}
	return err;
}

void mtd_part_parser_cleanup(struct mtd_partitions *parts)
{
	const struct mtd_part_parser *parser;

	if (!parts)
		return;

	parser = parts->parser;
	if (parser) {
		if (parser->cleanup)
			parser->cleanup(parts->parts, parts->nr_parts);

		mtd_part_parser_put(parser);
	}
}

 
uint64_t mtd_get_device_size(const struct mtd_info *mtd)
{
	struct mtd_info *master = mtd_get_master((struct mtd_info *)mtd);

	return master->size;
}
EXPORT_SYMBOL_GPL(mtd_get_device_size);
