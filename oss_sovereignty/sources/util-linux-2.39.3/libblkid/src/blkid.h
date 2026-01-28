

#ifndef _BLKID_BLKID_H
#define _BLKID_BLKID_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLKID_VERSION   "2.39.3"
#define BLKID_DATE      "04-Dec-2023"


typedef struct blkid_struct_dev *blkid_dev;


typedef struct blkid_struct_cache *blkid_cache;


typedef struct blkid_struct_probe *blkid_probe;


typedef struct blkid_struct_topology *blkid_topology;


typedef struct blkid_struct_partlist *blkid_partlist;


typedef struct blkid_struct_partition *blkid_partition;


typedef struct blkid_struct_parttable *blkid_parttable;


typedef int64_t blkid_loff_t;


typedef struct blkid_struct_tag_iterate *blkid_tag_iterate;


typedef struct blkid_struct_dev_iterate *blkid_dev_iterate;


#define BLKID_DEV_FIND		0x0000
#define BLKID_DEV_CREATE	0x0001
#define BLKID_DEV_VERIFY	0x0002
#define BLKID_DEV_NORMAL	(BLKID_DEV_CREATE | BLKID_DEV_VERIFY)


#ifndef __GNUC_PREREQ
# if defined __GNUC__ && defined __GNUC_MINOR__
#  define __GNUC_PREREQ(maj, min)  ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# else
#  define __GNUC_PREREQ(maj, min) 0
# endif
#endif

#ifndef __ul_attribute__
# if __GNUC_PREREQ (3, 4)
#  define __ul_attribute__(_a_) __attribute__(_a_)
# else
#  define __ul_attribute__(_a_)
# endif
#endif


extern void blkid_init_debug(int mask);


extern void blkid_put_cache(blkid_cache cache);
extern int blkid_get_cache(blkid_cache *cache, const char *filename);
extern void blkid_gc_cache(blkid_cache cache);


extern const char *blkid_dev_devname(blkid_dev dev)
			__ul_attribute__((warn_unused_result));

extern blkid_dev_iterate blkid_dev_iterate_begin(blkid_cache cache);
extern int blkid_dev_set_search(blkid_dev_iterate iter,
				const char *search_type, const char *search_value);
extern int blkid_dev_next(blkid_dev_iterate iterate, blkid_dev *dev);
extern void blkid_dev_iterate_end(blkid_dev_iterate iterate);


extern char *blkid_devno_to_devname(dev_t devno)
			__ul_attribute__((warn_unused_result));
extern int blkid_devno_to_wholedisk(dev_t dev, char *diskname,
                        size_t len, dev_t *diskdevno)
			__ul_attribute__((warn_unused_result));


extern int blkid_probe_all(blkid_cache cache);
extern int blkid_probe_all_new(blkid_cache cache);
extern int blkid_probe_all_removable(blkid_cache cache);

extern blkid_dev blkid_get_dev(blkid_cache cache, const char *devname, int flags);


extern blkid_loff_t blkid_get_dev_size(int fd);


extern blkid_dev blkid_verify(blkid_cache cache, blkid_dev dev);




extern char *blkid_get_tag_value(blkid_cache cache, const char *tagname,
				       const char *devname)
			__ul_attribute__((warn_unused_result));
extern char *blkid_get_devname(blkid_cache cache, const char *token,
			       const char *value)
			__ul_attribute__((warn_unused_result));


extern blkid_tag_iterate blkid_tag_iterate_begin(blkid_dev dev);
extern int blkid_tag_next(blkid_tag_iterate iterate,
			      const char **type, const char **value);
extern void blkid_tag_iterate_end(blkid_tag_iterate iterate);
extern int blkid_dev_has_tag(blkid_dev dev, const char *type, const char *value);

extern blkid_dev blkid_find_dev_with_tag(blkid_cache cache,
					 const char *type,
					 const char *value);

extern int blkid_parse_tag_string(const char *token, char **ret_type, char **ret_val);


extern int blkid_parse_version_string(const char *ver_string)
			__ul_attribute__((nonnull));
