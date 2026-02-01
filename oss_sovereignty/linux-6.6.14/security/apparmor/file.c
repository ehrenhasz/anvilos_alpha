
 

#include <linux/tty.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mount.h>

#include "include/apparmor.h"
#include "include/audit.h"
#include "include/cred.h"
#include "include/file.h"
#include "include/match.h"
#include "include/net.h"
#include "include/path.h"
#include "include/policy.h"
#include "include/label.h"

static u32 map_mask_to_chr_mask(u32 mask)
{
	u32 m = mask & PERMS_CHRS_MASK;

	if (mask & AA_MAY_GETATTR)
		m |= MAY_READ;
	if (mask & (AA_MAY_SETATTR | AA_MAY_CHMOD | AA_MAY_CHOWN))
		m |= MAY_WRITE;

	return m;
}

 
static void file_audit_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;
	struct apparmor_audit_data *ad = aad(sa);
	kuid_t fsuid = ad->subj_cred ? ad->subj_cred->fsuid : current_fsuid();
	char str[10];

	if (ad->request & AA_AUDIT_FILE_MASK) {
		aa_perm_mask_to_str(str, sizeof(str), aa_file_perm_chrs,
				    map_mask_to_chr_mask(ad->request));
		audit_log_format(ab, " requested_mask=\"%s\"", str);
	}
	if (ad->denied & AA_AUDIT_FILE_MASK) {
		aa_perm_mask_to_str(str, sizeof(str), aa_file_perm_chrs,
				    map_mask_to_chr_mask(ad->denied));
		audit_log_format(ab, " denied_mask=\"%s\"", str);
	}
	if (ad->request & AA_AUDIT_FILE_MASK) {
		audit_log_format(ab, " fsuid=%d",
				 from_kuid(&init_user_ns, fsuid));
		audit_log_format(ab, " ouid=%d",
				 from_kuid(&init_user_ns, ad->fs.ouid));
	}

	if (ad->peer) {
		audit_log_format(ab, " target=");
		aa_label_xaudit(ab, labels_ns(ad->subj_label), ad->peer,
				FLAG_VIEW_SUBNS, GFP_KERNEL);
	} else if (ad->fs.target) {
		audit_log_format(ab, " target=");
		audit_log_untrustedstring(ab, ad->fs.target);
	}
}

 
int aa_audit_file(const struct cred *subj_cred,
		  struct aa_profile *profile, struct aa_perms *perms,
		  const char *op, u32 request, const char *name,
		  const char *target, struct aa_label *tlabel,
		  kuid_t ouid, const char *info, int error)
{
	int type = AUDIT_APPARMOR_AUTO;
	DEFINE_AUDIT_DATA(ad, LSM_AUDIT_DATA_TASK, AA_CLASS_FILE, op);

	ad.subj_cred = subj_cred;
	ad.request = request;
	ad.name = name;
	ad.fs.target = target;
	ad.peer = tlabel;
	ad.fs.ouid = ouid;
	ad.info = info;
	ad.error = error;
	ad.common.u.tsk = NULL;

	if (likely(!ad.error)) {
		u32 mask = perms->audit;

		if (unlikely(AUDIT_MODE(profile) == AUDIT_ALL))
			mask = 0xffff;

		 
		ad.request &= mask;

		if (likely(!ad.request))
			return 0;
		type = AUDIT_APPARMOR_AUDIT;
	} else {
		 
		ad.request = ad.request & ~perms->allow;
		AA_BUG(!ad.request);

		if (ad.request & perms->kill)
			type = AUDIT_APPARMOR_KILL;

		 
		if ((ad.request & perms->quiet) &&
		    AUDIT_MODE(profile) != AUDIT_NOQUIET &&
		    AUDIT_MODE(profile) != AUDIT_ALL)
			ad.request &= ~perms->quiet;

		if (!ad.request)
			return ad.error;
	}

	ad.denied = ad.request & ~perms->allow;
	return aa_audit(type, profile, &ad, file_audit_cb);
}

 
static inline bool is_deleted(struct dentry *dentry)
{
	if (d_unlinked(dentry) && d_backing_inode(dentry)->i_nlink == 0)
		return true;
	return false;
}

