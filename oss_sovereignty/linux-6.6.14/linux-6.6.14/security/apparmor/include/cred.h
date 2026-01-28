#ifndef __AA_CONTEXT_H
#define __AA_CONTEXT_H
#include <linux/cred.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "label.h"
#include "policy_ns.h"
#include "task.h"
static inline struct aa_label *cred_label(const struct cred *cred)
{
	struct aa_label **blob = cred->security + apparmor_blob_sizes.lbs_cred;
	AA_BUG(!blob);
	return *blob;
}
static inline void set_cred_label(const struct cred *cred,
				  struct aa_label *label)
{
	struct aa_label **blob = cred->security + apparmor_blob_sizes.lbs_cred;
	AA_BUG(!blob);
	*blob = label;
}
static inline struct aa_label *aa_cred_raw_label(const struct cred *cred)
{
	struct aa_label *label = cred_label(cred);
	AA_BUG(!label);
	return label;
}
static inline struct aa_label *aa_get_newest_cred_label(const struct cred *cred)
{
	return aa_get_newest_label(aa_cred_raw_label(cred));
}
static inline struct aa_label *aa_current_raw_label(void)
{
	return aa_cred_raw_label(current_cred());
}
static inline struct aa_label *aa_get_current_label(void)
{
	struct aa_label *l = aa_current_raw_label();
	if (label_is_stale(l))
		return aa_get_newest_label(l);
	return aa_get_label(l);
}
#define __end_current_label_crit_section(X) end_current_label_crit_section(X)
static inline void end_current_label_crit_section(struct aa_label *label)
{
	if (label != aa_current_raw_label())
		aa_put_label(label);
}
static inline struct aa_label *__begin_current_label_crit_section(void)
{
	struct aa_label *label = aa_current_raw_label();
	if (label_is_stale(label))
		label = aa_get_newest_label(label);
	return label;
}
static inline struct aa_label *begin_current_label_crit_section(void)
{
	struct aa_label *label = aa_current_raw_label();
	might_sleep();
	if (label_is_stale(label)) {
		label = aa_get_newest_label(label);
		if (aa_replace_current_label(label) == 0)
			aa_put_label(label);
	}
	return label;
}
static inline struct aa_ns *aa_get_current_ns(void)
{
	struct aa_label *label;
	struct aa_ns *ns;
	label  = __begin_current_label_crit_section();
	ns = aa_get_ns(labels_ns(label));
	__end_current_label_crit_section(label);
	return ns;
}
#endif  