extern int blkid_get_library_version(const char **ver_string,
				     const char **date_string);


extern int blkid_encode_string(const char *str, char *str_enc, size_t len);
extern int blkid_safe_string(const char *str, char *str_safe, size_t len);


extern int blkid_send_uevent(const char *devname, const char *action);
extern char *blkid_evaluate_tag(const char *token, const char *value,
				blkid_cache *cache)
			__ul_attribute__((warn_unused_result));
extern char *blkid_evaluate_spec(const char *spec, blkid_cache *cache)
			__ul_attribute__((warn_unused_result));


extern blkid_probe blkid_new_probe(void)
			__ul_attribute__((warn_unused_result));
extern blkid_probe blkid_new_probe_from_filename(const char *filename)
			__ul_attribute__((warn_unused_result))
			__ul_attribute__((nonnull));
extern void blkid_free_probe(blkid_probe pr);

extern void blkid_reset_probe(blkid_probe pr);
extern int blkid_probe_reset_buffers(blkid_probe pr);
extern int blkid_probe_hide_range(blkid_probe pr, uint64_t off, uint64_t len);

extern int blkid_probe_set_device(blkid_probe pr, int fd,
	                blkid_loff_t off, blkid_loff_t size)
			__ul_attribute__((nonnull));

extern dev_t blkid_probe_get_devno(blkid_probe pr)
			__ul_attribute__((nonnull));

extern dev_t blkid_probe_get_wholedisk_devno(blkid_probe pr)
			__ul_attribute__((nonnull));

extern int blkid_probe_is_wholedisk(blkid_probe pr)
			__ul_attribute__((nonnull));

extern blkid_loff_t blkid_probe_get_size(blkid_probe pr)
			__ul_attribute__((nonnull));
extern blkid_loff_t blkid_probe_get_offset(blkid_probe pr)
			__ul_attribute__((nonnull));
extern unsigned int blkid_probe_get_sectorsize(blkid_probe pr)
			__ul_attribute__((nonnull));
extern int blkid_probe_set_sectorsize(blkid_probe pr, unsigned int sz)
			__ul_attribute__((nonnull));
extern blkid_loff_t blkid_probe_get_sectors(blkid_probe pr)
			__ul_attribute__((nonnull));

extern int blkid_probe_get_fd(blkid_probe pr)
			__ul_attribute__((nonnull));

extern int blkid_probe_set_hint(blkid_probe pr, const char *name, uint64_t value)
			__ul_attribute__((nonnull));
extern void blkid_probe_reset_hints(blkid_probe pr)
			__ul_attribute__((nonnull));


extern int blkid_known_fstype(const char *fstype)
			__ul_attribute__((nonnull));

extern int blkid_superblocks_get_name(size_t idx, const char **name, int *usage);

extern int blkid_probe_enable_superblocks(blkid_probe pr, int enable)
			__ul_attribute__((nonnull));

#define BLKID_SUBLKS_LABEL	(1 << 1) 
#define BLKID_SUBLKS_LABELRAW	(1 << 2) 
#define BLKID_SUBLKS_UUID	(1 << 3) 
#define BLKID_SUBLKS_UUIDRAW	(1 << 4) 
#define BLKID_SUBLKS_TYPE	(1 << 5) 
#define BLKID_SUBLKS_SECTYPE	(1 << 6) 
#define BLKID_SUBLKS_USAGE	(1 << 7) 
#define BLKID_SUBLKS_VERSION	(1 << 8) 
#define BLKID_SUBLKS_MAGIC	(1 << 9) 
#define BLKID_SUBLKS_BADCSUM	(1 << 10) 
#define BLKID_SUBLKS_FSINFO	(1 << 11) 

#define BLKID_SUBLKS_DEFAULT	(BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID | \
				 BLKID_SUBLKS_TYPE | BLKID_SUBLKS_SECTYPE)

extern int blkid_probe_set_superblocks_flags(blkid_probe pr, int flags)
			__ul_attribute__((nonnull));
extern int blkid_probe_reset_superblocks_filter(blkid_probe pr)
			__ul_attribute__((nonnull));
