 
#ifndef __LINUX_KVM_S390_H
#define __LINUX_KVM_S390_H
 
#include <linux/types.h>

#define __KVM_S390
#define __KVM_HAVE_GUEST_DEBUG

 
#define KVM_DEV_FLIC_GET_ALL_IRQS	1
#define KVM_DEV_FLIC_ENQUEUE		2
#define KVM_DEV_FLIC_CLEAR_IRQS		3
#define KVM_DEV_FLIC_APF_ENABLE		4
#define KVM_DEV_FLIC_APF_DISABLE_WAIT	5
#define KVM_DEV_FLIC_ADAPTER_REGISTER	6
#define KVM_DEV_FLIC_ADAPTER_MODIFY	7
#define KVM_DEV_FLIC_CLEAR_IO_IRQ	8
#define KVM_DEV_FLIC_AISM		9
#define KVM_DEV_FLIC_AIRQ_INJECT	10
#define KVM_DEV_FLIC_AISM_ALL		11
 
#define KVM_S390_MAX_FLOAT_IRQS	266250
#define KVM_S390_FLIC_MAX_BUFFER	0x2000000

struct kvm_s390_io_adapter {
	__u32 id;
	__u8 isc;
	__u8 maskable;
	__u8 swap;
	__u8 flags;
};

#define KVM_S390_ADAPTER_SUPPRESSIBLE 0x01

struct kvm_s390_ais_req {
	__u8 isc;
	__u16 mode;
};

struct kvm_s390_ais_all {
	__u8 simm;
	__u8 nimm;
};

#define KVM_S390_IO_ADAPTER_MASK 1
#define KVM_S390_IO_ADAPTER_MAP 2
#define KVM_S390_IO_ADAPTER_UNMAP 3

struct kvm_s390_io_adapter_req {
	__u32 id;
	__u8 type;
	__u8 mask;
	__u16 pad0;
	__u64 addr;
};

 
#define KVM_S390_VM_MEM_CTRL		0
#define KVM_S390_VM_TOD			1
#define KVM_S390_VM_CRYPTO		2
#define KVM_S390_VM_CPU_MODEL		3
#define KVM_S390_VM_MIGRATION		4
#define KVM_S390_VM_CPU_TOPOLOGY	5

 
#define KVM_S390_VM_MEM_ENABLE_CMMA	0
#define KVM_S390_VM_MEM_CLR_CMMA	1
#define KVM_S390_VM_MEM_LIMIT_SIZE	2

#define KVM_S390_NO_MEM_LIMIT		U64_MAX

 
#define KVM_S390_VM_TOD_LOW		0
#define KVM_S390_VM_TOD_HIGH		1
#define KVM_S390_VM_TOD_EXT		2

struct kvm_s390_vm_tod_clock {
	__u8  epoch_idx;
	__u64 tod;
};

 
 
#define KVM_S390_VM_CPU_PROCESSOR	0
struct kvm_s390_vm_cpu_processor {
	__u64 cpuid;
	__u16 ibc;
	__u8  pad[6];
	__u64 fac_list[256];
};

 
#define KVM_S390_VM_CPU_MACHINE		1
struct kvm_s390_vm_cpu_machine {
	__u64 cpuid;
	__u32 ibc;
	__u8  pad[4];
	__u64 fac_mask[256];
	__u64 fac_list[256];
};

#define KVM_S390_VM_CPU_PROCESSOR_FEAT	2
#define KVM_S390_VM_CPU_MACHINE_FEAT	3

#define KVM_S390_VM_CPU_FEAT_NR_BITS	1024
#define KVM_S390_VM_CPU_FEAT_ESOP	0
#define KVM_S390_VM_CPU_FEAT_SIEF2	1
#define KVM_S390_VM_CPU_FEAT_64BSCAO	2
#define KVM_S390_VM_CPU_FEAT_SIIF	3
#define KVM_S390_VM_CPU_FEAT_GPERE	4
#define KVM_S390_VM_CPU_FEAT_GSLS	5
#define KVM_S390_VM_CPU_FEAT_IB		6
#define KVM_S390_VM_CPU_FEAT_CEI	7
#define KVM_S390_VM_CPU_FEAT_IBS	8
#define KVM_S390_VM_CPU_FEAT_SKEY	9
#define KVM_S390_VM_CPU_FEAT_CMMA	10
#define KVM_S390_VM_CPU_FEAT_PFMFI	11
#define KVM_S390_VM_CPU_FEAT_SIGPIF	12
#define KVM_S390_VM_CPU_FEAT_KSS	13
struct kvm_s390_vm_cpu_feat {
	__u64 feat[16];
};

#define KVM_S390_VM_CPU_PROCESSOR_SUBFUNC	4
#define KVM_S390_VM_CPU_MACHINE_SUBFUNC		5
 