static int path_name(const char *op, const struct cred *subj_cred,
		     struct aa_label *label,
		     const struct path *path, int flags, char *buffer,
		     const char **name, struct path_cond *cond, u32 request)
{
	struct aa_profile *profile;
	const char *info = NULL;
	int error;

	error = aa_path_name(path, flags, buffer, name, &info,
			     labels_profile(label)->disconnected);
	if (error) {
		fn_for_each_confined(label, profile,
			aa_audit_file(subj_cred,
				      profile, &nullperms, op, request, *name,
				      NULL, NULL, cond->uid, info, error));
		return error;
	}

	return 0;
}

struct aa_perms default_perms = {};
 
struct aa_perms *aa_lookup_fperms(struct aa_policydb *file_rules,
				 aa_state_t state, struct path_cond *cond)
{
	unsigned int index = ACCEPT_TABLE(file_rules->dfa)[state];

	if (!(file_rules->perms))
		return &default_perms;

	if (uid_eq(current_fsuid(), cond->uid))
		return &(file_rules->perms[index]);

	return &(file_rules->perms[index + 1]);
}

 
aa_state_t aa_str_perms(struct aa_policydb *file_rules, aa_state_t start,
			const char *name, struct path_cond *cond,
			struct aa_perms *perms)
{
	aa_state_t state;
	state = aa_dfa_match(file_rules->dfa, start, name);
	*perms = *(aa_lookup_fperms(file_rules, state, cond));

	return state;
}

static int __aa_path_perm(const char *op, const struct cred *subj_cred,
			  struct aa_profile *profile, const char *name,
			  u32 request, struct path_cond *cond, int flags,
			  struct aa_perms *perms)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	int e = 0;

	if (profile_unconfined(profile))
		return 0;
	aa_str_perms(&(rules->file), rules->file.start[AA_CLASS_FILE],
		     name, cond, perms);
	if (request & ~perms->allow)
		e = -EACCES;
	return aa_audit_file(subj_cred,
			     profile, perms, op, request, name, NULL, NULL,
			     cond->uid, NULL, e);
}


static int profile_path_perm(const char *op, const struct cred *subj_cred,
			     struct aa_profile *profile,
			     const struct path *path, char *buffer, u32 request,
			     struct path_cond *cond, int flags,
			     struct aa_perms *perms)
{
	const char *name;
	int error;

	if (profile_unconfined(profile))
		return 0;

	error = path_name(op, subj_cred, &profile->label, path,
			  flags | profile->path_flags, buffer, &name, cond,
			  request);
	if (error)
		return error;
	return __aa_path_perm(op, subj_cred, profile, name, request, cond,
			      flags, perms);
}

 
int aa_path_perm(const char *op, const struct cred *subj_cred,
		 struct aa_label *label,
		 const struct path *path, int flags, u32 request,
		 struct path_cond *cond)
{
	struct aa_perms perms = {};
	struct aa_profile *profile;
	char *buffer = NULL;
	int error;

	flags |= PATH_DELEGATE_DELETED | (S_ISDIR(cond->mode) ? PATH_IS_DIR :
								0);
	buffer = aa_get_buffer(false);
	if (!buffer)
		return -ENOMEM;
	error = fn_for_each_confined(label, profile,
			profile_path_perm(op, subj_cred, profile, path, buffer,
					  request, cond, flags, &perms));

	aa_put_buffer(buffer);

	return error;
}

 
static inline bool xindex_is_subset(u32 link, u32 target)
{
	if (((link & ~AA_X_UNSAFE) != (target & ~AA_X_UNSAFE)) ||
	    ((link & AA_X_UNSAFE) && !(target & AA_X_UNSAFE)))
		return false;

	return true;
}

