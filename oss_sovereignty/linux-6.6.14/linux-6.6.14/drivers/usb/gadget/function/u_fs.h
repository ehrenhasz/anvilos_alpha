#ifndef U_FFS_H
#define U_FFS_H
#include <linux/usb/composite.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/refcount.h>
#ifdef VERBOSE_DEBUG
#ifndef pr_vdebug
#  define pr_vdebug pr_debug
#endif  
#  define ffs_dump_mem(prefix, ptr, len) \
	print_hex_dump_bytes(pr_fmt(prefix ": "), DUMP_PREFIX_NONE, ptr, len)
#else
#ifndef pr_vdebug
#  define pr_vdebug(...)                 do { } while (0)
#endif  
#  define ffs_dump_mem(prefix, ptr, len) do { } while (0)
#endif  
struct f_fs_opts;
struct ffs_dev {
	struct ffs_data *ffs_data;
	struct f_fs_opts *opts;
	struct list_head entry;
	char name[41];
	bool mounted;
	bool desc_ready;
	bool single;
	int (*ffs_ready_callback)(struct ffs_data *ffs);
	void (*ffs_closed_callback)(struct ffs_data *ffs);
	void *(*ffs_acquire_dev_callback)(struct ffs_dev *dev);
	void (*ffs_release_dev_callback)(struct ffs_dev *dev);
};
extern struct mutex ffs_lock;
static inline void ffs_dev_lock(void)
{
	mutex_lock(&ffs_lock);
}
static inline void ffs_dev_unlock(void)
{
	mutex_unlock(&ffs_lock);
}
int ffs_name_dev(struct ffs_dev *dev, const char *name);
int ffs_single_dev(struct ffs_dev *dev);
struct ffs_epfile;
struct ffs_function;
enum ffs_state {
	FFS_READ_DESCRIPTORS,
	FFS_READ_STRINGS,
	FFS_ACTIVE,
	FFS_DEACTIVATED,
	FFS_CLOSING
};
enum ffs_setup_state {
	FFS_NO_SETUP,
	FFS_SETUP_PENDING,
	FFS_SETUP_CANCELLED
};
struct ffs_data {
	struct usb_gadget		*gadget;
	struct mutex			mutex;
	spinlock_t			eps_lock;
	struct usb_request		*ep0req;		 
	struct completion		ep0req_completion;	 
	refcount_t			ref;
	atomic_t			opened;
	enum ffs_state			state;
	enum ffs_setup_state		setup_state;
	struct {
		u8				types[4];
		unsigned short			count;
		unsigned short			can_stall;
		struct usb_ctrlrequest		setup;
		wait_queue_head_t		waitq;
	} ev;  
	unsigned long			flags;
#define FFS_FL_CALL_CLOSED_CALLBACK 0
#define FFS_FL_BOUND                1
	wait_queue_head_t		wait;
	struct ffs_function		*func;
	const char			*dev_name;
	void				*private_data;
	const void			*raw_descs_data;
	const void			*raw_descs;
	unsigned			raw_descs_length;
	unsigned			fs_descs_count;
	unsigned			hs_descs_count;
	unsigned			ss_descs_count;
	unsigned			ms_os_descs_count;
	unsigned			ms_os_descs_ext_prop_count;
	unsigned			ms_os_descs_ext_prop_name_len;
	unsigned			ms_os_descs_ext_prop_data_len;
	void				*ms_os_descs_ext_prop_avail;
	void				*ms_os_descs_ext_prop_name_avail;
	void				*ms_os_descs_ext_prop_data_avail;
	unsigned			user_flags;
#define FFS_MAX_EPS_COUNT 31
	u8				eps_addrmap[FFS_MAX_EPS_COUNT];
	unsigned short			strings_count;
	unsigned short			interfaces_count;
	unsigned short			eps_count;
	unsigned short			_pad1;
	const void			*raw_strings;
	struct usb_gadget_strings	**stringtabs;
	struct super_block		*sb;
	struct ffs_file_perms {
		umode_t				mode;
		kuid_t				uid;
		kgid_t				gid;
	}				file_perms;
	struct eventfd_ctx *ffs_eventfd;
	struct workqueue_struct *io_completion_wq;
	bool no_disconnect;
	struct work_struct reset_work;
	struct ffs_epfile		*epfiles;
};
struct f_fs_opts {
	struct usb_function_instance	func_inst;
	struct ffs_dev			*dev;
	unsigned			refcnt;
	bool				no_configfs;
};
static inline struct f_fs_opts *to_f_fs_opts(struct usb_function_instance *fi)
{
	return container_of(fi, struct f_fs_opts, func_inst);
}
#endif  
