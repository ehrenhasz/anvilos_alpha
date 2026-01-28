#ifndef _ASM_VPE_H
#define _ASM_VPE_H
#include <linux/init.h>
#include <linux/list.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#define VPE_MODULE_NAME "vpe"
#define VPE_MODULE_MINOR 1
#ifdef CONFIG_MIPS_VPE_LOADER_TOM
#define P_SIZE (2 * 1024 * 1024)
#else
#define P_SIZE (256 * 1024)
#endif
#define MAX_VPES 16
static inline int aprp_cpu_index(void)
{
	extern int tclimit;
	return tclimit;
}
enum vpe_state {
	VPE_STATE_UNUSED = 0,
	VPE_STATE_INUSE,
	VPE_STATE_RUNNING
};
enum tc_state {
	TC_STATE_UNUSED = 0,
	TC_STATE_INUSE,
	TC_STATE_RUNNING,
	TC_STATE_DYNAMIC
};
struct vpe {
	enum vpe_state state;
	int minor;
	void *load_addr;
	unsigned long len;
	char *pbuffer;
	unsigned long plen;
	unsigned long __start;
	struct list_head tc;
	struct list_head list;
	void *shared_ptr;
	struct list_head notify;
	unsigned int ntcs;
};
struct tc {
	enum tc_state state;
	int index;
	struct vpe *pvpe;	 
	struct list_head tc;	 
	struct list_head list;	 
};
struct vpe_notifications {
	void (*start)(int vpe);
	void (*stop)(int vpe);
	struct list_head list;
};
struct vpe_control {
	spinlock_t vpe_list_lock;
	struct list_head vpe_list;       
	spinlock_t tc_list_lock;
	struct list_head tc_list;        
};
extern struct vpe_control vpecontrol;
extern const struct file_operations vpe_fops;
int vpe_notify(int index, struct vpe_notifications *notify);
void *vpe_get_shared(int index);
struct vpe *get_vpe(int minor);
struct tc *get_tc(int index);
struct vpe *alloc_vpe(int minor);
struct tc *alloc_tc(int index);
void release_vpe(struct vpe *v);
void *alloc_progmem(unsigned long len);
void release_progmem(void *ptr);
int vpe_run(struct vpe *v);
void cleanup_tc(struct tc *tc);
int __init vpe_module_init(void);
void __exit vpe_module_exit(void);
#endif  
