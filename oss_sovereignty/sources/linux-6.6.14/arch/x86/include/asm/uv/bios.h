
#ifndef _ASM_X86_UV_BIOS_H
#define _ASM_X86_UV_BIOS_H



#include <linux/efi.h>
#include <linux/rtc.h>


enum uv_bios_cmd {
	UV_BIOS_COMMON,
	UV_BIOS_GET_SN_INFO,
	UV_BIOS_FREQ_BASE,
	UV_BIOS_WATCHLIST_ALLOC,
	UV_BIOS_WATCHLIST_FREE,
	UV_BIOS_MEMPROTECT,
	UV_BIOS_GET_PARTITION_ADDR,
	UV_BIOS_SET_LEGACY_VGA_TARGET
};

#define UV_BIOS_EXTRA			    0x10000
#define UV_BIOS_GET_PCI_TOPOLOGY	    0x10001
#define UV_BIOS_GET_GEOINFO		    0x10003

#define UV_BIOS_EXTRA_OP_MEM_COPYIN	    0x1000
#define UV_BIOS_EXTRA_OP_MEM_COPYOUT	    0x2000
#define UV_BIOS_EXTRA_OP_MASK		    0x0fff
#define UV_BIOS_EXTRA_GET_HEAPSIZE	    1
#define UV_BIOS_EXTRA_INSTALL_HEAP	    2
#define UV_BIOS_EXTRA_MASTER_NASID	    3
#define UV_BIOS_EXTRA_OBJECT_COUNT	    (10|UV_BIOS_EXTRA_OP_MEM_COPYOUT)
#define UV_BIOS_EXTRA_ENUM_OBJECTS	    (12|UV_BIOS_EXTRA_OP_MEM_COPYOUT)
#define UV_BIOS_EXTRA_ENUM_PORTS	    (13|UV_BIOS_EXTRA_OP_MEM_COPYOUT)


enum {
	BIOS_STATUS_MORE_PASSES		=  1,
	BIOS_STATUS_SUCCESS		=  0,
	BIOS_STATUS_UNIMPLEMENTED	= -ENOSYS,
	BIOS_STATUS_EINVAL		= -EINVAL,
	BIOS_STATUS_UNAVAIL		= -EBUSY,
	BIOS_STATUS_ABORT		= -EINTR,
};


struct uv_gam_parameters {
	u64	mmr_base;
	u64	gru_base;
	u8	mmr_shift;	
	u8	gru_shift;	
	u8	gpa_shift;	
	u8	unused1;
};


#define UV_GAM_RANGE_TYPE_UNUSED	0 
#define UV_GAM_RANGE_TYPE_RAM		1 
#define UV_GAM_RANGE_TYPE_NVRAM		2 
#define UV_GAM_RANGE_TYPE_NV_WINDOW	3 
#define UV_GAM_RANGE_TYPE_NV_MAILBOX	4 
#define UV_GAM_RANGE_TYPE_HOLE		5 
#define UV_GAM_RANGE_TYPE_MAX		6


#define UV_GAM_RANGE_SHFT		26		

struct uv_gam_range_entry {
	char	type;		
	char	unused1;
	u16	nasid;		
	u16	sockid;		
	u16	pnode;		
	u32	unused2;
	u32	limit;		
};

#define	UV_AT_SIZE	8	
struct uv_arch_type_entry {
	char	archtype[UV_AT_SIZE];
};

#define	UV_SYSTAB_SIG			"UVST"
#define	UV_SYSTAB_VERSION_1		1	
#define	UV_SYSTAB_VERSION_UV4		0x400	
#define	UV_SYSTAB_VERSION_UV4_1		0x401	
#define	UV_SYSTAB_VERSION_UV4_2		0x402	
#define	UV_SYSTAB_VERSION_UV4_3		0x403	
#define	UV_SYSTAB_VERSION_UV4_LATEST	UV_SYSTAB_VERSION_UV4_3

