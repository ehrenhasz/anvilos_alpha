
 

#include <linux/gfp.h>
#include <linux/ptrace.h>

#include "include/audit.h"
#include "include/cred.h"
#include "include/policy.h"
#include "include/task.h"

 
struct aa_label *aa_get_task_label(struct task_struct *task)
{
	struct aa_label *p;

	rcu_read_lock();
	p = aa_get_newest_cred_label(__task_cred(task));
	rcu_read_unlock();

	return p;
}

 
int aa_replace_current_label(struct aa_label *label)
{
	struct aa_label *old = aa_current_raw_label();
	struct aa_task_ctx *ctx = task_ctx(current);
	struct cred *new;

	AA_BUG(!label);

	if (old == label)
		return 0;

	if (current_cred() != current_real_cred())
		return -EBUSY;

	new  = prepare_creds();
	if (!new)
		return -ENOMEM;

	if (ctx->nnp && label_is_stale(ctx->nnp)) {
		struct aa_label *tmp = ctx->nnp;

		ctx->nnp = aa_get_newest_label(tmp);
		aa_put_label(tmp);
	}
	if (unconfined(label) || (labels_ns(old) != labels_ns(label)))
		 
		aa_clear_task_ctx_trans(task_ctx(current));

	 
	aa_get_label(label);
	aa_put_label(cred_label(new));
	set_cred_label(new, label);

	commit_creds(new);
	return 0;
}


 
int aa_set_current_onexec(struct aa_label *label, bool stack)
{
	struct aa_task_ctx *ctx = task_ctx(current);

	aa_get_label(label);
	aa_put_label(ctx->onexec);
	ctx->onexec = label;
	ctx->token = stack;

	return 0;
}

 
int aa_set_current_hat(struct aa_label *label, u64 token)
{
	struct aa_task_ctx *ctx = task_ctx(current);
	struct cred *new;

	new = prepare_creds();
	if (!new)
		return -ENOMEM;
	AA_BUG(!label);

	if (!ctx->previous) {
		 
		ctx->previous = cred_label(new);
		ctx->token = token;
	} else if (ctx->token == token) {
		aa_put_label(cred_label(new));
	} else {
		 
		abort_creds(new);
		return -EACCES;
	}

	set_cred_label(new, aa_get_newest_label(label));
	 
	aa_put_label(ctx->onexec);
	ctx->onexec = NULL;

	commit_creds(new);
	return 0;
}

 
int aa_restore_previous_label(u64 token)
{
	struct aa_task_ctx *ctx = task_ctx(current);
	struct cred *new;

	if (ctx->token != token)
		return -EACCES;
	 
	if (!ctx->previous)
		return 0;

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	aa_put_label(cred_label(new));
	set_cred_label(new, aa_get_newest_label(ctx->previous));
	AA_BUG(!cred_label(new));
	 
	aa_clear_task_ctx_trans(ctx);

	commit_creds(new);

	return 0;
}

 
static const char *audit_ptrace_mask(u32 mask)
{
	switch (mask) {
	case MAY_READ:
		return "read";
	case MAY_WRITE:
		return "trace";
	case AA_MAY_BE_READ:
		return "readby";
	case AA_MAY_BE_TRACED:
		return "tracedby";
	}
	return "";
}

 
static void audit_ptrace_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;
	struct apparmor_audit_data *ad = aad(sa);

	if (ad->request & AA_PTRACE_PERM_MASK) {
		audit_log_format(ab, " requested_mask=\"%s\"",
				 audit_ptrace_mask(ad->request));

		if (ad->denied & AA_PTRACE_PERM_MASK) {
			audit_log_format(ab, " denied_mask=\"%s\"",
					 audit_ptrace_mask(ad->denied));
		}
	}
	audit_log_format(ab, " peer=");
	aa_label_xaudit(ab, labels_ns(ad->subj_label), ad->peer,
			FLAGS_NONE, GFP_ATOMIC);
}

 
 
static int profile_ptrace_perm(const struct cred *cred,
			       struct aa_profile *profile,
			       struct aa_label *peer, u32 request,
			       struct apparmor_audit_data *ad)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	struct aa_perms perms = { };

	ad->subj_cred = cred;
	ad->peer = peer;
	aa_profile_match_label(profile, rules, peer, AA_CLASS_PTRACE, request,
			       &perms);
	aa_apply_modes_to_perms(profile, &perms);
	return aa_check_perms(profile, &perms, request, ad, audit_ptrace_cb);
}

static int profile_tracee_perm(const struct cred *cred,
			       struct aa_profile *tracee,
			       struct aa_label *tracer, u32 request,
			       struct apparmor_audit_data *ad)
{
	if (profile_unconfined(tracee) || unconfined(tracer) ||
	    !ANY_RULE_MEDIATES(&tracee->rules, AA_CLASS_PTRACE))
		return 0;

	return profile_ptrace_perm(cred, tracee, tracer, request, ad);
}

static int profile_tracer_perm(const struct cred *cred,
			       struct aa_profile *tracer,
			       struct aa_label *tracee, u32 request,
			       struct apparmor_audit_data *ad)
{
	if (profile_unconfined(tracer))
		return 0;

	if (ANY_RULE_MEDIATES(&tracer->rules, AA_CLASS_PTRACE))
		return profile_ptrace_perm(cred, tracer, tracee, request, ad);

	 
	if (&tracer->label == tracee)
		return 0;

	ad->subj_label = &tracer->label;
	ad->peer = tracee;
	ad->request = 0;
	ad->error = aa_capable(cred, &tracer->label, CAP_SYS_PTRACE,
			       CAP_OPT_NONE);

	return aa_audit(AUDIT_APPARMOR_AUTO, tracer, ad, audit_ptrace_cb);
}

 
int aa_may_ptrace(const struct cred *tracer_cred, struct aa_label *tracer,
		  const struct cred *tracee_cred, struct aa_label *tracee,
		  u32 request)
{
	struct aa_profile *profile;
	u32 xrequest = request << PTRACE_PERM_SHIFT;
	DEFINE_AUDIT_DATA(sa, LSM_AUDIT_DATA_NONE, AA_CLASS_PTRACE, OP_PTRACE);

	return xcheck_labels(tracer, tracee, profile,
			profile_tracer_perm(tracer_cred, profile, tracee,
					    request, &sa),
			profile_tracee_perm(tracee_cred, profile, tracer,
					    xrequest, &sa));
}
