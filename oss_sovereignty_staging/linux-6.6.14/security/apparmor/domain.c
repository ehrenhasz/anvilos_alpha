
 

#include <linux/errno.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mount.h>
#include <linux/syscalls.h>
#include <linux/personality.h>
#include <linux/xattr.h>
#include <linux/user_namespace.h>

#include "include/audit.h"
#include "include/apparmorfs.h"
#include "include/cred.h"
#include "include/domain.h"
#include "include/file.h"
#include "include/ipc.h"
#include "include/match.h"
#include "include/path.h"
#include "include/policy.h"
#include "include/policy_ns.h"

 
static int may_change_ptraced_domain(const struct cred *to_cred,
				     struct aa_label *to_label,
				     const char **info)
{
	struct task_struct *tracer;
	struct aa_label *tracerl = NULL;
	const struct cred *tracer_cred = NULL;

	int error = 0;

	rcu_read_lock();
	tracer = ptrace_parent(current);
	if (tracer) {
		 
		tracerl = aa_get_task_label(tracer);
		tracer_cred = get_task_cred(tracer);
	}
	 
	if (!tracer || unconfined(tracerl))
		goto out;

	error = aa_may_ptrace(tracer_cred, tracerl, to_cred, to_label,
			      PTRACE_MODE_ATTACH);

out:
	rcu_read_unlock();
	aa_put_label(tracerl);
	put_cred(tracer_cred);

	if (error)
		*info = "ptrace prevents transition";
	return error;
}

 
 
static inline aa_state_t match_component(struct aa_profile *profile,
					 struct aa_profile *tp,
					 bool stack, aa_state_t state)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	const char *ns_name;

	if (stack)
		state = aa_dfa_match(rules->file.dfa, state, "&");
	if (profile->ns == tp->ns)
		return aa_dfa_match(rules->file.dfa, state, tp->base.hname);

	 
	ns_name = aa_ns_name(profile->ns, tp->ns, true);
	state = aa_dfa_match_len(rules->file.dfa, state, ":", 1);
	state = aa_dfa_match(rules->file.dfa, state, ns_name);
	state = aa_dfa_match_len(rules->file.dfa, state, ":", 1);
	return aa_dfa_match(rules->file.dfa, state, tp->base.hname);
}

 
static int label_compound_match(struct aa_profile *profile,
				struct aa_label *label, bool stack,
				aa_state_t state, bool subns, u32 request,
				struct aa_perms *perms)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	struct aa_profile *tp;
	struct label_it i;
	struct path_cond cond = { };

	 
	label_for_each(i, label, tp) {
		if (!aa_ns_visible(profile->ns, tp->ns, subns))
			continue;
		state = match_component(profile, tp, stack, state);
		if (!state)
			goto fail;
		goto next;
	}

	 
	*perms = allperms;
	return 0;

next:
	label_for_each_cont(i, label, tp) {
		if (!aa_ns_visible(profile->ns, tp->ns, subns))
			continue;
		state = aa_dfa_match(rules->file.dfa, state, "//&");
		state = match_component(profile, tp, false, state);
		if (!state)
			goto fail;
	}
	*perms = *(aa_lookup_fperms(&(rules->file), state, &cond));
	aa_apply_modes_to_perms(profile, perms);
	if ((perms->allow & request) != request)
		return -EACCES;

	return 0;

fail:
	*perms = nullperms;
	return -EACCES;
}

 
static int label_components_match(struct aa_profile *profile,
				  struct aa_label *label, bool stack,
				  aa_state_t start, bool subns, u32 request,
				  struct aa_perms *perms)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	struct aa_profile *tp;
	struct label_it i;
	struct aa_perms tmp;
	struct path_cond cond = { };
	aa_state_t state = 0;

	 
	label_for_each(i, label, tp) {
		if (!aa_ns_visible(profile->ns, tp->ns, subns))
			continue;
		state = match_component(profile, tp, stack, start);
		if (!state)
			goto fail;
		goto next;
	}

	 
	return 0;

