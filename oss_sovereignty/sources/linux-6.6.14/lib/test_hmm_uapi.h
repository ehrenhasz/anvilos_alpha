

#ifndef _LIB_TEST_HMM_UAPI_H
#define _LIB_TEST_HMM_UAPI_H

#include <linux/types.h>
#include <linux/ioctl.h>


struct hmm_dmirror_cmd {
	__u64		addr;
	__u64		ptr;
	__u64		npages;
	__u64		cpages;
	__u64		faults;
};


#define HMM_DMIRROR_READ		_IOWR('H', 0x00, struct hmm_dmirror_cmd)
#define HMM_DMIRROR_WRITE		_IOWR('H', 0x01, struct hmm_dmirror_cmd)
#define HMM_DMIRROR_MIGRATE_TO_DEV	_IOWR('H', 0x02, struct hmm_dmirror_cmd)
#define HMM_DMIRROR_MIGRATE_TO_SYS	_IOWR('H', 0x03, struct hmm_dmirror_cmd)
#define HMM_DMIRROR_SNAPSHOT		_IOWR('H', 0x04, struct hmm_dmirror_cmd)
#define HMM_DMIRROR_EXCLUSIVE		_IOWR('H', 0x05, struct hmm_dmirror_cmd)
#define HMM_DMIRROR_CHECK_EXCLUSIVE	_IOWR('H', 0x06, struct hmm_dmirror_cmd)
#define HMM_DMIRROR_RELEASE		_IOWR('H', 0x07, struct hmm_dmirror_cmd)


enum {
	HMM_DMIRROR_PROT_ERROR			= 0xFF,
	HMM_DMIRROR_PROT_NONE			= 0x00,
	HMM_DMIRROR_PROT_READ			= 0x01,
	HMM_DMIRROR_PROT_WRITE			= 0x02,
	HMM_DMIRROR_PROT_PMD			= 0x04,
	HMM_DMIRROR_PROT_PUD			= 0x08,
	HMM_DMIRROR_PROT_ZERO			= 0x10,
	HMM_DMIRROR_PROT_DEV_PRIVATE_LOCAL	= 0x20,
	HMM_DMIRROR_PROT_DEV_PRIVATE_REMOTE	= 0x30,
	HMM_DMIRROR_PROT_DEV_COHERENT_LOCAL	= 0x40,
	HMM_DMIRROR_PROT_DEV_COHERENT_REMOTE	= 0x50,
};

enum {
	
	HMM_DMIRROR_MEMORY_DEVICE_PRIVATE = 1,
	HMM_DMIRROR_MEMORY_DEVICE_COHERENT,
};

#endif 