static int profile_path_link(const struct cred *subj_cred,
			     struct aa_profile *profile,
			     const struct path *link, char *buffer,
			     const struct path *target, char *buffer2,
			     struct path_cond *cond)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	const char *lname, *tname = NULL;
	struct aa_perms lperms = {}, perms;
	const char *info = NULL;
	u32 request = AA_MAY_LINK;
	aa_state_t state;
	int error;

	error = path_name(OP_LINK, subj_cred, &profile->label, link,
			  profile->path_flags,
			  buffer, &lname, cond, AA_MAY_LINK);
	if (error)
		goto audit;

	 
	error = path_name(OP_LINK, subj_cred, &profile->label, target,
			  profile->path_flags,
			  buffer2, &tname, cond, AA_MAY_LINK);
	if (error)
		goto audit;

	error = -EACCES;
	 
	state = aa_str_perms(&(rules->file),
			     rules->file.start[AA_CLASS_FILE], lname,
			     cond, &lperms);

	if (!(lperms.allow & AA_MAY_LINK))
		goto audit;

	 
	state = aa_dfa_null_transition(rules->file.dfa, state);
	aa_str_perms(&(rules->file), state, tname, cond, &perms);

	 
	lperms.audit = perms.audit;
	lperms.quiet = perms.quiet;
	lperms.kill = perms.kill;

	if (!(perms.allow & AA_MAY_LINK)) {
		info = "target restricted";
		lperms = perms;
		goto audit;
	}

	 
	if (!(perms.allow & AA_LINK_SUBSET))
		goto done_tests;

	 
	aa_str_perms(&(rules->file), rules->file.start[AA_CLASS_FILE],
		     tname, cond, &perms);

	 
	request = lperms.allow & ~AA_MAY_LINK;
	lperms.allow &= perms.allow | AA_MAY_LINK;

	request |= AA_AUDIT_FILE_MASK & (lperms.allow & ~perms.allow);
	if (request & ~lperms.allow) {
		goto audit;
	} else if ((lperms.allow & MAY_EXEC) &&
		   !xindex_is_subset(lperms.xindex, perms.xindex)) {
		lperms.allow &= ~MAY_EXEC;
		request |= MAY_EXEC;
		info = "link not subset of target";
		goto audit;
	}

done_tests:
	error = 0;

audit:
	return aa_audit_file(subj_cred,
			     profile, &lperms, OP_LINK, request, lname, tname,
			     NULL, cond->uid, info, error);
}

 
int aa_path_link(const struct cred *subj_cred,
		 struct aa_label *label, struct dentry *old_dentry,
		 const struct path *new_dir, struct dentry *new_dentry)
{
	struct path link = { .mnt = new_dir->mnt, .dentry = new_dentry };
	struct path target = { .mnt = new_dir->mnt, .dentry = old_dentry };
	struct path_cond cond = {
		d_backing_inode(old_dentry)->i_uid,
		d_backing_inode(old_dentry)->i_mode
	};
	char *buffer = NULL, *buffer2 = NULL;
	struct aa_profile *profile;
	int error;

	 
	buffer = aa_get_buffer(false);
	buffer2 = aa_get_buffer(false);
	error = -ENOMEM;
	if (!buffer || !buffer2)
		goto out;

	error = fn_for_each_confined(label, profile,
			profile_path_link(subj_cred, profile, &link, buffer,
					  &target, buffer2, &cond));
out:
	aa_put_buffer(buffer);
	aa_put_buffer(buffer2);
	return error;
}

static void update_file_ctx(struct aa_file_ctx *fctx, struct aa_label *label,
			    u32 request)
{
	struct aa_label *l, *old;

	 
	spin_lock(&fctx->lock);
	old = rcu_dereference_protected(fctx->label,
					lockdep_is_held(&fctx->lock));
	l = aa_label_merge(old, label, GFP_ATOMIC);
	if (l) {
		if (l != old) {
			rcu_assign_pointer(fctx->label, l);
			aa_put_label(old);
		} else
			aa_put_label(l);
		fctx->allow |= request;
	}
	spin_unlock(&fctx->lock);
}

static int __file_path_perm(const char *op, const struct cred *subj_cred,
			    struct aa_label *label,
			    struct aa_label *flabel, struct file *file,
			    u32 request, u32 denied, bool in_atomic)
{
	struct aa_profile *profile;
	struct aa_perms perms = {};
	vfsuid_t vfsuid = i_uid_into_vfsuid(file_mnt_idmap(file),
					    file_inode(file));
	struct path_cond cond = {
		.uid = vfsuid_into_kuid(vfsuid),
		.mode = file_inode(file)->i_mode
	};
	char *buffer;
	int flags, error;

	 
	if (!denied && aa_label_is_subset(flabel, label))
		 
		return 0;

	flags = PATH_DELEGATE_DELETED | (S_ISDIR(cond.mode) ? PATH_IS_DIR : 0);
	buffer = aa_get_buffer(in_atomic);
	if (!buffer)
		return -ENOMEM;

	 
	error = fn_for_each_not_in_set(flabel, label, profile,
			profile_path_perm(op, subj_cred, profile,
					  &file->f_path, buffer,
					  request, &cond, flags, &perms));
	if (denied && !error) {
		 
		if (label == flabel)
			error = fn_for_each(label, profile,
				profile_path_perm(op, subj_cred,
						  profile, &file->f_path,
						  buffer, request, &cond, flags,
						  &perms));
		else
			error = fn_for_each_not_in_set(label, flabel, profile,
				profile_path_perm(op, subj_cred,
						  profile, &file->f_path,
						  buffer, request, &cond, flags,
						  &perms));
	}
	if (!error)
		update_file_ctx(file_ctx(file), label, request);

	aa_put_buffer(buffer);

	return error;
}