extern int blkid_probe_invert_superblocks_filter(blkid_probe pr)
			__ul_attribute__((nonnull));


#define BLKID_FLTR_NOTIN		1

#define BLKID_FLTR_ONLYIN		2
extern int blkid_probe_filter_superblocks_type(blkid_probe pr, int flag, char *names[])
			__ul_attribute__((nonnull));

#define BLKID_USAGE_FILESYSTEM		(1 << 1)
#define BLKID_USAGE_RAID		(1 << 2)
#define BLKID_USAGE_CRYPTO		(1 << 3)
#define BLKID_USAGE_OTHER		(1 << 4)
extern int blkid_probe_filter_superblocks_usage(blkid_probe pr, int flag, int usage)
			__ul_attribute__((nonnull));


extern int blkid_probe_enable_topology(blkid_probe pr, int enable)
			__ul_attribute__((nonnull));


extern blkid_topology blkid_probe_get_topology(blkid_probe pr)
			__ul_attribute__((nonnull));

extern unsigned long blkid_topology_get_alignment_offset(blkid_topology tp)
			__ul_attribute__((nonnull));
extern unsigned long blkid_topology_get_minimum_io_size(blkid_topology tp)
			__ul_attribute__((nonnull));
extern unsigned long blkid_topology_get_optimal_io_size(blkid_topology tp)
			__ul_attribute__((nonnull));
extern unsigned long blkid_topology_get_logical_sector_size(blkid_topology tp)
			__ul_attribute__((nonnull));
extern unsigned long blkid_topology_get_physical_sector_size(blkid_topology tp)
			__ul_attribute__((nonnull));
extern unsigned long blkid_topology_get_dax(blkid_topology tp)
			__ul_attribute__((nonnull));
extern uint64_t blkid_topology_get_diskseq(blkid_topology tp)
			__ul_attribute__((nonnull));


extern int blkid_known_pttype(const char *pttype);
extern int blkid_partitions_get_name(const size_t idx, const char **name);

extern int blkid_probe_enable_partitions(blkid_probe pr, int enable)
			__ul_attribute__((nonnull));

extern int blkid_probe_reset_partitions_filter(blkid_probe pr)
			__ul_attribute__((nonnull));
extern int blkid_probe_invert_partitions_filter(blkid_probe pr)
			__ul_attribute__((nonnull));
extern int blkid_probe_filter_partitions_type(blkid_probe pr, int flag, char *names[])
			__ul_attribute__((nonnull));


#define BLKID_PARTS_FORCE_GPT		(1 << 1)
#define BLKID_PARTS_ENTRY_DETAILS	(1 << 2)
#define BLKID_PARTS_MAGIC		(1 << 3)
extern int blkid_probe_set_partitions_flags(blkid_probe pr, int flags)
			__ul_attribute__((nonnull));


extern blkid_partlist blkid_probe_get_partitions(blkid_probe pr)
			__ul_attribute__((nonnull));

extern int blkid_partlist_numof_partitions(blkid_partlist ls)
			__ul_attribute__((nonnull));
extern blkid_parttable blkid_partlist_get_table(blkid_partlist ls)
			__ul_attribute__((nonnull));
extern blkid_partition blkid_partlist_get_partition(blkid_partlist ls, int n)
			__ul_attribute__((nonnull));
extern blkid_partition blkid_partlist_get_partition_by_partno(blkid_partlist ls, int n)
			__ul_attribute__((nonnull));
extern blkid_partition blkid_partlist_devno_to_partition(blkid_partlist ls, dev_t devno)
			__ul_attribute__((nonnull));
extern blkid_parttable blkid_partition_get_table(blkid_partition par)
			__ul_attribute__((nonnull));

extern const char *blkid_partition_get_name(blkid_partition par)
			__ul_attribute__((nonnull));
extern const char *blkid_partition_get_uuid(blkid_partition par)
			__ul_attribute__((nonnull));
extern int blkid_partition_get_partno(blkid_partition par)
			__ul_attribute__((nonnull));