next:
	tmp = *(aa_lookup_fperms(&(rules->file), state, &cond));
	aa_apply_modes_to_perms(profile, &tmp);
	aa_perms_accum(perms, &tmp);
	label_for_each_cont(i, label, tp) {
		if (!aa_ns_visible(profile->ns, tp->ns, subns))
			continue;
		state = match_component(profile, tp, stack, start);
		if (!state)
			goto fail;
		tmp = *(aa_lookup_fperms(&(rules->file), state, &cond));
		aa_apply_modes_to_perms(profile, &tmp);
		aa_perms_accum(perms, &tmp);
	}

	if ((perms->allow & request) != request)
		return -EACCES;

	return 0;

fail:
	*perms = nullperms;
	return -EACCES;
}

 
static int label_match(struct aa_profile *profile, struct aa_label *label,
		       bool stack, aa_state_t state, bool subns, u32 request,
		       struct aa_perms *perms)
{
	int error;

	*perms = nullperms;
	error = label_compound_match(profile, label, stack, state, subns,
				     request, perms);
	if (!error)
		return error;

	*perms = allperms;
	return label_components_match(profile, label, stack, state, subns,
				      request, perms);
}

 

 
static int change_profile_perms(struct aa_profile *profile,
				struct aa_label *target, bool stack,
				u32 request, aa_state_t start,
				struct aa_perms *perms)
{
	if (profile_unconfined(profile)) {
		perms->allow = AA_MAY_CHANGE_PROFILE | AA_MAY_ONEXEC;
		perms->audit = perms->quiet = perms->kill = 0;
		return 0;
	}

	 
	return label_match(profile, target, stack, start, true, request, perms);
}

 
static int aa_xattrs_match(const struct linux_binprm *bprm,
			   struct aa_profile *profile, aa_state_t state)
{
	int i;
	struct dentry *d;
	char *value = NULL;
	struct aa_attachment *attach = &profile->attach;
	int size, value_size = 0, ret = attach->xattr_count;

	if (!bprm || !attach->xattr_count)
		return 0;
	might_sleep();

	 
	state = aa_dfa_outofband_transition(attach->xmatch.dfa, state);
	d = bprm->file->f_path.dentry;

	for (i = 0; i < attach->xattr_count; i++) {
		size = vfs_getxattr_alloc(&nop_mnt_idmap, d, attach->xattrs[i],
					  &value, value_size, GFP_KERNEL);
		if (size >= 0) {
			u32 index, perm;

			 
			state = aa_dfa_null_transition(attach->xmatch.dfa,
						       state);
			 
			state = aa_dfa_match_len(attach->xmatch.dfa, state,
						 value, size);
			index = ACCEPT_TABLE(attach->xmatch.dfa)[state];
			perm = attach->xmatch.perms[index].allow;
			if (!(perm & MAY_EXEC)) {
				ret = -EINVAL;
				goto out;
			}
		}
		 
		state = aa_dfa_outofband_transition(attach->xmatch.dfa, state);
		if (size < 0) {
			 
			if (!state) {
				ret = -EINVAL;
				goto out;
			}
			 
			ret--;
		}
	}

out:
	kfree(value);
	return ret;
}

 
static struct aa_label *find_attach(const struct linux_binprm *bprm,
				    struct aa_ns *ns, struct list_head *head,
				    const char *name, const char **info)
{
	int candidate_len = 0, candidate_xattrs = 0;
	bool conflict = false;
	struct aa_profile *profile, *candidate = NULL;

	AA_BUG(!name);
	AA_BUG(!head);

	rcu_read_lock();
restart:
	list_for_each_entry_rcu(profile, head, base.list) {
		struct aa_attachment *attach = &profile->attach;

		if (profile->label.flags & FLAG_NULL &&
		    &profile->label == ns_unconfined(profile->ns))
			continue;

		 
		if (attach->xmatch.dfa) {
			unsigned int count;
			aa_state_t state;
			u32 index, perm;

			state = aa_dfa_leftmatch(attach->xmatch.dfa,
					attach->xmatch.start[AA_CLASS_XMATCH],
					name, &count);
			index = ACCEPT_TABLE(attach->xmatch.dfa)[state];
			perm = attach->xmatch.perms[index].allow;
			 
			if (perm & MAY_EXEC) {
				int ret = 0;

				if (count < candidate_len)
					continue;

				if (bprm && attach->xattr_count) {
					long rev = READ_ONCE(ns->revision);

					if (!aa_get_profile_not0(profile))
						goto restart;
					rcu_read_unlock();
					ret = aa_xattrs_match(bprm, profile,
							      state);
					rcu_read_lock();
					aa_put_profile(profile);
					if (rev !=
					    READ_ONCE(ns->revision))
						 
						goto restart;
					 
					if (ret < 0)
						continue;
				}
				 
				if (count == candidate_len &&
				    ret <= candidate_xattrs) {
					 
					if (ret == candidate_xattrs)
						conflict = true;
					continue;
				}

				 
				candidate = profile;
				candidate_len = max(count, attach->xmatch_len);
				candidate_xattrs = ret;
				conflict = false;
			}
		} else if (!strcmp(profile->base.name, name)) {
			 
			candidate = profile;
			goto out;
		}
	}

	if (!candidate || conflict) {
		if (conflict)
			*info = "conflicting profile attachments";
		rcu_read_unlock();
		return NULL;
	}

out:
	candidate = aa_get_newest_profile(candidate);
	rcu_read_unlock();

	return &candidate->label;
}