struct kvm_s390_vm_cpu_subfunc {
	__u8 plo[32];		 
	__u8 ptff[16];		 
	__u8 kmac[16];		 
	__u8 kmc[16];		 
	__u8 km[16];		 
	__u8 kimd[16];		 
	__u8 klmd[16];		 
	__u8 pckmo[16];		 
	__u8 kmctr[16];		 
	__u8 kmf[16];		 
	__u8 kmo[16];		 
	__u8 pcc[16];		 
	__u8 ppno[16];		 
	__u8 kma[16];		 
	__u8 kdsa[16];		 
	__u8 sortl[32];		 
	__u8 dfltcc[32];	 
	__u8 reserved[1728];
};

 
#define KVM_S390_VM_CRYPTO_ENABLE_AES_KW	0
#define KVM_S390_VM_CRYPTO_ENABLE_DEA_KW	1
#define KVM_S390_VM_CRYPTO_DISABLE_AES_KW	2
#define KVM_S390_VM_CRYPTO_DISABLE_DEA_KW	3
#define KVM_S390_VM_CRYPTO_ENABLE_APIE		4
#define KVM_S390_VM_CRYPTO_DISABLE_APIE		5

 
#define KVM_S390_VM_MIGRATION_STOP	0
#define KVM_S390_VM_MIGRATION_START	1
#define KVM_S390_VM_MIGRATION_STATUS	2

 
struct kvm_regs {
	 
	__u64 gprs[16];
};

 
struct kvm_sregs {
	__u32 acrs[16];
	__u64 crs[16];
};

 
struct kvm_fpu {
	__u32 fpc;
	__u64 fprs[16];
};

#define KVM_GUESTDBG_USE_HW_BP		0x00010000

#define KVM_HW_BP			1
#define KVM_HW_WP_WRITE			2
#define KVM_SINGLESTEP			4

struct kvm_debug_exit_arch {
	__u64 addr;
	__u8 type;
	__u8 pad[7];  
};

struct kvm_hw_breakpoint {
	__u64 addr;
	__u64 phys_addr;
	__u64 len;
	__u8 type;
	__u8 pad[7];  
};

 
struct kvm_guest_debug_arch {
	__u32 nr_hw_bp;
	__u32 pad;  
	struct kvm_hw_breakpoint __user *hw_bp;
};

 
#define KVM_S390_PFAULT_TOKEN_INVALID	0xffffffffffffffffULL

#define KVM_SYNC_PREFIX (1UL << 0)
#define KVM_SYNC_GPRS   (1UL << 1)
#define KVM_SYNC_ACRS   (1UL << 2)
#define KVM_SYNC_CRS    (1UL << 3)
#define KVM_SYNC_ARCH0  (1UL << 4)
#define KVM_SYNC_PFAULT (1UL << 5)
#define KVM_SYNC_VRS    (1UL << 6)
#define KVM_SYNC_RICCB  (1UL << 7)
#define KVM_SYNC_FPRS   (1UL << 8)
#define KVM_SYNC_GSCB   (1UL << 9)
#define KVM_SYNC_BPBC   (1UL << 10)
#define KVM_SYNC_ETOKEN (1UL << 11)
#define KVM_SYNC_DIAG318 (1UL << 12)

#define KVM_SYNC_S390_VALID_FIELDS \
	(KVM_SYNC_PREFIX | KVM_SYNC_GPRS | KVM_SYNC_ACRS | KVM_SYNC_CRS | \
	 KVM_SYNC_ARCH0 | KVM_SYNC_PFAULT | KVM_SYNC_VRS | KVM_SYNC_RICCB | \
	 KVM_SYNC_FPRS | KVM_SYNC_GSCB | KVM_SYNC_BPBC | KVM_SYNC_ETOKEN | \
	 KVM_SYNC_DIAG318)

 
#define SDNXC 8
#define SDNXL (1UL << SDNXC)
 
struct kvm_sync_regs {
	__u64 prefix;	 
	__u64 gprs[16];	 
	__u32 acrs[16];	 
	__u64 crs[16];	 
	__u64 todpr;	 
	__u64 cputm;	 
	__u64 ckc;	 
	__u64 pp;	 
	__u64 gbea;	 
	__u64 pft;	 
	__u64 pfs;	 
	__u64 pfc;	 
	union {
		__u64 vrs[32][2];	 
		__u64 fprs[16];		 
	};
	__u8  reserved[512];	 
	__u32 fpc;		 
	__u8 bpbc : 1;		 
	__u8 reserved2 : 7;
	__u8 padding1[51];	 
	__u8 riccb[64];		 
	__u64 diag318;		 
	__u8 padding2[184];	 
	union {
		__u8 sdnx[SDNXL];   
		struct {
			__u64 reserved1[2];
			__u64 gscb[4];
			__u64 etoken;
			__u64 etoken_extension;
		};
	};
};

#define KVM_REG_S390_TODPR	(KVM_REG_S390 | KVM_REG_SIZE_U32 | 0x1)
#define KVM_REG_S390_EPOCHDIFF	(KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x2)
#define KVM_REG_S390_CPU_TIMER  (KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x3)
#define KVM_REG_S390_CLOCK_COMP (KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x4)
#define KVM_REG_S390_PFTOKEN	(KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x5)
#define KVM_REG_S390_PFCOMPARE	(KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x6)
#define KVM_REG_S390_PFSELECT	(KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x7)
#define KVM_REG_S390_PP		(KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x8)
#define KVM_REG_S390_GBEA	(KVM_REG_S390 | KVM_REG_SIZE_U64 | 0x9)
#endif