static int __file_sock_perm(const char *op, const struct cred *subj_cred,
			    struct aa_label *label,
			    struct aa_label *flabel, struct file *file,
			    u32 request, u32 denied)
{
	struct socket *sock = (struct socket *) file->private_data;
	int error;

	AA_BUG(!sock);

	 
	if (!denied && aa_label_is_subset(flabel, label))
		return 0;

	 
	error = aa_sock_file_perm(subj_cred, label, op, request, sock);
	if (denied) {
		 
		 
		last_error(error, aa_sock_file_perm(subj_cred, flabel, op,
						    request, sock));
	}
	if (!error)
		update_file_ctx(file_ctx(file), label, request);

	return error;
}

 
int aa_file_perm(const char *op, const struct cred *subj_cred,
		 struct aa_label *label, struct file *file,
		 u32 request, bool in_atomic)
{
	struct aa_file_ctx *fctx;
	struct aa_label *flabel;
	u32 denied;
	int error = 0;

	AA_BUG(!label);
	AA_BUG(!file);

	fctx = file_ctx(file);

	rcu_read_lock();
	flabel  = rcu_dereference(fctx->label);
	AA_BUG(!flabel);

	 
	denied = request & ~fctx->allow;
	if (unconfined(label) || unconfined(flabel) ||
	    (!denied && aa_label_is_subset(flabel, label))) {
		rcu_read_unlock();
		goto done;
	}

	flabel  = aa_get_newest_label(flabel);
	rcu_read_unlock();
	 

	if (file->f_path.mnt && path_mediated_fs(file->f_path.dentry))
		error = __file_path_perm(op, subj_cred, label, flabel, file,
					 request, denied, in_atomic);

	else if (S_ISSOCK(file_inode(file)->i_mode))
		error = __file_sock_perm(op, subj_cred, label, flabel, file,
					 request, denied);
	aa_put_label(flabel);

done:
	return error;
}

static void revalidate_tty(const struct cred *subj_cred, struct aa_label *label)
{
	struct tty_struct *tty;
	int drop_tty = 0;

	tty = get_current_tty();
	if (!tty)
		return;

	spin_lock(&tty->files_lock);
	if (!list_empty(&tty->tty_files)) {
		struct tty_file_private *file_priv;
		struct file *file;
		 
		file_priv = list_first_entry(&tty->tty_files,
					     struct tty_file_private, list);
		file = file_priv->file;

		if (aa_file_perm(OP_INHERIT, subj_cred, label, file,
				 MAY_READ | MAY_WRITE, IN_ATOMIC))
			drop_tty = 1;
	}
	spin_unlock(&tty->files_lock);
	tty_kref_put(tty);

	if (drop_tty)
		no_tty();
}

struct cred_label {
	const struct cred *cred;
	struct aa_label *label;
};

static int match_file(const void *p, struct file *file, unsigned int fd)
{
	struct cred_label *cl = (struct cred_label *)p;

	if (aa_file_perm(OP_INHERIT, cl->cred, cl->label, file,
			 aa_map_file_to_perms(file), IN_ATOMIC))
		return fd + 1;
	return 0;
}


 
void aa_inherit_files(const struct cred *cred, struct files_struct *files)
{
	struct aa_label *label = aa_get_newest_cred_label(cred);
	struct cred_label cl = {
		.cred = cred,
		.label = label,
	};
	struct file *devnull = NULL;
	unsigned int n;

	revalidate_tty(cred, label);

	 
	n = iterate_fd(files, 0, match_file, &cl);
	if (!n)  
		goto out;

	devnull = dentry_open(&aa_null, O_RDWR, cred);
	if (IS_ERR(devnull))
		devnull = NULL;
	 
	do {
		replace_fd(n - 1, devnull, 0);
	} while ((n = iterate_fd(files, n, match_file, &cl)) != 0);
	if (devnull)
		fput(devnull);
out:
	aa_put_label(label);
}
