#ifndef __AA_FILE_H
#define __AA_FILE_H
#include <linux/spinlock.h>
#include "domain.h"
#include "match.h"
#include "perms.h"
struct aa_policydb;
struct aa_profile;
struct path;
#define mask_mode_t(X) (X & (MAY_EXEC | MAY_WRITE | MAY_READ | MAY_APPEND))
#define AA_AUDIT_FILE_MASK	(MAY_READ | MAY_WRITE | MAY_EXEC | MAY_APPEND |\
				 AA_MAY_CREATE | AA_MAY_DELETE |	\
				 AA_MAY_GETATTR | AA_MAY_SETATTR | \
				 AA_MAY_CHMOD | AA_MAY_CHOWN | AA_MAY_LOCK | \
				 AA_EXEC_MMAP | AA_MAY_LINK)
static inline struct aa_file_ctx *file_ctx(struct file *file)
{
	return file->f_security + apparmor_blob_sizes.lbs_file;
}
struct aa_file_ctx {
	spinlock_t lock;
	struct aa_label __rcu *label;
	u32 allow;
};
static inline struct aa_file_ctx *aa_alloc_file_ctx(struct aa_label *label,
						    gfp_t gfp)
{
	struct aa_file_ctx *ctx;
	ctx = kzalloc(sizeof(struct aa_file_ctx), gfp);
	if (ctx) {
		spin_lock_init(&ctx->lock);
		rcu_assign_pointer(ctx->label, aa_get_label(label));
	}
	return ctx;
}
static inline void aa_free_file_ctx(struct aa_file_ctx *ctx)
{
	if (ctx) {
		aa_put_label(rcu_access_pointer(ctx->label));
		kfree_sensitive(ctx);
	}
}
static inline struct aa_label *aa_get_file_label(struct aa_file_ctx *ctx)
{
	return aa_get_label_rcu(&ctx->label);
}
#define AA_X_INDEX_MASK		AA_INDEX_MASK
#define AA_X_TYPE_MASK		0x0c000000
#define AA_X_NONE		AA_INDEX_NONE
#define AA_X_NAME		0x04000000  
#define AA_X_TABLE		0x08000000  
#define AA_X_UNSAFE		0x10000000
#define AA_X_CHILD		0x20000000
#define AA_X_INHERIT		0x40000000
#define AA_X_UNCONFINED		0x80000000
struct path_cond {
	kuid_t uid;
	umode_t mode;
};
#define COMBINED_PERM_MASK(X) ((X).allow | (X).audit | (X).quiet | (X).kill)
int aa_audit_file(const struct cred *cred,
		  struct aa_profile *profile, struct aa_perms *perms,
		  const char *op, u32 request, const char *name,
		  const char *target, struct aa_label *tlabel, kuid_t ouid,
		  const char *info, int error);
struct aa_perms *aa_lookup_fperms(struct aa_policydb *file_rules,
				  aa_state_t state, struct path_cond *cond);
aa_state_t aa_str_perms(struct aa_policydb *file_rules, aa_state_t start,
			const char *name, struct path_cond *cond,
			struct aa_perms *perms);
int aa_path_perm(const char *op, const struct cred *subj_cred,
		 struct aa_label *label, const struct path *path,
		 int flags, u32 request, struct path_cond *cond);
int aa_path_link(const struct cred *subj_cred, struct aa_label *label,
		 struct dentry *old_dentry, const struct path *new_dir,
		 struct dentry *new_dentry);
int aa_file_perm(const char *op, const struct cred *subj_cred,
		 struct aa_label *label, struct file *file,
		 u32 request, bool in_atomic);
void aa_inherit_files(const struct cred *cred, struct files_struct *files);
static inline u32 aa_map_file_to_perms(struct file *file)
{
	int flags = file->f_flags;
	u32 perms = 0;
	if (file->f_mode & FMODE_WRITE)
		perms |= MAY_WRITE;
	if (file->f_mode & FMODE_READ)
		perms |= MAY_READ;
	if ((flags & O_APPEND) && (perms & MAY_WRITE))
		perms = (perms & ~MAY_WRITE) | MAY_APPEND;
	if (flags & O_TRUNC)
		perms |= MAY_WRITE;
	if (flags & O_CREAT)
		perms |= AA_MAY_CREATE;
	return perms;
}
#endif  