static const char *next_name(int xtype, const char *name)
{
	return NULL;
}

 
struct aa_label *x_table_lookup(struct aa_profile *profile, u32 xindex,
				const char **name)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	struct aa_label *label = NULL;
	u32 xtype = xindex & AA_X_TYPE_MASK;
	int index = xindex & AA_X_INDEX_MASK;

	AA_BUG(!name);

	 
	 
	for (*name = rules->file.trans.table[index]; !label && *name;
	     *name = next_name(xtype, *name)) {
		if (xindex & AA_X_CHILD) {
			struct aa_profile *new_profile;
			 
			new_profile = aa_find_child(profile, *name);
			if (new_profile)
				label = &new_profile->label;
			continue;
		}
		label = aa_label_parse(&profile->label, *name, GFP_KERNEL,
				       true, false);
		if (IS_ERR(label))
			label = NULL;
	}

	 

	return label;
}

 
static struct aa_label *x_to_label(struct aa_profile *profile,
				   const struct linux_binprm *bprm,
				   const char *name, u32 xindex,
				   const char **lookupname,
				   const char **info)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	struct aa_label *new = NULL;
	struct aa_ns *ns = profile->ns;
	u32 xtype = xindex & AA_X_TYPE_MASK;
	const char *stack = NULL;

	switch (xtype) {
	case AA_X_NONE:
		 
		*lookupname = NULL;
		break;
	case AA_X_TABLE:
		 
		stack = rules->file.trans.table[xindex & AA_X_INDEX_MASK];
		if (*stack != '&') {
			 
			new = x_table_lookup(profile, xindex, lookupname);
			stack = NULL;
			break;
		}
		fallthrough;	 
	case AA_X_NAME:
		if (xindex & AA_X_CHILD)
			 
			new = find_attach(bprm, ns, &profile->base.profiles,
					  name, info);
		else
			 
			new = find_attach(bprm, ns, &ns->base.profiles,
					  name, info);
		*lookupname = name;
		break;
	}

	if (!new) {
		if (xindex & AA_X_INHERIT) {
			 
			*info = "ix fallback";
			 
			new = aa_get_newest_label(&profile->label);
		} else if (xindex & AA_X_UNCONFINED) {
			new = aa_get_newest_label(ns_unconfined(profile->ns));
			*info = "ux fallback";
		}
	}

	if (new && stack) {
		 
		struct aa_label *base = new;

		new = aa_label_parse(base, stack, GFP_KERNEL, true, false);
		if (IS_ERR(new))
			new = NULL;
		aa_put_label(base);
	}

	 
	return new;
}

