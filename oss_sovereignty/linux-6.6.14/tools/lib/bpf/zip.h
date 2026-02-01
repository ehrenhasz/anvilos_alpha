 

#ifndef __LIBBPF_ZIP_H
#define __LIBBPF_ZIP_H

#include <linux/types.h>

 
struct zip_archive;

 
struct zip_entry {
	 
	__u16 compression;

	 
	const char *name;
	 
	__u16 name_length;

	 
	const void *data;
	 
	__u32 data_length;
	 
	__u32 data_offset;
};

 
struct zip_archive *zip_archive_open(const char *path);

 
void zip_archive_close(struct zip_archive *archive);

 
int zip_archive_find_entry(struct zip_archive *archive, const char *name, struct zip_entry *out);

#endif
