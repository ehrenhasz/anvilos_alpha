
 

#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/mnt_idmapping.h>
#include <linux/slab.h>
#include <linux/user_namespace.h>

#include "internal.h"

struct mnt_idmap {
	struct user_namespace *owner;
	refcount_t count;
};

 
struct mnt_idmap nop_mnt_idmap = {
	.owner	= &init_user_ns,
	.count	= REFCOUNT_INIT(1),
};
EXPORT_SYMBOL_GPL(nop_mnt_idmap);

 
bool check_fsmapping(const struct mnt_idmap *idmap,
		     const struct super_block *sb)
{
	return idmap->owner != sb->s_user_ns;
}

 
static inline bool initial_idmapping(const struct user_namespace *ns)
{
	return ns == &init_user_ns;
}

 
static inline bool no_idmapping(const struct user_namespace *mnt_userns,
				const struct user_namespace *fs_userns)
{
	return initial_idmapping(mnt_userns) || mnt_userns == fs_userns;
}

 

vfsuid_t make_vfsuid(struct mnt_idmap *idmap,
				   struct user_namespace *fs_userns,
				   kuid_t kuid)
{
	uid_t uid;
	struct user_namespace *mnt_userns = idmap->owner;

	if (no_idmapping(mnt_userns, fs_userns))
		return VFSUIDT_INIT(kuid);
	if (initial_idmapping(fs_userns))
		uid = __kuid_val(kuid);
	else
		uid = from_kuid(fs_userns, kuid);
	if (uid == (uid_t)-1)
		return INVALID_VFSUID;
	return VFSUIDT_INIT(make_kuid(mnt_userns, uid));
}
EXPORT_SYMBOL_GPL(make_vfsuid);

 
vfsgid_t make_vfsgid(struct mnt_idmap *idmap,
		     struct user_namespace *fs_userns, kgid_t kgid)
{
	gid_t gid;
	struct user_namespace *mnt_userns = idmap->owner;

	if (no_idmapping(mnt_userns, fs_userns))
		return VFSGIDT_INIT(kgid);
	if (initial_idmapping(fs_userns))
		gid = __kgid_val(kgid);
	else
		gid = from_kgid(fs_userns, kgid);
	if (gid == (gid_t)-1)
		return INVALID_VFSGID;
	return VFSGIDT_INIT(make_kgid(mnt_userns, gid));
}
EXPORT_SYMBOL_GPL(make_vfsgid);

 
kuid_t from_vfsuid(struct mnt_idmap *idmap,
		   struct user_namespace *fs_userns, vfsuid_t vfsuid)
{
	uid_t uid;
	struct user_namespace *mnt_userns = idmap->owner;

	if (no_idmapping(mnt_userns, fs_userns))
		return AS_KUIDT(vfsuid);
	uid = from_kuid(mnt_userns, AS_KUIDT(vfsuid));
	if (uid == (uid_t)-1)
		return INVALID_UID;
	if (initial_idmapping(fs_userns))
		return KUIDT_INIT(uid);
	return make_kuid(fs_userns, uid);
}
EXPORT_SYMBOL_GPL(from_vfsuid);

 
kgid_t from_vfsgid(struct mnt_idmap *idmap,
		   struct user_namespace *fs_userns, vfsgid_t vfsgid)
{
	gid_t gid;
	struct user_namespace *mnt_userns = idmap->owner;

	if (no_idmapping(mnt_userns, fs_userns))
		return AS_KGIDT(vfsgid);
	gid = from_kgid(mnt_userns, AS_KGIDT(vfsgid));
	if (gid == (gid_t)-1)
		return INVALID_GID;
	if (initial_idmapping(fs_userns))
		return KGIDT_INIT(gid);
	return make_kgid(fs_userns, gid);
}
EXPORT_SYMBOL_GPL(from_vfsgid);

#ifdef CONFIG_MULTIUSER
 
int vfsgid_in_group_p(vfsgid_t vfsgid)
{
	return in_group_p(AS_KGIDT(vfsgid));
}
#else
int vfsgid_in_group_p(vfsgid_t vfsgid)
{
	return 1;
}
#endif
EXPORT_SYMBOL_GPL(vfsgid_in_group_p);

struct mnt_idmap *alloc_mnt_idmap(struct user_namespace *mnt_userns)
{
	struct mnt_idmap *idmap;

	idmap = kzalloc(sizeof(struct mnt_idmap), GFP_KERNEL_ACCOUNT);
	if (!idmap)
		return ERR_PTR(-ENOMEM);

	idmap->owner = get_user_ns(mnt_userns);
	refcount_set(&idmap->count, 1);
	return idmap;
}

 
struct mnt_idmap *mnt_idmap_get(struct mnt_idmap *idmap)
{
	if (idmap != &nop_mnt_idmap)
		refcount_inc(&idmap->count);

	return idmap;
}

 
void mnt_idmap_put(struct mnt_idmap *idmap)
{
	if (idmap != &nop_mnt_idmap && refcount_dec_and_test(&idmap->count)) {
		put_user_ns(idmap->owner);
		kfree(idmap);
	}
}