static struct aa_label *profile_transition(const struct cred *subj_cred,
					   struct aa_profile *profile,
					   const struct linux_binprm *bprm,
					   char *buffer, struct path_cond *cond,
					   bool *secure_exec)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	struct aa_label *new = NULL;
	const char *info = NULL, *name = NULL, *target = NULL;
	aa_state_t state = rules->file.start[AA_CLASS_FILE];
	struct aa_perms perms = {};
	bool nonewprivs = false;
	int error = 0;

	AA_BUG(!profile);
	AA_BUG(!bprm);
	AA_BUG(!buffer);

	error = aa_path_name(&bprm->file->f_path, profile->path_flags, buffer,
			     &name, &info, profile->disconnected);
	if (error) {
		if (profile_unconfined(profile) ||
		    (profile->label.flags & FLAG_IX_ON_NAME_ERROR)) {
			AA_DEBUG("name lookup ix on error");
			error = 0;
			new = aa_get_newest_label(&profile->label);
		}
		name = bprm->filename;
		goto audit;
	}

	if (profile_unconfined(profile)) {
		new = find_attach(bprm, profile->ns,
				  &profile->ns->base.profiles, name, &info);
		if (new) {
			AA_DEBUG("unconfined attached to new label");
			return new;
		}
		AA_DEBUG("unconfined exec no attachment");
		return aa_get_newest_label(&profile->label);
	}

	 
	state = aa_str_perms(&(rules->file), state, name, cond, &perms);
	if (perms.allow & MAY_EXEC) {
		 
		new = x_to_label(profile, bprm, name, perms.xindex, &target,
				 &info);
		if (new && new->proxy == profile->label.proxy && info) {
			 
			goto audit;
		} else if (!new) {
			error = -EACCES;
			info = "profile transition not found";
			 
			perms.allow &= ~MAY_EXEC;
		}
	} else if (COMPLAIN_MODE(profile)) {
		 
		struct aa_profile *new_profile = NULL;

		new_profile = aa_new_learning_profile(profile, false, name,
						      GFP_KERNEL);
		if (!new_profile) {
			error = -ENOMEM;
			info = "could not create null profile";
		} else {
			error = -EACCES;
			new = &new_profile->label;
		}
		perms.xindex |= AA_X_UNSAFE;
	} else
		 
		error = -EACCES;

	if (!new)
		goto audit;


	if (!(perms.xindex & AA_X_UNSAFE)) {
		if (DEBUG_ON) {
			dbg_printk("apparmor: scrubbing environment variables"
				   " for %s profile=", name);
			aa_label_printk(new, GFP_KERNEL);
			dbg_printk("\n");
		}
		*secure_exec = true;
	}

audit:
	aa_audit_file(subj_cred, profile, &perms, OP_EXEC, MAY_EXEC, name,
		      target, new,
		      cond->uid, info, error);
	if (!new || nonewprivs) {
		aa_put_label(new);
		return ERR_PTR(error);
	}

	return new;
}

static int profile_onexec(const struct cred *subj_cred,
			  struct aa_profile *profile, struct aa_label *onexec,
			  bool stack, const struct linux_binprm *bprm,
			  char *buffer, struct path_cond *cond,
			  bool *secure_exec)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	aa_state_t state = rules->file.start[AA_CLASS_FILE];
	struct aa_perms perms = {};
	const char *xname = NULL, *info = "change_profile onexec";
	int error = -EACCES;

	AA_BUG(!profile);
	AA_BUG(!onexec);
	AA_BUG(!bprm);
	AA_BUG(!buffer);

	if (profile_unconfined(profile)) {
		 
		 
		return 0;
	}

	error = aa_path_name(&bprm->file->f_path, profile->path_flags, buffer,
			     &xname, &info, profile->disconnected);
	if (error) {
		if (profile_unconfined(profile) ||
		    (profile->label.flags & FLAG_IX_ON_NAME_ERROR)) {
			AA_DEBUG("name lookup ix on error");
			error = 0;
		}
		xname = bprm->filename;
		goto audit;
	}

	 
	state = aa_str_perms(&(rules->file), state, xname, cond, &perms);
	if (!(perms.allow & AA_MAY_ONEXEC)) {
		info = "no change_onexec valid for executable";
		goto audit;
	}
	 
	state = aa_dfa_null_transition(rules->file.dfa, state);
	error = change_profile_perms(profile, onexec, stack, AA_MAY_ONEXEC,
				     state, &perms);
	if (error) {
		perms.allow &= ~AA_MAY_ONEXEC;
		goto audit;
	}

	if (!(perms.xindex & AA_X_UNSAFE)) {
		if (DEBUG_ON) {
			dbg_printk("apparmor: scrubbing environment "
				   "variables for %s label=", xname);
			aa_label_printk(onexec, GFP_KERNEL);
			dbg_printk("\n");
		}
		*secure_exec = true;
	}

