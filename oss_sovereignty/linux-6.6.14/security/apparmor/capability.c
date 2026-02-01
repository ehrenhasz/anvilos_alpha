
 

#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/security.h>

#include "include/apparmor.h"
#include "include/capability.h"
#include "include/cred.h"
#include "include/policy.h"
#include "include/audit.h"

 
#include "capability_names.h"

struct aa_sfs_entry aa_sfs_entry_caps[] = {
	AA_SFS_FILE_STRING("mask", AA_SFS_CAPS_MASK),
	{ }
};

struct audit_cache {
	struct aa_profile *profile;
	kernel_cap_t caps;
};

static DEFINE_PER_CPU(struct audit_cache, audit_cache);

 
static void audit_cb(struct audit_buffer *ab, void *va)
{
	struct common_audit_data *sa = va;

	audit_log_format(ab, " capname=");
	audit_log_untrustedstring(ab, capability_names[sa->u.cap]);
}

 
static int audit_caps(struct apparmor_audit_data *ad, struct aa_profile *profile,
		      int cap, int error)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	struct audit_cache *ent;
	int type = AUDIT_APPARMOR_AUTO;

	ad->error = error;

	if (likely(!error)) {
		 
		if (likely((AUDIT_MODE(profile) != AUDIT_ALL) &&
			   !cap_raised(rules->caps.audit, cap)))
			return 0;
		type = AUDIT_APPARMOR_AUDIT;
	} else if (KILL_MODE(profile) ||
		   cap_raised(rules->caps.kill, cap)) {
		type = AUDIT_APPARMOR_KILL;
	} else if (cap_raised(rules->caps.quiet, cap) &&
		   AUDIT_MODE(profile) != AUDIT_NOQUIET &&
		   AUDIT_MODE(profile) != AUDIT_ALL) {
		 
		return error;
	}

	 
	ent = &get_cpu_var(audit_cache);
	if (profile == ent->profile && cap_raised(ent->caps, cap)) {
		put_cpu_var(audit_cache);
		if (COMPLAIN_MODE(profile))
			return complain_error(error);
		return error;
	} else {
		aa_put_profile(ent->profile);
		ent->profile = aa_get_profile(profile);
		cap_raise(ent->caps, cap);
	}
	put_cpu_var(audit_cache);

	return aa_audit(type, profile, ad, audit_cb);
}

 
static int profile_capable(struct aa_profile *profile, int cap,
			   unsigned int opts, struct apparmor_audit_data *ad)
{
	struct aa_ruleset *rules = list_first_entry(&profile->rules,
						    typeof(*rules), list);
	int error;

	if (cap_raised(rules->caps.allow, cap) &&
	    !cap_raised(rules->caps.denied, cap))
		error = 0;
	else
		error = -EPERM;

	if (opts & CAP_OPT_NOAUDIT) {
		if (!COMPLAIN_MODE(profile))
			return error;
		 
		ad->info = "optional: no audit";
	}

	return audit_caps(ad, profile, cap, error);
}

 
int aa_capable(const struct cred *subj_cred, struct aa_label *label,
	       int cap, unsigned int opts)
{
	struct aa_profile *profile;
	int error = 0;
	DEFINE_AUDIT_DATA(ad, LSM_AUDIT_DATA_CAP, AA_CLASS_CAP, OP_CAPABLE);

	ad.subj_cred = subj_cred;
	ad.common.u.cap = cap;
	error = fn_for_each_confined(label, profile,
			profile_capable(profile, cap, opts, &ad));

	return error;
}