#define	UV_SYSTAB_VERSION_UV5		0x500	
#define	UV_SYSTAB_VERSION_UV5_LATEST	UV_SYSTAB_VERSION_UV5

#define	UV_SYSTAB_TYPE_UNUSED		0	
#define	UV_SYSTAB_TYPE_GAM_PARAMS	1	
#define	UV_SYSTAB_TYPE_GAM_RNG_TBL	2	
#define	UV_SYSTAB_TYPE_ARCH_TYPE	3	
#define	UV_SYSTAB_TYPE_MAX		4


struct uv_systab {
	char signature[4];	
	u32 revision;		
	u64 (__efiapi *function)(enum uv_bios_cmd, ...);
				
	u32 size;		
	struct {
		u32 type:8;	
		u32 offset:24;	
	} entry[1];		
};
extern struct uv_systab *uv_systab;

#define UV_BIOS_MAXSTRING	      128
struct uv_bios_hub_info {
	unsigned int id;
	union {
		struct {
			unsigned long long this_part:1;
			unsigned long long is_shared:1;
			unsigned long long is_disabled:1;
		} fields;
		struct {
			unsigned long long flags;
			unsigned long long reserved;
		} b;
	} f;
	char name[UV_BIOS_MAXSTRING];
	char location[UV_BIOS_MAXSTRING];
	unsigned int ports;
};

struct uv_bios_port_info {
	unsigned int port;
	unsigned int conn_id;
	unsigned int conn_port;
};



enum {
	BIOS_FREQ_BASE_PLATFORM = 0,
	BIOS_FREQ_BASE_INTERVAL_TIMER = 1,
	BIOS_FREQ_BASE_REALTIME_CLOCK = 2
};

union partition_info_u {
	u64	val;
	struct {
		u64	hub_version	:  8,
			partition_id	: 16,
			coherence_id	: 16,
			region_size	: 24;
	};
};

enum uv_memprotect {
	UV_MEMPROT_RESTRICT_ACCESS,
	UV_MEMPROT_ALLOW_AMO,
	UV_MEMPROT_ALLOW_RW
};

extern s64 uv_bios_get_sn_info(int, int *, long *, long *, long *, long *);
extern s64 uv_bios_freq_base(u64, u64 *);
extern int uv_bios_mq_watchlist_alloc(unsigned long, unsigned int,
					unsigned long *);
extern int uv_bios_mq_watchlist_free(int, int);
extern s64 uv_bios_change_memprotect(u64, u64, enum uv_memprotect);
extern s64 uv_bios_reserved_page_pa(u64, u64 *, u64 *, u64 *);
extern int uv_bios_set_legacy_vga_target(bool decode, int domain, int bus);

extern s64 uv_bios_get_master_nasid(u64 sz, u64 *nasid);
extern s64 uv_bios_get_heapsize(u64 nasid, u64 sz, u64 *heap_sz);
extern s64 uv_bios_install_heap(u64 nasid, u64 sz, u64 *heap);
extern s64 uv_bios_obj_count(u64 nasid, u64 sz, u64 *objcnt);
extern s64 uv_bios_enum_objs(u64 nasid, u64 sz, u64 *objbuf);
extern s64 uv_bios_enum_ports(u64 nasid, u64 obj_id, u64 sz, u64 *portbuf);
extern s64 uv_bios_get_geoinfo(u64 nasid, u64 sz, u64 *geo);
extern s64 uv_bios_get_pci_topology(u64 sz, u64 *buf);

extern int uv_bios_init(void);
extern unsigned long get_uv_systab_phys(bool msg);

extern unsigned long sn_rtc_cycles_per_second;
extern int uv_type;
extern long sn_partition_id;
extern long sn_coherency_id;
extern long sn_region_size;
extern long system_serial_number;
extern ssize_t uv_get_archtype(char *buf, int len);
extern int uv_get_hubless_system(void);

extern struct kobject *sgi_uv_kobj;	


extern struct semaphore __efi_uv_runtime_lock;

#endif 