audit:
	return aa_audit_file(subj_cred, profile, &perms, OP_EXEC,
			     AA_MAY_ONEXEC, xname,
			     NULL, onexec, cond->uid, info, error);
}

 

static struct aa_label *handle_onexec(const struct cred *subj_cred,
				      struct aa_label *label,
				      struct aa_label *onexec, bool stack,
				      const struct linux_binprm *bprm,
				      char *buffer, struct path_cond *cond,
				      bool *unsafe)
{
	struct aa_profile *profile;
	struct aa_label *new;
	int error;

	AA_BUG(!label);
	AA_BUG(!onexec);
	AA_BUG(!bprm);
	AA_BUG(!buffer);

	if (!stack) {
		error = fn_for_each_in_ns(label, profile,
				profile_onexec(subj_cred, profile, onexec, stack,
					       bprm, buffer, cond, unsafe));
		if (error)
			return ERR_PTR(error);
		new = fn_label_build_in_ns(label, profile, GFP_KERNEL,
				aa_get_newest_label(onexec),
				profile_transition(subj_cred, profile, bprm,
						   buffer,
						   cond, unsafe));

	} else {
		 
		error = fn_for_each_in_ns(label, profile,
				profile_onexec(subj_cred, profile, onexec, stack, bprm,
					       buffer, cond, unsafe));
		if (error)
			return ERR_PTR(error);
		new = fn_label_build_in_ns(label, profile, GFP_KERNEL,
				aa_label_merge(&profile->label, onexec,
					       GFP_KERNEL),
				profile_transition(subj_cred, profile, bprm,
						   buffer,
						   cond, unsafe));
	}

	if (new)
		return new;

	 
	error = fn_for_each_in_ns(label, profile,
			aa_audit_file(subj_cred, profile, &nullperms,
				      OP_CHANGE_ONEXEC,
				      AA_MAY_ONEXEC, bprm->filename, NULL,
				      onexec, GLOBAL_ROOT_UID,
				      "failed to build target label", -ENOMEM));
	return ERR_PTR(error);
}

 
int apparmor_bprm_creds_for_exec(struct linux_binprm *bprm)
{
	struct aa_task_ctx *ctx;
	struct aa_label *label, *new = NULL;
	const struct cred *subj_cred;
	struct aa_profile *profile;
	char *buffer = NULL;
	const char *info = NULL;
	int error = 0;
	bool unsafe = false;
	vfsuid_t vfsuid = i_uid_into_vfsuid(file_mnt_idmap(bprm->file),
					    file_inode(bprm->file));
	struct path_cond cond = {
		vfsuid_into_kuid(vfsuid),
		file_inode(bprm->file)->i_mode
	};

	subj_cred = current_cred();
	ctx = task_ctx(current);
	AA_BUG(!cred_label(bprm->cred));
	AA_BUG(!ctx);

	label = aa_get_newest_label(cred_label(bprm->cred));

	 
	if ((bprm->unsafe & LSM_UNSAFE_NO_NEW_PRIVS) && !unconfined(label) &&
	    !ctx->nnp)
		ctx->nnp = aa_get_label(label);

	 
	buffer = aa_get_buffer(false);
	if (!buffer) {
		error = -ENOMEM;
		goto done;
	}

	 
	if (ctx->onexec)
		new = handle_onexec(subj_cred, label, ctx->onexec, ctx->token,
				    bprm, buffer, &cond, &unsafe);
	else
		new = fn_label_build(label, profile, GFP_KERNEL,
				profile_transition(subj_cred, profile, bprm,
						   buffer,
						   &cond, &unsafe));

	AA_BUG(!new);
	if (IS_ERR(new)) {
		error = PTR_ERR(new);
		goto done;
	} else if (!new) {
		error = -ENOMEM;
		goto done;
	}

	 
	if ((bprm->unsafe & LSM_UNSAFE_NO_NEW_PRIVS) &&
	    !unconfined(label) &&
	    !aa_label_is_unconfined_subset(new, ctx->nnp)) {
		error = -EPERM;
		info = "no new privs";
		goto audit;
	}

	if (bprm->unsafe & LSM_UNSAFE_SHARE) {
		 
		;
	}

	if (bprm->unsafe & (LSM_UNSAFE_PTRACE)) {
		 
		error = may_change_ptraced_domain(bprm->cred, new, &info);
		if (error)
			goto audit;
	}

	if (unsafe) {
		if (DEBUG_ON) {
			dbg_printk("scrubbing environment variables for %s "
				   "label=", bprm->filename);
			aa_label_printk(new, GFP_KERNEL);
			dbg_printk("\n");
		}
		bprm->secureexec = 1;
	}

	if (label->proxy != new->proxy) {
		 
		if (DEBUG_ON) {
			dbg_printk("apparmor: clearing unsafe personality "
				   "bits. %s label=", bprm->filename);
			aa_label_printk(new, GFP_KERNEL);
			dbg_printk("\n");
		}
		bprm->per_clear |= PER_CLEAR_ON_SETID;
	}
	aa_put_label(cred_label(bprm->cred));
	 
	set_cred_label(bprm->cred, new);

done:
	aa_put_label(label);
	aa_put_buffer(buffer);

	return error;

audit:
	error = fn_for_each(label, profile,
			aa_audit_file(current_cred(), profile, &nullperms,
				      OP_EXEC, MAY_EXEC,
				      bprm->filename, NULL, new,
				      vfsuid_into_kuid(vfsuid), info, error));
	aa_put_label(new);
	goto done;
}

 


 
static struct aa_label *build_change_hat(const struct cred *subj_cred,
					 struct aa_profile *profile,
					 const char *name, bool sibling)
{
	struct aa_profile *root, *hat = NULL;
	const char *info = NULL;
	int error = 0;

	if (sibling && PROFILE_IS_HAT(profile)) {
		root = aa_get_profile_rcu(&profile->parent);
	} else if (!sibling && !PROFILE_IS_HAT(profile)) {
		root = aa_get_profile(profile);
	} else {
		info = "conflicting target types";
		error = -EPERM;
		goto audit;
	}

	hat = aa_find_child(root, name);
	if (!hat) {
		error = -ENOENT;
		if (COMPLAIN_MODE(profile)) {
			hat = aa_new_learning_profile(profile, true, name,
						      GFP_KERNEL);
			if (!hat) {
				info = "failed null profile create";
				error = -ENOMEM;
			}
		}
	}
	aa_put_profile(root);

audit:
	aa_audit_file(subj_cred, profile, &nullperms, OP_CHANGE_HAT,
		      AA_MAY_CHANGEHAT,
		      name, hat ? hat->base.hname : NULL,
		      hat ? &hat->label : NULL, GLOBAL_ROOT_UID, info,
		      error);
	if (!hat || (error && error != -ENOENT))
		return ERR_PTR(error);
	 
	return &hat->label;
}

 
static struct aa_label *change_hat(const struct cred *subj_cred,
				   struct aa_label *label, const char *hats[],
				   int count, int flags)
{
	struct aa_profile *profile, *root, *hat = NULL;
	struct aa_label *new;
	struct label_it it;
	bool sibling = false;
	const char *name, *info = NULL;
	int i, error;

	AA_BUG(!label);
	AA_BUG(!hats);
	AA_BUG(count < 1);

	if (PROFILE_IS_HAT(labels_profile(label)))
		sibling = true;

	 
	for (i = 0; i < count && !hat; i++) {
		name = hats[i];
		label_for_each_in_ns(it, labels_ns(label), label, profile) {
			if (sibling && PROFILE_IS_HAT(profile)) {
				root = aa_get_profile_rcu(&profile->parent);
			} else if (!sibling && !PROFILE_IS_HAT(profile)) {
				root = aa_get_profile(profile);
			} else {	 
				info = "conflicting targets types";
				error = -EPERM;
				goto fail;
			}
			hat = aa_find_child(root, name);
			aa_put_profile(root);
			if (!hat) {
				if (!COMPLAIN_MODE(profile))
					goto outer_continue;
				 
			} else if (!PROFILE_IS_HAT(hat)) {
				info = "target not hat";
				error = -EPERM;
				aa_put_profile(hat);
				goto fail;
			}
			aa_put_profile(hat);
		}
		 
		goto build;
outer_continue:
	;
	}
	 
	name = NULL;
	label_for_each_in_ns(it, labels_ns(label), label, profile) {
		if (!list_empty(&profile->base.profiles)) {
			info = "hat not found";
			error = -ENOENT;
			goto fail;
		}
	}
	info = "no hats defined";
	error = -ECHILD;

fail:
	label_for_each_in_ns(it, labels_ns(label), label, profile) {
		 
		 
		if (count > 1 || COMPLAIN_MODE(profile)) {
			aa_audit_file(subj_cred, profile, &nullperms,
				      OP_CHANGE_HAT,
				      AA_MAY_CHANGEHAT, name, NULL, NULL,
				      GLOBAL_ROOT_UID, info, error);
		}
	}
	return ERR_PTR(error);

build:
	new = fn_label_build_in_ns(label, profile, GFP_KERNEL,
				   build_change_hat(subj_cred, profile, name,
						    sibling),
				   aa_get_label(&profile->label));
	if (!new) {
		info = "label build failed";
		error = -ENOMEM;
		goto fail;
	}  

	return new;
}

 
int aa_change_hat(const char *hats[], int count, u64 token, int flags)
{
	const struct cred *subj_cred;
	struct aa_task_ctx *ctx = task_ctx(current);
	struct aa_label *label, *previous, *new = NULL, *target = NULL;
	struct aa_profile *profile;
	struct aa_perms perms = {};
	const char *info = NULL;
	int error = 0;

	 
	subj_cred = get_current_cred();
	label = aa_get_newest_cred_label(subj_cred);
	previous = aa_get_newest_label(ctx->previous);

	 
	if (task_no_new_privs(current) && !unconfined(label) && !ctx->nnp)
		ctx->nnp = aa_get_label(label);

	if (unconfined(label)) {
		info = "unconfined can not change_hat";
		error = -EPERM;
		goto fail;
	}

	if (count) {
		new = change_hat(subj_cred, label, hats, count, flags);
		AA_BUG(!new);
		if (IS_ERR(new)) {
			error = PTR_ERR(new);
			new = NULL;
			 
			goto out;
		}

		 
		error = may_change_ptraced_domain(subj_cred, new, &info);
		if (error)
			goto fail;

		 
		if (task_no_new_privs(current) && !unconfined(label) &&
		    !aa_label_is_unconfined_subset(new, ctx->nnp)) {
			 
			AA_DEBUG("no_new_privs - change_hat denied");
			error = -EPERM;
			goto out;
		}

		if (flags & AA_CHANGE_TEST)
			goto out;

		target = new;
		error = aa_set_current_hat(new, token);
		if (error == -EACCES)
			 
			goto kill;
	} else if (previous && !(flags & AA_CHANGE_TEST)) {
		 
		if (task_no_new_privs(current) && !unconfined(label) &&
		    !aa_label_is_unconfined_subset(previous, ctx->nnp)) {
			 
			AA_DEBUG("no_new_privs - change_hat denied");
			error = -EPERM;
			goto out;
		}

		 
		target = previous;
		error = aa_restore_previous_label(token);
		if (error) {
			if (error == -EACCES)
				goto kill;
			goto fail;
		}
	}  

out:
	aa_put_label(new);
	aa_put_label(previous);
	aa_put_label(label);
	put_cred(subj_cred);

	return error;

kill:
	info = "failed token match";
	perms.kill = AA_MAY_CHANGEHAT;

fail:
	fn_for_each_in_ns(label, profile,
		aa_audit_file(subj_cred, profile, &perms, OP_CHANGE_HAT,
			      AA_MAY_CHANGEHAT, NULL, NULL, target,
			      GLOBAL_ROOT_UID, info, error));

	goto out;
}


