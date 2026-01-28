
#ifndef __NETCNT_COMMON_H
#define __NETCNT_COMMON_H

#include <linux/types.h>

#define MAX_PERCPU_PACKETS 32


#define SIZEOF_BPF_LOCAL_STORAGE_ELEM		768


#define BPF_LOCAL_STORAGE_MAX_VALUE_SIZE	(0xFFFF - \
						 SIZEOF_BPF_LOCAL_STORAGE_ELEM)

#define PCPU_MIN_UNIT_SIZE			32768

union percpu_net_cnt {
	struct {
		__u64 packets;
		__u64 bytes;

		__u64 prev_ts;

		__u64 prev_packets;
		__u64 prev_bytes;
	};
	__u8 data[PCPU_MIN_UNIT_SIZE];
};

union net_cnt {
	struct {
		__u64 packets;
		__u64 bytes;
	};
	__u8 data[BPF_LOCAL_STORAGE_MAX_VALUE_SIZE];
};

#endif
