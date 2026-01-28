


#ifndef DM_IMA_H
#define DM_IMA_H

#define DM_IMA_MEASUREMENT_BUF_LEN	4096
#define DM_IMA_DEVICE_BUF_LEN		1024
#define DM_IMA_TARGET_METADATA_BUF_LEN	128
#define DM_IMA_TARGET_DATA_BUF_LEN	2048
#define DM_IMA_DEVICE_CAPACITY_BUF_LEN	128
#define DM_IMA_TABLE_HASH_ALG		"sha256"

#define __dm_ima_stringify(s) #s
#define __dm_ima_str(s) __dm_ima_stringify(s)

#define DM_IMA_VERSION_STR "dm_version="	\
	__dm_ima_str(DM_VERSION_MAJOR) "."	\
	__dm_ima_str(DM_VERSION_MINOR) "."	\
	__dm_ima_str(DM_VERSION_PATCHLEVEL) ";"

#ifdef CONFIG_IMA

struct dm_ima_device_table_metadata {
	
	char *device_metadata;
	unsigned int device_metadata_len;
	unsigned int num_targets;

	
	char *hash;
	unsigned int hash_len;
};


struct dm_ima_measurements {
	struct dm_ima_device_table_metadata active_table;
	struct dm_ima_device_table_metadata inactive_table;
	unsigned int dm_version_str_len;
};

void dm_ima_reset_data(struct mapped_device *md);
void dm_ima_measure_on_table_load(struct dm_table *table, unsigned int status_flags);
void dm_ima_measure_on_device_resume(struct mapped_device *md, bool swap);
void dm_ima_measure_on_device_remove(struct mapped_device *md, bool remove_all);
void dm_ima_measure_on_table_clear(struct mapped_device *md, bool new_map);
void dm_ima_measure_on_device_rename(struct mapped_device *md);

#else

static inline void dm_ima_reset_data(struct mapped_device *md) {}
static inline void dm_ima_measure_on_table_load(struct dm_table *table, unsigned int status_flags) {}
static inline void dm_ima_measure_on_device_resume(struct mapped_device *md, bool swap) {}
static inline void dm_ima_measure_on_device_remove(struct mapped_device *md, bool remove_all) {}
static inline void dm_ima_measure_on_table_clear(struct mapped_device *md, bool new_map) {}
static inline void dm_ima_measure_on_device_rename(struct mapped_device *md) {}

#endif 

#endif 