static int change_profile_perms_wrapper(const char *op, const char *name,
					const struct cred *subj_cred,
					struct aa_profile *profile,
					struct aa_label *target, bool stack,
					u32 request, struct aa_perms *perms)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	const char *info = NULL;
	int error = 0;

	if (!error)
		error = change_profile_perms(profile, target, stack, request,
					     rules->file.start[AA_CLASS_FILE],
					     perms);
	if (error)
		error = aa_audit_file(subj_cred, profile, perms, op, request,
				      name,
				      NULL, target, GLOBAL_ROOT_UID, info,
				      error);

	return error;
}

 
int aa_change_profile(const char *fqname, int flags)
{
	struct aa_label *label, *new = NULL, *target = NULL;
	struct aa_profile *profile;
	struct aa_perms perms = {};
	const char *info = NULL;
	const char *auditname = fqname;		 
	bool stack = flags & AA_CHANGE_STACK;
	struct aa_task_ctx *ctx = task_ctx(current);
	const struct cred *subj_cred = get_current_cred();
	int error = 0;
	char *op;
	u32 request;

	label = aa_get_current_label();

	 
	if (task_no_new_privs(current) && !unconfined(label) && !ctx->nnp)
		ctx->nnp = aa_get_label(label);

	if (!fqname || !*fqname) {
		aa_put_label(label);
		AA_DEBUG("no profile name");
		return -EINVAL;
	}

	if (flags & AA_CHANGE_ONEXEC) {
		request = AA_MAY_ONEXEC;
		if (stack)
			op = OP_STACK_ONEXEC;
		else
			op = OP_CHANGE_ONEXEC;
	} else {
		request = AA_MAY_CHANGE_PROFILE;
		if (stack)
			op = OP_STACK;
		else
			op = OP_CHANGE_PROFILE;
	}

	if (*fqname == '&') {
		stack = true;
		 
		fqname++;
	}
	target = aa_label_parse(label, fqname, GFP_KERNEL, true, false);
	if (IS_ERR(target)) {
		struct aa_profile *tprofile;

		info = "label not found";
		error = PTR_ERR(target);
		target = NULL;
		 
		if ((flags & AA_CHANGE_TEST) ||
		    !COMPLAIN_MODE(labels_profile(label)))
			goto audit;
		 
		tprofile = aa_new_learning_profile(labels_profile(label), false,
						   fqname, GFP_KERNEL);
		if (!tprofile) {
			info = "failed null profile create";
			error = -ENOMEM;
			goto audit;
		}
		target = &tprofile->label;
		goto check;
	}

	 
	error = fn_for_each_in_ns(label, profile,
			change_profile_perms_wrapper(op, auditname,
						     subj_cred,
						     profile, target, stack,
						     request, &perms));
	if (error)
		 
		goto out;

	 

check:
	 
	error = may_change_ptraced_domain(subj_cred, target, &info);
	if (error && !fn_for_each_in_ns(label, profile,
					COMPLAIN_MODE(profile)))
		goto audit;

	 
	if (flags & AA_CHANGE_TEST)
		goto out;

	 
	if (!stack) {
		new = fn_label_build_in_ns(label, profile, GFP_KERNEL,
					   aa_get_label(target),
					   aa_get_label(&profile->label));
		 
		if (task_no_new_privs(current) && !unconfined(label) &&
		    !aa_label_is_unconfined_subset(new, ctx->nnp)) {
			 
			AA_DEBUG("no_new_privs - change_hat denied");
			error = -EPERM;
			goto out;
		}
	}

	if (!(flags & AA_CHANGE_ONEXEC)) {
		 
		if (stack)
			new = aa_label_merge(label, target, GFP_KERNEL);
		if (IS_ERR_OR_NULL(new)) {
			info = "failed to build target label";
			if (!new)
				error = -ENOMEM;
			else
				error = PTR_ERR(new);
			new = NULL;
			perms.allow = 0;
			goto audit;
		}
		error = aa_replace_current_label(new);
	} else {
		if (new) {
			aa_put_label(new);
			new = NULL;
		}

		 
		error = aa_set_current_onexec(target, stack);
	}

audit:
	error = fn_for_each_in_ns(label, profile,
			aa_audit_file(subj_cred,
				      profile, &perms, op, request, auditname,
				      NULL, new ? new : target,
				      GLOBAL_ROOT_UID, info, error));

out:
	aa_put_label(new);
	aa_put_label(target);
	aa_put_label(label);
	put_cred(subj_cred);

	return error;
}
