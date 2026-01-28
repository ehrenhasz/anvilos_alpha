#ifndef _ASM_S390_LOWCORE_H
#define _ASM_S390_LOWCORE_H
#include <linux/types.h>
#include <asm/ptrace.h>
#include <asm/cpu.h>
#include <asm/types.h>
#define LC_ORDER 1
#define LC_PAGES 2
struct pgm_tdb {
	u64 data[32];
};
struct lowcore {
	__u8	pad_0x0000[0x0014-0x0000];	 
	__u32	ipl_parmblock_ptr;		 
	__u8	pad_0x0018[0x0080-0x0018];	 
	__u32	ext_params;			 
	union {
		struct {
			__u16 ext_cpu_addr;	 
			__u16 ext_int_code;	 
		};
		__u32 ext_int_code_addr;
	};
	__u32	svc_int_code;			 
	union {
		struct {
			__u16	pgm_ilc;	 
			__u16	pgm_code;	 
		};
		__u32 pgm_int_code;
	};
	__u32	data_exc_code;			 
	__u16	mon_class_num;			 
	union {
		struct {
			__u8	per_code;	 
			__u8	per_atmid;	 
		};
		__u16 per_code_combined;
	};
	__u64	per_address;			 
	__u8	exc_access_id;			 
	__u8	per_access_id;			 
	__u8	op_access_id;			 
	__u8	ar_mode_id;			 
	__u8	pad_0x00a4[0x00a8-0x00a4];	 
	__u64	trans_exc_code;			 
	__u64	monitor_code;			 
	union {
		struct {
			__u16	subchannel_id;	 
			__u16	subchannel_nr;	 
			__u32	io_int_parm;	 
			__u32	io_int_word;	 
		};
		struct tpi_info	tpi_info;	 
	};
	__u8	pad_0x00c4[0x00c8-0x00c4];	 
	__u32	stfl_fac_list;			 
	__u8	pad_0x00cc[0x00e8-0x00cc];	 
	__u64	mcck_interruption_code;		 
	__u8	pad_0x00f0[0x00f4-0x00f0];	 
	__u32	external_damage_code;		 
	__u64	failing_storage_address;	 
	__u8	pad_0x0100[0x0110-0x0100];	 
	__u64	pgm_last_break;			 
	__u8	pad_0x0118[0x0120-0x0118];	 
	psw_t	restart_old_psw;		 
	psw_t	external_old_psw;		 
	psw_t	svc_old_psw;			 
	psw_t	program_old_psw;		 
	psw_t	mcck_old_psw;			 
	psw_t	io_old_psw;			 
	__u8	pad_0x0180[0x01a0-0x0180];	 
	psw_t	restart_psw;			 
	psw_t	external_new_psw;		 
	psw_t	svc_new_psw;			 
	psw_t	program_new_psw;		 
	psw_t	mcck_new_psw;			 
	psw_t	io_new_psw;			 
	__u64	save_area_sync[8];		 
	__u64	save_area_async[8];		 
	__u64	save_area_restart[1];		 
	__u64	cpu_flags;			 
	psw_t	return_psw;			 
	psw_t	return_mcck_psw;		 
	__u64	last_break;			 
	__u64	sys_enter_timer;		 
	__u64	mcck_enter_timer;		 
	__u64	exit_timer;			 
	__u64	user_timer;			 
	__u64	guest_timer;			 
	__u64	system_timer;			 
	__u64	hardirq_timer;			 
	__u64	softirq_timer;			 
	__u64	steal_timer;			 
	__u64	avg_steal_timer;		 
	__u64	last_update_timer;		 
	__u64	last_update_clock;		 
	__u64	int_clock;			 
	__u8	pad_0x0320[0x0328-0x0320];	 
	__u64	clock_comparator;		 
	__u64	boot_clock[2];			 
	__u64	current_task;			 
	__u64	kernel_stack;			 
	__u64	async_stack;			 
	__u64	nodat_stack;			 
	__u64	restart_stack;			 
	__u64	mcck_stack;			 
	__u64	restart_fn;			 
	__u64	restart_data;			 
	__u32	restart_source;			 
	__u32	restart_flags;			 
	__u64	kernel_asce;			 
	__u64	user_asce;			 
	__u32	lpp;				 
	__u32	current_pid;			 
	__u32	cpu_nr;				 
	__u32	softirq_pending;		 
	__s32	preempt_count;			 
	__u32	spinlock_lockval;		 
	__u32	spinlock_index;			 
	__u32	fpu_flags;			 
	__u64	percpu_offset;			 
	__u8	pad_0x03c0[0x03c8-0x03c0];	 
	__u64	machine_flags;			 
	__u64	gmap;				 
	__u8	pad_0x03d8[0x0400-0x03d8];	 
	__u32	return_lpswe;			 
	__u32	return_mcck_lpswe;		 
	__u8	pad_0x040a[0x0e00-0x0408];	 
	__u64	ipib;				 
	__u32	ipib_checksum;			 
	__u64	vmcore_info;			 
	__u8	pad_0x0e14[0x0e18-0x0e14];	 
	__u64	os_info;			 
	__u8	pad_0x0e20[0x11b0-0x0e20];	 
	__u64	mcesad;				 
	__u64	ext_params2;			 
	__u8	pad_0x11c0[0x1200-0x11C0];	 
	__u64	floating_pt_save_area[16];	 
	__u64	gpregs_save_area[16];		 
	psw_t	psw_save_area;			 
	__u8	pad_0x1310[0x1318-0x1310];	 
	__u32	prefixreg_save_area;		 
	__u32	fpt_creg_save_area;		 
	__u8	pad_0x1320[0x1324-0x1320];	 
	__u32	tod_progreg_save_area;		 
	__u32	cpu_timer_save_area[2];		 
	__u32	clock_comp_save_area[2];	 
	__u64	last_break_save_area;		 
	__u32	access_regs_save_area[16];	 
	__u64	cregs_save_area[16];		 
	__u8	pad_0x1400[0x1500-0x1400];	 
	__u64	ccd;				 
	__u64	aicd;				 
	__u8	pad_0x1510[0x1800-0x1510];	 
	struct pgm_tdb pgm_tdb;			 
	__u8	pad_0x1900[0x2000-0x1900];	 
} __packed __aligned(8192);
#define S390_lowcore (*((struct lowcore *) 0))
extern struct lowcore *lowcore_ptr[];
static inline void set_prefix(__u32 address)
{
	asm volatile("spx %0" : : "Q" (address) : "memory");
}
static inline __u32 store_prefix(void)
{
	__u32 address;
	asm volatile("stpx %0" : "=Q" (address));
	return address;
}
#endif  
