 
 

#ifndef _IFS_H_
#define _IFS_H_

 
#include <linux/device.h>
#include <linux/miscdevice.h>

#define MSR_ARRAY_BIST				0x00000105
#define MSR_COPY_SCAN_HASHES			0x000002c2
#define MSR_SCAN_HASHES_STATUS			0x000002c3
#define MSR_AUTHENTICATE_AND_COPY_CHUNK		0x000002c4
#define MSR_CHUNKS_AUTHENTICATION_STATUS	0x000002c5
#define MSR_ACTIVATE_SCAN			0x000002c6
#define MSR_SCAN_STATUS				0x000002c7
#define SCAN_NOT_TESTED				0
#define SCAN_TEST_PASS				1
#define SCAN_TEST_FAIL				2

#define IFS_TYPE_SAF			0
#define IFS_TYPE_ARRAY_BIST		1

 
union ifs_scan_hashes_status {
	u64	data;
	struct {
		u32	chunk_size	:16;
		u32	num_chunks	:8;
		u32	rsvd1		:8;
		u32	error_code	:8;
		u32	rsvd2		:11;
		u32	max_core_limit	:12;
		u32	valid		:1;
	};
};

 
union ifs_chunks_auth_status {
	u64	data;
	struct {
		u32	valid_chunks	:8;
		u32	total_chunks	:8;
		u32	rsvd1		:16;
		u32	error_code	:8;
		u32	rsvd2		:24;
	};
};

 
union ifs_scan {
	u64	data;
	struct {
		u32	start	:8;
		u32	stop	:8;
		u32	rsvd	:16;
		u32	delay	:31;
		u32	sigmce	:1;
	};
};

 
union ifs_status {
	u64	data;
	struct {
		u32	chunk_num		:8;
		u32	chunk_stop_index	:8;
		u32	rsvd1			:16;
		u32	error_code		:8;
		u32	rsvd2			:22;
		u32	control_error		:1;
		u32	signature_error		:1;
	};
};

 
union ifs_array {
	u64	data;
	struct {
		u32	array_bitmask;
		u16	array_bank;
		u16	rsvd			:15;
		u16	ctrl_result		:1;
	};
};

 
#define IFS_SW_TIMEOUT				0xFD
#define IFS_SW_PARTIAL_COMPLETION		0xFE

struct ifs_test_caps {
	int	integrity_cap_bit;
	int	test_num;
};

 
struct ifs_data {
	int	loaded_version;
	bool	loaded;
	bool	loading_error;
	int	valid_chunks;
	int	status;
	u64	scan_details;
	u32	cur_batch;
};

struct ifs_work {
	struct work_struct w;
	struct device *dev;
};

struct ifs_device {
	const struct ifs_test_caps *test_caps;
	struct ifs_data rw_data;
	struct miscdevice misc;
};

static inline struct ifs_data *ifs_get_data(struct device *dev)
{
	struct miscdevice *m = dev_get_drvdata(dev);
	struct ifs_device *d = container_of(m, struct ifs_device, misc);

	return &d->rw_data;
}

static inline const struct ifs_test_caps *ifs_get_test_caps(struct device *dev)
{
	struct miscdevice *m = dev_get_drvdata(dev);
	struct ifs_device *d = container_of(m, struct ifs_device, misc);

	return d->test_caps;
}

extern bool *ifs_pkg_auth;
int ifs_load_firmware(struct device *dev);
int do_core_test(int cpu, struct device *dev);
extern struct attribute *plat_ifs_attrs[];
extern struct attribute *plat_ifs_array_attrs[];

#endif