extern blkid_loff_t blkid_partition_get_start(blkid_partition par)
			__ul_attribute__((nonnull));
extern blkid_loff_t blkid_partition_get_size(blkid_partition par)
			__ul_attribute__((nonnull));

extern int blkid_partition_get_type(blkid_partition par)
			__ul_attribute__((nonnull));
extern const char *blkid_partition_get_type_string(blkid_partition par)
			__ul_attribute__((nonnull));
extern unsigned long long blkid_partition_get_flags(blkid_partition par)
			__ul_attribute__((nonnull));

extern int blkid_partition_is_logical(blkid_partition par)
			__ul_attribute__((nonnull));
extern int blkid_partition_is_extended(blkid_partition par)
			__ul_attribute__((nonnull));
extern int blkid_partition_is_primary(blkid_partition par)
			__ul_attribute__((nonnull));

extern const char *blkid_parttable_get_type(blkid_parttable tab)
			__ul_attribute__((nonnull));
extern const char *blkid_parttable_get_id(blkid_parttable tab)
			__ul_attribute__((nonnull));
extern blkid_loff_t blkid_parttable_get_offset(blkid_parttable tab)
			__ul_attribute__((nonnull));
extern blkid_partition blkid_parttable_get_parent(blkid_parttable tab)
			__ul_attribute__((nonnull));


extern int blkid_do_probe(blkid_probe pr)
			__ul_attribute__((nonnull));
extern int blkid_do_safeprobe(blkid_probe pr)
			__ul_attribute__((nonnull));
extern int blkid_do_fullprobe(blkid_probe pr)
			__ul_attribute__((nonnull));


#define BLKID_PROBE_OK	0

#define BLKID_PROBE_NONE	1

#define BLKID_PROBE_ERROR	-1

#define BLKID_PROBE_AMBIGUOUS	-2

extern int blkid_probe_numof_values(blkid_probe pr)
			__ul_attribute__((nonnull));
extern int blkid_probe_get_value(blkid_probe pr, int num, const char **name,
                        const char **data, size_t *len)
			__ul_attribute__((nonnull(1)));
extern int blkid_probe_lookup_value(blkid_probe pr, const char *name,
                        const char **data, size_t *len)
			__ul_attribute__((nonnull(1, 2)));
extern int blkid_probe_has_value(blkid_probe pr, const char *name)
			__ul_attribute__((nonnull));
extern int blkid_do_wipe(blkid_probe pr, int dryrun)
			__ul_attribute__((nonnull));
extern int blkid_probe_step_back(blkid_probe pr)
			__ul_attribute__((nonnull));


#ifndef BLKID_DISABLE_DEPRECATED

#define BLKID_PROBREQ_LABEL     BLKID_SUBLKS_LABEL
#define BLKID_PROBREQ_LABELRAW  BLKID_SUBLKS_LABELRAW
#define BLKID_PROBREQ_UUID      BLKID_SUBLKS_UUID
#define BLKID_PROBREQ_UUIDRAW   BLKID_SUBLKS_UUIDRAW
#define BLKID_PROBREQ_TYPE      BLKID_SUBLKS_TYPE
#define BLKID_PROBREQ_SECTYPE   BLKID_SUBLKS_SECTYPE
#define BLKID_PROBREQ_USAGE     BLKID_SUBLKS_USAGE
#define BLKID_PROBREQ_VERSION   BLKID_SUBLKS_VERSION

extern int blkid_probe_set_request(blkid_probe pr, int flags)
			__ul_attribute__((deprecated));

extern int blkid_probe_filter_usage(blkid_probe pr, int flag, int usage)
			__ul_attribute__((deprecated));

extern int blkid_probe_filter_types(blkid_probe pr, int flag, char *names[])
			__ul_attribute__((deprecated));

extern int blkid_probe_invert_filter(blkid_probe pr)
			__ul_attribute__((deprecated));

extern int blkid_probe_reset_filter(blkid_probe pr)
			__ul_attribute__((deprecated));

#endif 

#ifdef __cplusplus
}
#endif

#endif 
