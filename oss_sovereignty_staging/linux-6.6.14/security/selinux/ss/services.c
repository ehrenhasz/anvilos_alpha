
 
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/sched.h>
#include <linux/audit.h>
#include <linux/vmalloc.h>
#include <linux/lsm_hooks.h>
#include <net/netlabel.h>

#include "flask.h"
#include "avc.h"
#include "avc_ss.h"
#include "security.h"
#include "context.h"
#include "policydb.h"
#include "sidtab.h"
#include "services.h"
#include "conditional.h"
#include "mls.h"
#include "objsec.h"
#include "netlabel.h"
#include "xfrm.h"
#include "ebitmap.h"
#include "audit.h"
#include "policycap_names.h"
#include "ima.h"

struct selinux_policy_convert_data {
	struct convert_context_args args;
	struct sidtab_convert_params sidtab_params;
};

 
static int context_struct_to_string(struct policydb *policydb,
				    struct context *context,
				    char **scontext,
				    u32 *scontext_len);

static int sidtab_entry_to_string(struct policydb *policydb,
				  struct sidtab *sidtab,
				  struct sidtab_entry *entry,
				  char **scontext,
				  u32 *scontext_len);

static void context_struct_compute_av(struct policydb *policydb,
				      struct context *scontext,
				      struct context *tcontext,
				      u16 tclass,
				      struct av_decision *avd,
				      struct extended_perms *xperms);

static int selinux_set_mapping(struct policydb *pol,
			       const struct security_class_mapping *map,
			       struct selinux_map *out_map)
{
	u16 i, j;
	bool print_unknown_handle = false;

	 
	if (!map)
		return -EINVAL;
	i = 0;
	while (map[i].name)
		i++;

	 
	out_map->mapping = kcalloc(++i, sizeof(*out_map->mapping), GFP_ATOMIC);
	if (!out_map->mapping)
		return -ENOMEM;

	 
	j = 0;
	while (map[j].name) {
		const struct security_class_mapping *p_in = map + (j++);
		struct selinux_mapping *p_out = out_map->mapping + j;
		u16 k;

		 
		if (!strcmp(p_in->name, "")) {
			p_out->num_perms = 0;
			continue;
		}

		p_out->value = string_to_security_class(pol, p_in->name);
		if (!p_out->value) {
			pr_info("SELinux:  Class %s not defined in policy.\n",
			       p_in->name);
			if (pol->reject_unknown)
				goto err;
			p_out->num_perms = 0;
			print_unknown_handle = true;
			continue;
		}

		k = 0;
		while (p_in->perms[k]) {
			 
			if (!*p_in->perms[k]) {
				k++;
				continue;
			}
			p_out->perms[k] = string_to_av_perm(pol, p_out->value,
							    p_in->perms[k]);
			if (!p_out->perms[k]) {
				pr_info("SELinux:  Permission %s in class %s not defined in policy.\n",
				       p_in->perms[k], p_in->name);
				if (pol->reject_unknown)
					goto err;
				print_unknown_handle = true;
			}

			k++;
		}
		p_out->num_perms = k;
	}

	if (print_unknown_handle)
		pr_info("SELinux: the above unknown classes and permissions will be %s\n",
		       pol->allow_unknown ? "allowed" : "denied");

	out_map->size = i;
	return 0;
err:
	kfree(out_map->mapping);
	out_map->mapping = NULL;
	return -EINVAL;
}

 

static u16 unmap_class(struct selinux_map *map, u16 tclass)
{
	if (tclass < map->size)
		return map->mapping[tclass].value;

	return tclass;
}

 
static u16 map_class(struct selinux_map *map, u16 pol_value)
{
	u16 i;

	for (i = 1; i < map->size; i++) {
		if (map->mapping[i].value == pol_value)
			return i;
	}

	return SECCLASS_NULL;
}

static void map_decision(struct selinux_map *map,
			 u16 tclass, struct av_decision *avd,
			 int allow_unknown)
{
	if (tclass < map->size) {
		struct selinux_mapping *mapping = &map->mapping[tclass];
		unsigned int i, n = mapping->num_perms;
		u32 result;

		for (i = 0, result = 0; i < n; i++) {
			if (avd->allowed & mapping->perms[i])
				result |= (u32)1<<i;
			if (allow_unknown && !mapping->perms[i])
				result |= (u32)1<<i;
		}
		avd->allowed = result;

		for (i = 0, result = 0; i < n; i++)
			if (avd->auditallow & mapping->perms[i])
				result |= (u32)1<<i;
		avd->auditallow = result;

		for (i = 0, result = 0; i < n; i++) {
			if (avd->auditdeny & mapping->perms[i])
				result |= (u32)1<<i;
			if (!allow_unknown && !mapping->perms[i])
				result |= (u32)1<<i;
		}
		 
		for (; i < (sizeof(u32)*8); i++)
			result |= (u32)1<<i;
		avd->auditdeny = result;
	}
}

int security_mls_enabled(void)
{
	int mls_enabled;
	struct selinux_policy *policy;

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	mls_enabled = policy->policydb.mls_enabled;
	rcu_read_unlock();
	return mls_enabled;
}

 
static int constraint_expr_eval(struct policydb *policydb,
				struct context *scontext,
				struct context *tcontext,
				struct context *xcontext,
				struct constraint_expr *cexpr)
{
	u32 val1, val2;
	struct context *c;
	struct role_datum *r1, *r2;
	struct mls_level *l1, *l2;
	struct constraint_expr *e;
	int s[CEXPR_MAXDEPTH];
	int sp = -1;

	for (e = cexpr; e; e = e->next) {
		switch (e->expr_type) {
		case CEXPR_NOT:
			BUG_ON(sp < 0);
			s[sp] = !s[sp];
			break;
		case CEXPR_AND:
			BUG_ON(sp < 1);
			sp--;
			s[sp] &= s[sp + 1];
			break;
		case CEXPR_OR:
			BUG_ON(sp < 1);
			sp--;
			s[sp] |= s[sp + 1];
			break;
		case CEXPR_ATTR:
			if (sp == (CEXPR_MAXDEPTH - 1))
				return 0;
			switch (e->attr) {
			case CEXPR_USER:
				val1 = scontext->user;
				val2 = tcontext->user;
				break;
			case CEXPR_TYPE:
				val1 = scontext->type;
				val2 = tcontext->type;
				break;
			case CEXPR_ROLE:
				val1 = scontext->role;
				val2 = tcontext->role;
				r1 = policydb->role_val_to_struct[val1 - 1];
				r2 = policydb->role_val_to_struct[val2 - 1];
				switch (e->op) {
				case CEXPR_DOM:
					s[++sp] = ebitmap_get_bit(&r1->dominates,
								  val2 - 1);
					continue;
				case CEXPR_DOMBY:
					s[++sp] = ebitmap_get_bit(&r2->dominates,
								  val1 - 1);
					continue;
				case CEXPR_INCOMP:
					s[++sp] = (!ebitmap_get_bit(&r1->dominates,
								    val2 - 1) &&
						   !ebitmap_get_bit(&r2->dominates,
								    val1 - 1));
					continue;
				default:
					break;
				}
				break;
			case CEXPR_L1L2:
				l1 = &(scontext->range.level[0]);
				l2 = &(tcontext->range.level[0]);
				goto mls_ops;
			case CEXPR_L1H2:
				l1 = &(scontext->range.level[0]);
				l2 = &(tcontext->range.level[1]);
				goto mls_ops;
			case CEXPR_H1L2:
				l1 = &(scontext->range.level[1]);
				l2 = &(tcontext->range.level[0]);
				goto mls_ops;
			case CEXPR_H1H2:
				l1 = &(scontext->range.level[1]);
				l2 = &(tcontext->range.level[1]);
				goto mls_ops;
			case CEXPR_L1H1:
				l1 = &(scontext->range.level[0]);
				l2 = &(scontext->range.level[1]);
				goto mls_ops;
			case CEXPR_L2H2:
				l1 = &(tcontext->range.level[0]);
				l2 = &(tcontext->range.level[1]);
				goto mls_ops;
mls_ops:
				switch (e->op) {
				case CEXPR_EQ:
					s[++sp] = mls_level_eq(l1, l2);
					continue;
				case CEXPR_NEQ:
					s[++sp] = !mls_level_eq(l1, l2);
					continue;
				case CEXPR_DOM:
					s[++sp] = mls_level_dom(l1, l2);
					continue;
				case CEXPR_DOMBY:
					s[++sp] = mls_level_dom(l2, l1);
					continue;
				case CEXPR_INCOMP:
					s[++sp] = mls_level_incomp(l2, l1);
					continue;
				default:
					BUG();
					return 0;
				}
				break;
			default:
				BUG();
				return 0;
			}

			switch (e->op) {
			case CEXPR_EQ:
				s[++sp] = (val1 == val2);
				break;
			case CEXPR_NEQ:
				s[++sp] = (val1 != val2);
				break;
			default:
				BUG();
				return 0;
			}
			break;
		case CEXPR_NAMES:
			if (sp == (CEXPR_MAXDEPTH-1))
				return 0;
			c = scontext;
			if (e->attr & CEXPR_TARGET)
				c = tcontext;
			else if (e->attr & CEXPR_XTARGET) {
				c = xcontext;
				if (!c) {
					BUG();
					return 0;
				}
			}
			if (e->attr & CEXPR_USER)
				val1 = c->user;
			else if (e->attr & CEXPR_ROLE)
				val1 = c->role;
			else if (e->attr & CEXPR_TYPE)
				val1 = c->type;
			else {
				BUG();
				return 0;
			}

			switch (e->op) {
			case CEXPR_EQ:
				s[++sp] = ebitmap_get_bit(&e->names, val1 - 1);
				break;
			case CEXPR_NEQ:
				s[++sp] = !ebitmap_get_bit(&e->names, val1 - 1);
				break;
			default:
				BUG();
				return 0;
			}
			break;
		default:
			BUG();
			return 0;
		}
	}

	BUG_ON(sp != 0);
	return s[0];
}

 
static int dump_masked_av_helper(void *k, void *d, void *args)
{
	struct perm_datum *pdatum = d;
	char **permission_names = args;

	BUG_ON(pdatum->value < 1 || pdatum->value > 32);

	permission_names[pdatum->value - 1] = (char *)k;

	return 0;
}

static void security_dump_masked_av(struct policydb *policydb,
				    struct context *scontext,
				    struct context *tcontext,
				    u16 tclass,
				    u32 permissions,
				    const char *reason)
{
	struct common_datum *common_dat;
	struct class_datum *tclass_dat;
	struct audit_buffer *ab;
	char *tclass_name;
	char *scontext_name = NULL;
	char *tcontext_name = NULL;
	char *permission_names[32];
	int index;
	u32 length;
	bool need_comma = false;

	if (!permissions)
		return;

	tclass_name = sym_name(policydb, SYM_CLASSES, tclass - 1);
	tclass_dat = policydb->class_val_to_struct[tclass - 1];
	common_dat = tclass_dat->comdatum;

	 
	if (common_dat &&
	    hashtab_map(&common_dat->permissions.table,
			dump_masked_av_helper, permission_names) < 0)
		goto out;

	if (hashtab_map(&tclass_dat->permissions.table,
			dump_masked_av_helper, permission_names) < 0)
		goto out;

	 
	if (context_struct_to_string(policydb, scontext,
				     &scontext_name, &length) < 0)
		goto out;

	if (context_struct_to_string(policydb, tcontext,
				     &tcontext_name, &length) < 0)
		goto out;

	 
	ab = audit_log_start(audit_context(),
			     GFP_ATOMIC, AUDIT_SELINUX_ERR);
	if (!ab)
		goto out;

	audit_log_format(ab, "op=security_compute_av reason=%s "
			 "scontext=%s tcontext=%s tclass=%s perms=",
			 reason, scontext_name, tcontext_name, tclass_name);

	for (index = 0; index < 32; index++) {
		u32 mask = (1 << index);

		if ((mask & permissions) == 0)
			continue;

		audit_log_format(ab, "%s%s",
				 need_comma ? "," : "",
				 permission_names[index]
				 ? permission_names[index] : "????");
		need_comma = true;
	}
	audit_log_end(ab);
out:
	 
	kfree(tcontext_name);
	kfree(scontext_name);
}

 
static void type_attribute_bounds_av(struct policydb *policydb,
				     struct context *scontext,
				     struct context *tcontext,
				     u16 tclass,
				     struct av_decision *avd)
{
	struct context lo_scontext;
	struct context lo_tcontext, *tcontextp = tcontext;
	struct av_decision lo_avd;
	struct type_datum *source;
	struct type_datum *target;
	u32 masked = 0;

	source = policydb->type_val_to_struct[scontext->type - 1];
	BUG_ON(!source);

	if (!source->bounds)
		return;

	target = policydb->type_val_to_struct[tcontext->type - 1];
	BUG_ON(!target);

	memset(&lo_avd, 0, sizeof(lo_avd));

	memcpy(&lo_scontext, scontext, sizeof(lo_scontext));
	lo_scontext.type = source->bounds;

	if (target->bounds) {
		memcpy(&lo_tcontext, tcontext, sizeof(lo_tcontext));
		lo_tcontext.type = target->bounds;
		tcontextp = &lo_tcontext;
	}

	context_struct_compute_av(policydb, &lo_scontext,
				  tcontextp,
				  tclass,
				  &lo_avd,
				  NULL);

	masked = ~lo_avd.allowed & avd->allowed;

	if (likely(!masked))
		return;		 

	 
	avd->allowed &= ~masked;

	 
	security_dump_masked_av(policydb, scontext, tcontext,
				tclass, masked, "bounds");
}

 
void services_compute_xperms_drivers(
		struct extended_perms *xperms,
		struct avtab_node *node)
{
	unsigned int i;

	if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLDRIVER) {
		 
		for (i = 0; i < ARRAY_SIZE(xperms->drivers.p); i++)
			xperms->drivers.p[i] |= node->datum.u.xperms->perms.p[i];
	} else if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLFUNCTION) {
		 
		security_xperm_set(xperms->drivers.p,
					node->datum.u.xperms->driver);
	}

	xperms->len = 1;
}

 
static void context_struct_compute_av(struct policydb *policydb,
				      struct context *scontext,
				      struct context *tcontext,
				      u16 tclass,
				      struct av_decision *avd,
				      struct extended_perms *xperms)
{
	struct constraint_node *constraint;
	struct role_allow *ra;
	struct avtab_key avkey;
	struct avtab_node *node;
	struct class_datum *tclass_datum;
	struct ebitmap *sattr, *tattr;
	struct ebitmap_node *snode, *tnode;
	unsigned int i, j;

	avd->allowed = 0;
	avd->auditallow = 0;
	avd->auditdeny = 0xffffffff;
	if (xperms) {
		memset(&xperms->drivers, 0, sizeof(xperms->drivers));
		xperms->len = 0;
	}

	if (unlikely(!tclass || tclass > policydb->p_classes.nprim)) {
		if (printk_ratelimit())
			pr_warn("SELinux:  Invalid class %hu\n", tclass);
		return;
	}

	tclass_datum = policydb->class_val_to_struct[tclass - 1];

	 
	avkey.target_class = tclass;
	avkey.specified = AVTAB_AV | AVTAB_XPERMS;
	sattr = &policydb->type_attr_map_array[scontext->type - 1];
	tattr = &policydb->type_attr_map_array[tcontext->type - 1];
	ebitmap_for_each_positive_bit(sattr, snode, i) {
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			avkey.source_type = i + 1;
			avkey.target_type = j + 1;
			for (node = avtab_search_node(&policydb->te_avtab,
						      &avkey);
			     node;
			     node = avtab_search_node_next(node, avkey.specified)) {
				if (node->key.specified == AVTAB_ALLOWED)
					avd->allowed |= node->datum.u.data;
				else if (node->key.specified == AVTAB_AUDITALLOW)
					avd->auditallow |= node->datum.u.data;
				else if (node->key.specified == AVTAB_AUDITDENY)
					avd->auditdeny &= node->datum.u.data;
				else if (xperms && (node->key.specified & AVTAB_XPERMS))
					services_compute_xperms_drivers(xperms, node);
			}

			 
			cond_compute_av(&policydb->te_cond_avtab, &avkey,
					avd, xperms);

		}
	}

	 
	constraint = tclass_datum->constraints;
	while (constraint) {
		if ((constraint->permissions & (avd->allowed)) &&
		    !constraint_expr_eval(policydb, scontext, tcontext, NULL,
					  constraint->expr)) {
			avd->allowed &= ~(constraint->permissions);
		}
		constraint = constraint->next;
	}

	 
	if (tclass == policydb->process_class &&
	    (avd->allowed & policydb->process_trans_perms) &&
	    scontext->role != tcontext->role) {
		for (ra = policydb->role_allow; ra; ra = ra->next) {
			if (scontext->role == ra->role &&
			    tcontext->role == ra->new_role)
				break;
		}
		if (!ra)
			avd->allowed &= ~policydb->process_trans_perms;
	}

	 
	type_attribute_bounds_av(policydb, scontext, tcontext,
				 tclass, avd);
}

static int security_validtrans_handle_fail(struct selinux_policy *policy,
					struct sidtab_entry *oentry,
					struct sidtab_entry *nentry,
					struct sidtab_entry *tentry,
					u16 tclass)
{
	struct policydb *p = &policy->policydb;
	struct sidtab *sidtab = policy->sidtab;
	char *o = NULL, *n = NULL, *t = NULL;
	u32 olen, nlen, tlen;

	if (sidtab_entry_to_string(p, sidtab, oentry, &o, &olen))
		goto out;
	if (sidtab_entry_to_string(p, sidtab, nentry, &n, &nlen))
		goto out;
	if (sidtab_entry_to_string(p, sidtab, tentry, &t, &tlen))
		goto out;
	audit_log(audit_context(), GFP_ATOMIC, AUDIT_SELINUX_ERR,
		  "op=security_validate_transition seresult=denied"
		  " oldcontext=%s newcontext=%s taskcontext=%s tclass=%s",
		  o, n, t, sym_name(p, SYM_CLASSES, tclass-1));
out:
	kfree(o);
	kfree(n);
	kfree(t);

	if (!enforcing_enabled())
		return 0;
	return -EPERM;
}

static int security_compute_validatetrans(u32 oldsid, u32 newsid, u32 tasksid,
					  u16 orig_tclass, bool user)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct sidtab_entry *oentry;
	struct sidtab_entry *nentry;
	struct sidtab_entry *tentry;
	struct class_datum *tclass_datum;
	struct constraint_node *constraint;
	u16 tclass;
	int rc = 0;


	if (!selinux_initialized())
		return 0;

	rcu_read_lock();

	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	if (!user)
		tclass = unmap_class(&policy->map, orig_tclass);
	else
		tclass = orig_tclass;

	if (!tclass || tclass > policydb->p_classes.nprim) {
		rc = -EINVAL;
		goto out;
	}
	tclass_datum = policydb->class_val_to_struct[tclass - 1];

	oentry = sidtab_search_entry(sidtab, oldsid);
	if (!oentry) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
			__func__, oldsid);
		rc = -EINVAL;
		goto out;
	}

	nentry = sidtab_search_entry(sidtab, newsid);
	if (!nentry) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
			__func__, newsid);
		rc = -EINVAL;
		goto out;
	}

	tentry = sidtab_search_entry(sidtab, tasksid);
	if (!tentry) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
			__func__, tasksid);
		rc = -EINVAL;
		goto out;
	}

	constraint = tclass_datum->validatetrans;
	while (constraint) {
		if (!constraint_expr_eval(policydb, &oentry->context,
					  &nentry->context, &tentry->context,
					  constraint->expr)) {
			if (user)
				rc = -EPERM;
			else
				rc = security_validtrans_handle_fail(policy,
								oentry,
								nentry,
								tentry,
								tclass);
			goto out;
		}
		constraint = constraint->next;
	}

out:
	rcu_read_unlock();
	return rc;
}

int security_validate_transition_user(u32 oldsid, u32 newsid, u32 tasksid,
				      u16 tclass)
{
	return security_compute_validatetrans(oldsid, newsid, tasksid,
					      tclass, true);
}

int security_validate_transition(u32 oldsid, u32 newsid, u32 tasksid,
				 u16 orig_tclass)
{
	return security_compute_validatetrans(oldsid, newsid, tasksid,
					      orig_tclass, false);
}

 
int security_bounded_transition(u32 old_sid, u32 new_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct sidtab_entry *old_entry, *new_entry;
	struct type_datum *type;
	u32 index;
	int rc;

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	rc = -EINVAL;
	old_entry = sidtab_search_entry(sidtab, old_sid);
	if (!old_entry) {
		pr_err("SELinux: %s: unrecognized SID %u\n",
		       __func__, old_sid);
		goto out;
	}

	rc = -EINVAL;
	new_entry = sidtab_search_entry(sidtab, new_sid);
	if (!new_entry) {
		pr_err("SELinux: %s: unrecognized SID %u\n",
		       __func__, new_sid);
		goto out;
	}

	rc = 0;
	 
	if (old_entry->context.type == new_entry->context.type)
		goto out;

	index = new_entry->context.type;
	while (true) {
		type = policydb->type_val_to_struct[index - 1];
		BUG_ON(!type);

		 
		rc = -EPERM;
		if (!type->bounds)
			break;

		 
		rc = 0;
		if (type->bounds == old_entry->context.type)
			break;

		index = type->bounds;
	}

	if (rc) {
		char *old_name = NULL;
		char *new_name = NULL;
		u32 length;

		if (!sidtab_entry_to_string(policydb, sidtab, old_entry,
					    &old_name, &length) &&
		    !sidtab_entry_to_string(policydb, sidtab, new_entry,
					    &new_name, &length)) {
			audit_log(audit_context(),
				  GFP_ATOMIC, AUDIT_SELINUX_ERR,
				  "op=security_bounded_transition "
				  "seresult=denied "
				  "oldcontext=%s newcontext=%s",
				  old_name, new_name);
		}
		kfree(new_name);
		kfree(old_name);
	}
out:
	rcu_read_unlock();

	return rc;
}

static void avd_init(struct selinux_policy *policy, struct av_decision *avd)
{
	avd->allowed = 0;
	avd->auditallow = 0;
	avd->auditdeny = 0xffffffff;
	if (policy)
		avd->seqno = policy->latest_granting;
	else
		avd->seqno = 0;
	avd->flags = 0;
}

void services_compute_xperms_decision(struct extended_perms_decision *xpermd,
					struct avtab_node *node)
{
	unsigned int i;

	if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLFUNCTION) {
		if (xpermd->driver != node->datum.u.xperms->driver)
			return;
	} else if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLDRIVER) {
		if (!security_xperm_test(node->datum.u.xperms->perms.p,
					xpermd->driver))
			return;
	} else {
		BUG();
	}

	if (node->key.specified == AVTAB_XPERMS_ALLOWED) {
		xpermd->used |= XPERMS_ALLOWED;
		if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLDRIVER) {
			memset(xpermd->allowed->p, 0xff,
					sizeof(xpermd->allowed->p));
		}
		if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLFUNCTION) {
			for (i = 0; i < ARRAY_SIZE(xpermd->allowed->p); i++)
				xpermd->allowed->p[i] |=
					node->datum.u.xperms->perms.p[i];
		}
	} else if (node->key.specified == AVTAB_XPERMS_AUDITALLOW) {
		xpermd->used |= XPERMS_AUDITALLOW;
		if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLDRIVER) {
			memset(xpermd->auditallow->p, 0xff,
					sizeof(xpermd->auditallow->p));
		}
		if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLFUNCTION) {
			for (i = 0; i < ARRAY_SIZE(xpermd->auditallow->p); i++)
				xpermd->auditallow->p[i] |=
					node->datum.u.xperms->perms.p[i];
		}
	} else if (node->key.specified == AVTAB_XPERMS_DONTAUDIT) {
		xpermd->used |= XPERMS_DONTAUDIT;
		if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLDRIVER) {
			memset(xpermd->dontaudit->p, 0xff,
					sizeof(xpermd->dontaudit->p));
		}
		if (node->datum.u.xperms->specified == AVTAB_XPERMS_IOCTLFUNCTION) {
			for (i = 0; i < ARRAY_SIZE(xpermd->dontaudit->p); i++)
				xpermd->dontaudit->p[i] |=
					node->datum.u.xperms->perms.p[i];
		}
	} else {
		BUG();
	}
}

void security_compute_xperms_decision(u32 ssid,
				      u32 tsid,
				      u16 orig_tclass,
				      u8 driver,
				      struct extended_perms_decision *xpermd)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	u16 tclass;
	struct context *scontext, *tcontext;
	struct avtab_key avkey;
	struct avtab_node *node;
	struct ebitmap *sattr, *tattr;
	struct ebitmap_node *snode, *tnode;
	unsigned int i, j;

	xpermd->driver = driver;
	xpermd->used = 0;
	memset(xpermd->allowed->p, 0, sizeof(xpermd->allowed->p));
	memset(xpermd->auditallow->p, 0, sizeof(xpermd->auditallow->p));
	memset(xpermd->dontaudit->p, 0, sizeof(xpermd->dontaudit->p));

	rcu_read_lock();
	if (!selinux_initialized())
		goto allow;

	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	scontext = sidtab_search(sidtab, ssid);
	if (!scontext) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, ssid);
		goto out;
	}

	tcontext = sidtab_search(sidtab, tsid);
	if (!tcontext) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, tsid);
		goto out;
	}

	tclass = unmap_class(&policy->map, orig_tclass);
	if (unlikely(orig_tclass && !tclass)) {
		if (policydb->allow_unknown)
			goto allow;
		goto out;
	}


	if (unlikely(!tclass || tclass > policydb->p_classes.nprim)) {
		pr_warn_ratelimited("SELinux:  Invalid class %hu\n", tclass);
		goto out;
	}

	avkey.target_class = tclass;
	avkey.specified = AVTAB_XPERMS;
	sattr = &policydb->type_attr_map_array[scontext->type - 1];
	tattr = &policydb->type_attr_map_array[tcontext->type - 1];
	ebitmap_for_each_positive_bit(sattr, snode, i) {
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			avkey.source_type = i + 1;
			avkey.target_type = j + 1;
			for (node = avtab_search_node(&policydb->te_avtab,
						      &avkey);
			     node;
			     node = avtab_search_node_next(node, avkey.specified))
				services_compute_xperms_decision(xpermd, node);

			cond_compute_xperms(&policydb->te_cond_avtab,
						&avkey, xpermd);
		}
	}
out:
	rcu_read_unlock();
	return;
allow:
	memset(xpermd->allowed->p, 0xff, sizeof(xpermd->allowed->p));
	goto out;
}

 
void security_compute_av(u32 ssid,
			 u32 tsid,
			 u16 orig_tclass,
			 struct av_decision *avd,
			 struct extended_perms *xperms)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	u16 tclass;
	struct context *scontext = NULL, *tcontext = NULL;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	avd_init(policy, avd);
	xperms->len = 0;
	if (!selinux_initialized())
		goto allow;

	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	scontext = sidtab_search(sidtab, ssid);
	if (!scontext) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, ssid);
		goto out;
	}

	 
	if (ebitmap_get_bit(&policydb->permissive_map, scontext->type))
		avd->flags |= AVD_FLAGS_PERMISSIVE;

	tcontext = sidtab_search(sidtab, tsid);
	if (!tcontext) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, tsid);
		goto out;
	}

	tclass = unmap_class(&policy->map, orig_tclass);
	if (unlikely(orig_tclass && !tclass)) {
		if (policydb->allow_unknown)
			goto allow;
		goto out;
	}
	context_struct_compute_av(policydb, scontext, tcontext, tclass, avd,
				  xperms);
	map_decision(&policy->map, orig_tclass, avd,
		     policydb->allow_unknown);
out:
	rcu_read_unlock();
	return;
allow:
	avd->allowed = 0xffffffff;
	goto out;
}

void security_compute_av_user(u32 ssid,
			      u32 tsid,
			      u16 tclass,
			      struct av_decision *avd)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct context *scontext = NULL, *tcontext = NULL;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	avd_init(policy, avd);
	if (!selinux_initialized())
		goto allow;

	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	scontext = sidtab_search(sidtab, ssid);
	if (!scontext) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, ssid);
		goto out;
	}

	 
	if (ebitmap_get_bit(&policydb->permissive_map, scontext->type))
		avd->flags |= AVD_FLAGS_PERMISSIVE;

	tcontext = sidtab_search(sidtab, tsid);
	if (!tcontext) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, tsid);
		goto out;
	}

	if (unlikely(!tclass)) {
		if (policydb->allow_unknown)
			goto allow;
		goto out;
	}

	context_struct_compute_av(policydb, scontext, tcontext, tclass, avd,
				  NULL);
 out:
	rcu_read_unlock();
	return;
allow:
	avd->allowed = 0xffffffff;
	goto out;
}

 
static int context_struct_to_string(struct policydb *p,
				    struct context *context,
				    char **scontext, u32 *scontext_len)
{
	char *scontextp;

	if (scontext)
		*scontext = NULL;
	*scontext_len = 0;

	if (context->len) {
		*scontext_len = context->len;
		if (scontext) {
			*scontext = kstrdup(context->str, GFP_ATOMIC);
			if (!(*scontext))
				return -ENOMEM;
		}
		return 0;
	}

	 
	*scontext_len += strlen(sym_name(p, SYM_USERS, context->user - 1)) + 1;
	*scontext_len += strlen(sym_name(p, SYM_ROLES, context->role - 1)) + 1;
	*scontext_len += strlen(sym_name(p, SYM_TYPES, context->type - 1)) + 1;
	*scontext_len += mls_compute_context_len(p, context);

	if (!scontext)
		return 0;

	 
	scontextp = kmalloc(*scontext_len, GFP_ATOMIC);
	if (!scontextp)
		return -ENOMEM;
	*scontext = scontextp;

	 
	scontextp += sprintf(scontextp, "%s:%s:%s",
		sym_name(p, SYM_USERS, context->user - 1),
		sym_name(p, SYM_ROLES, context->role - 1),
		sym_name(p, SYM_TYPES, context->type - 1));

	mls_sid_to_context(p, context, &scontextp);

	*scontextp = 0;

	return 0;
}

static int sidtab_entry_to_string(struct policydb *p,
				  struct sidtab *sidtab,
				  struct sidtab_entry *entry,
				  char **scontext, u32 *scontext_len)
{
	int rc = sidtab_sid2str_get(sidtab, entry, scontext, scontext_len);

	if (rc != -ENOENT)
		return rc;

	rc = context_struct_to_string(p, &entry->context, scontext,
				      scontext_len);
	if (!rc && scontext)
		sidtab_sid2str_put(sidtab, entry, *scontext, *scontext_len);
	return rc;
}

#include "initial_sid_to_string.h"

int security_sidtab_hash_stats(char *page)
{
	struct selinux_policy *policy;
	int rc;

	if (!selinux_initialized()) {
		pr_err("SELinux: %s:  called before initial load_policy\n",
		       __func__);
		return -EINVAL;
	}

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	rc = sidtab_hash_stats(policy->sidtab, page);
	rcu_read_unlock();

	return rc;
}

const char *security_get_initial_sid_context(u32 sid)
{
	if (unlikely(sid > SECINITSID_NUM))
		return NULL;
	return initial_sid_to_string[sid];
}

static int security_sid_to_context_core(u32 sid, char **scontext,
					u32 *scontext_len, int force,
					int only_invalid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct sidtab_entry *entry;
	int rc = 0;

	if (scontext)
		*scontext = NULL;
	*scontext_len  = 0;

	if (!selinux_initialized()) {
		if (sid <= SECINITSID_NUM) {
			char *scontextp;
			const char *s = initial_sid_to_string[sid];

			if (!s)
				return -EINVAL;
			*scontext_len = strlen(s) + 1;
			if (!scontext)
				return 0;
			scontextp = kmemdup(s, *scontext_len, GFP_ATOMIC);
			if (!scontextp)
				return -ENOMEM;
			*scontext = scontextp;
			return 0;
		}
		pr_err("SELinux: %s:  called before initial "
		       "load_policy on unknown SID %d\n", __func__, sid);
		return -EINVAL;
	}
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	if (force)
		entry = sidtab_search_entry_force(sidtab, sid);
	else
		entry = sidtab_search_entry(sidtab, sid);
	if (!entry) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
			__func__, sid);
		rc = -EINVAL;
		goto out_unlock;
	}
	if (only_invalid && !entry->context.len)
		goto out_unlock;

	rc = sidtab_entry_to_string(policydb, sidtab, entry, scontext,
				    scontext_len);

out_unlock:
	rcu_read_unlock();
	return rc;

}

 
int security_sid_to_context(u32 sid, char **scontext, u32 *scontext_len)
{
	return security_sid_to_context_core(sid, scontext,
					    scontext_len, 0, 0);
}

int security_sid_to_context_force(u32 sid,
				  char **scontext, u32 *scontext_len)
{
	return security_sid_to_context_core(sid, scontext,
					    scontext_len, 1, 0);
}

 
int security_sid_to_context_inval(u32 sid,
				  char **scontext, u32 *scontext_len)
{
	return security_sid_to_context_core(sid, scontext,
					    scontext_len, 1, 1);
}

 
static int string_to_context_struct(struct policydb *pol,
				    struct sidtab *sidtabp,
				    char *scontext,
				    struct context *ctx,
				    u32 def_sid)
{
	struct role_datum *role;
	struct type_datum *typdatum;
	struct user_datum *usrdatum;
	char *scontextp, *p, oldc;
	int rc = 0;

	context_init(ctx);

	 

	rc = -EINVAL;
	scontextp = scontext;

	 
	p = scontextp;
	while (*p && *p != ':')
		p++;

	if (*p == 0)
		goto out;

	*p++ = 0;

	usrdatum = symtab_search(&pol->p_users, scontextp);
	if (!usrdatum)
		goto out;

	ctx->user = usrdatum->value;

	 
	scontextp = p;
	while (*p && *p != ':')
		p++;

	if (*p == 0)
		goto out;

	*p++ = 0;

	role = symtab_search(&pol->p_roles, scontextp);
	if (!role)
		goto out;
	ctx->role = role->value;

	 
	scontextp = p;
	while (*p && *p != ':')
		p++;
	oldc = *p;
	*p++ = 0;

	typdatum = symtab_search(&pol->p_types, scontextp);
	if (!typdatum || typdatum->attribute)
		goto out;

	ctx->type = typdatum->value;

	rc = mls_context_to_sid(pol, oldc, p, ctx, sidtabp, def_sid);
	if (rc)
		goto out;

	 
	rc = -EINVAL;
	if (!policydb_context_isvalid(pol, ctx))
		goto out;
	rc = 0;
out:
	if (rc)
		context_destroy(ctx);
	return rc;
}

static int security_context_to_sid_core(const char *scontext, u32 scontext_len,
					u32 *sid, u32 def_sid, gfp_t gfp_flags,
					int force)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	char *scontext2, *str = NULL;
	struct context context;
	int rc = 0;

	 
	if (!scontext_len)
		return -EINVAL;

	 
	scontext2 = kmemdup_nul(scontext, scontext_len, gfp_flags);
	if (!scontext2)
		return -ENOMEM;

	if (!selinux_initialized()) {
		u32 i;

		for (i = 1; i < SECINITSID_NUM; i++) {
			const char *s = initial_sid_to_string[i];

			if (s && !strcmp(s, scontext2)) {
				*sid = i;
				goto out;
			}
		}
		*sid = SECINITSID_KERNEL;
		goto out;
	}
	*sid = SECSID_NULL;

	if (force) {
		 
		rc = -ENOMEM;
		str = kstrdup(scontext2, gfp_flags);
		if (!str)
			goto out;
	}
retry:
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;
	rc = string_to_context_struct(policydb, sidtab, scontext2,
				      &context, def_sid);
	if (rc == -EINVAL && force) {
		context.str = str;
		context.len = strlen(str) + 1;
		str = NULL;
	} else if (rc)
		goto out_unlock;
	rc = sidtab_context_to_sid(sidtab, &context, sid);
	if (rc == -ESTALE) {
		rcu_read_unlock();
		if (context.str) {
			str = context.str;
			context.str = NULL;
		}
		context_destroy(&context);
		goto retry;
	}
	context_destroy(&context);
out_unlock:
	rcu_read_unlock();
out:
	kfree(scontext2);
	kfree(str);
	return rc;
}

 
int security_context_to_sid(const char *scontext, u32 scontext_len, u32 *sid,
			    gfp_t gfp)
{
	return security_context_to_sid_core(scontext, scontext_len,
					    sid, SECSID_NULL, gfp, 0);
}

int security_context_str_to_sid(const char *scontext, u32 *sid, gfp_t gfp)
{
	return security_context_to_sid(scontext, strlen(scontext),
				       sid, gfp);
}

 
int security_context_to_sid_default(const char *scontext, u32 scontext_len,
				    u32 *sid, u32 def_sid, gfp_t gfp_flags)
{
	return security_context_to_sid_core(scontext, scontext_len,
					    sid, def_sid, gfp_flags, 1);
}

int security_context_to_sid_force(const char *scontext, u32 scontext_len,
				  u32 *sid)
{
	return security_context_to_sid_core(scontext, scontext_len,
					    sid, SECSID_NULL, GFP_KERNEL, 1);
}

static int compute_sid_handle_invalid_context(
	struct selinux_policy *policy,
	struct sidtab_entry *sentry,
	struct sidtab_entry *tentry,
	u16 tclass,
	struct context *newcontext)
{
	struct policydb *policydb = &policy->policydb;
	struct sidtab *sidtab = policy->sidtab;
	char *s = NULL, *t = NULL, *n = NULL;
	u32 slen, tlen, nlen;
	struct audit_buffer *ab;

	if (sidtab_entry_to_string(policydb, sidtab, sentry, &s, &slen))
		goto out;
	if (sidtab_entry_to_string(policydb, sidtab, tentry, &t, &tlen))
		goto out;
	if (context_struct_to_string(policydb, newcontext, &n, &nlen))
		goto out;
	ab = audit_log_start(audit_context(), GFP_ATOMIC, AUDIT_SELINUX_ERR);
	if (!ab)
		goto out;
	audit_log_format(ab,
			 "op=security_compute_sid invalid_context=");
	 
	audit_log_n_untrustedstring(ab, n, nlen - 1);
	audit_log_format(ab, " scontext=%s tcontext=%s tclass=%s",
			 s, t, sym_name(policydb, SYM_CLASSES, tclass-1));
	audit_log_end(ab);
out:
	kfree(s);
	kfree(t);
	kfree(n);
	if (!enforcing_enabled())
		return 0;
	return -EACCES;
}

static void filename_compute_type(struct policydb *policydb,
				  struct context *newcontext,
				  u32 stype, u32 ttype, u16 tclass,
				  const char *objname)
{
	struct filename_trans_key ft;
	struct filename_trans_datum *datum;

	 
	if (!ebitmap_get_bit(&policydb->filename_trans_ttypes, ttype))
		return;

	ft.ttype = ttype;
	ft.tclass = tclass;
	ft.name = objname;

	datum = policydb_filenametr_search(policydb, &ft);
	while (datum) {
		if (ebitmap_get_bit(&datum->stypes, stype - 1)) {
			newcontext->type = datum->otype;
			return;
		}
		datum = datum->next;
	}
}

static int security_compute_sid(u32 ssid,
				u32 tsid,
				u16 orig_tclass,
				u16 specified,
				const char *objname,
				u32 *out_sid,
				bool kern)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct class_datum *cladatum;
	struct context *scontext, *tcontext, newcontext;
	struct sidtab_entry *sentry, *tentry;
	struct avtab_key avkey;
	struct avtab_node *avnode, *node;
	u16 tclass;
	int rc = 0;
	bool sock;

	if (!selinux_initialized()) {
		switch (orig_tclass) {
		case SECCLASS_PROCESS:  
			*out_sid = ssid;
			break;
		default:
			*out_sid = tsid;
			break;
		}
		goto out;
	}

retry:
	cladatum = NULL;
	context_init(&newcontext);

	rcu_read_lock();

	policy = rcu_dereference(selinux_state.policy);

	if (kern) {
		tclass = unmap_class(&policy->map, orig_tclass);
		sock = security_is_socket_class(orig_tclass);
	} else {
		tclass = orig_tclass;
		sock = security_is_socket_class(map_class(&policy->map,
							  tclass));
	}

	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	sentry = sidtab_search_entry(sidtab, ssid);
	if (!sentry) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, ssid);
		rc = -EINVAL;
		goto out_unlock;
	}
	tentry = sidtab_search_entry(sidtab, tsid);
	if (!tentry) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, tsid);
		rc = -EINVAL;
		goto out_unlock;
	}

	scontext = &sentry->context;
	tcontext = &tentry->context;

	if (tclass && tclass <= policydb->p_classes.nprim)
		cladatum = policydb->class_val_to_struct[tclass - 1];

	 
	switch (specified) {
	case AVTAB_TRANSITION:
	case AVTAB_CHANGE:
		if (cladatum && cladatum->default_user == DEFAULT_TARGET) {
			newcontext.user = tcontext->user;
		} else {
			 
			 
			newcontext.user = scontext->user;
		}
		break;
	case AVTAB_MEMBER:
		 
		newcontext.user = tcontext->user;
		break;
	}

	 
	if (cladatum && cladatum->default_role == DEFAULT_SOURCE) {
		newcontext.role = scontext->role;
	} else if (cladatum && cladatum->default_role == DEFAULT_TARGET) {
		newcontext.role = tcontext->role;
	} else {
		if ((tclass == policydb->process_class) || sock)
			newcontext.role = scontext->role;
		else
			newcontext.role = OBJECT_R_VAL;
	}

	 
	if (cladatum && cladatum->default_type == DEFAULT_SOURCE) {
		newcontext.type = scontext->type;
	} else if (cladatum && cladatum->default_type == DEFAULT_TARGET) {
		newcontext.type = tcontext->type;
	} else {
		if ((tclass == policydb->process_class) || sock) {
			 
			newcontext.type = scontext->type;
		} else {
			 
			newcontext.type = tcontext->type;
		}
	}

	 
	avkey.source_type = scontext->type;
	avkey.target_type = tcontext->type;
	avkey.target_class = tclass;
	avkey.specified = specified;
	avnode = avtab_search_node(&policydb->te_avtab, &avkey);

	 
	if (!avnode) {
		node = avtab_search_node(&policydb->te_cond_avtab, &avkey);
		for (; node; node = avtab_search_node_next(node, specified)) {
			if (node->key.specified & AVTAB_ENABLED) {
				avnode = node;
				break;
			}
		}
	}

	if (avnode) {
		 
		newcontext.type = avnode->datum.u.data;
	}

	 
	if (objname)
		filename_compute_type(policydb, &newcontext, scontext->type,
				      tcontext->type, tclass, objname);

	 
	if (specified & AVTAB_TRANSITION) {
		 
		struct role_trans_datum *rtd;
		struct role_trans_key rtk = {
			.role = scontext->role,
			.type = tcontext->type,
			.tclass = tclass,
		};

		rtd = policydb_roletr_search(policydb, &rtk);
		if (rtd)
			newcontext.role = rtd->new_role;
	}

	 
	rc = mls_compute_sid(policydb, scontext, tcontext, tclass, specified,
			     &newcontext, sock);
	if (rc)
		goto out_unlock;

	 
	if (!policydb_context_isvalid(policydb, &newcontext)) {
		rc = compute_sid_handle_invalid_context(policy, sentry,
							tentry, tclass,
							&newcontext);
		if (rc)
			goto out_unlock;
	}
	 
	rc = sidtab_context_to_sid(sidtab, &newcontext, out_sid);
	if (rc == -ESTALE) {
		rcu_read_unlock();
		context_destroy(&newcontext);
		goto retry;
	}
out_unlock:
	rcu_read_unlock();
	context_destroy(&newcontext);
out:
	return rc;
}

 
int security_transition_sid(u32 ssid, u32 tsid, u16 tclass,
			    const struct qstr *qstr, u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass,
				    AVTAB_TRANSITION,
				    qstr ? qstr->name : NULL, out_sid, true);
}

int security_transition_sid_user(u32 ssid, u32 tsid, u16 tclass,
				 const char *objname, u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass,
				    AVTAB_TRANSITION,
				    objname, out_sid, false);
}

 
int security_member_sid(u32 ssid,
			u32 tsid,
			u16 tclass,
			u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass,
				    AVTAB_MEMBER, NULL,
				    out_sid, false);
}

 
int security_change_sid(u32 ssid,
			u32 tsid,
			u16 tclass,
			u32 *out_sid)
{
	return security_compute_sid(ssid, tsid, tclass, AVTAB_CHANGE, NULL,
				    out_sid, false);
}

static inline int convert_context_handle_invalid_context(
	struct policydb *policydb,
	struct context *context)
{
	char *s;
	u32 len;

	if (enforcing_enabled())
		return -EINVAL;

	if (!context_struct_to_string(policydb, context, &s, &len)) {
		pr_warn("SELinux:  Context %s would be invalid if enforcing\n",
			s);
		kfree(s);
	}
	return 0;
}

 
int services_convert_context(struct convert_context_args *args,
			     struct context *oldc, struct context *newc,
			     gfp_t gfp_flags)
{
	struct ocontext *oc;
	struct role_datum *role;
	struct type_datum *typdatum;
	struct user_datum *usrdatum;
	char *s;
	u32 len;
	int rc;

	if (oldc->str) {
		s = kstrdup(oldc->str, gfp_flags);
		if (!s)
			return -ENOMEM;

		rc = string_to_context_struct(args->newp, NULL, s, newc, SECSID_NULL);
		if (rc == -EINVAL) {
			 
			memcpy(s, oldc->str, oldc->len);
			context_init(newc);
			newc->str = s;
			newc->len = oldc->len;
			return 0;
		}
		kfree(s);
		if (rc) {
			 
			pr_err("SELinux:   Unable to map context %s, rc = %d.\n",
			       oldc->str, -rc);
			return rc;
		}
		pr_info("SELinux:  Context %s became valid (mapped).\n",
			oldc->str);
		return 0;
	}

	context_init(newc);

	 
	usrdatum = symtab_search(&args->newp->p_users,
				 sym_name(args->oldp, SYM_USERS, oldc->user - 1));
	if (!usrdatum)
		goto bad;
	newc->user = usrdatum->value;

	 
	role = symtab_search(&args->newp->p_roles,
			     sym_name(args->oldp, SYM_ROLES, oldc->role - 1));
	if (!role)
		goto bad;
	newc->role = role->value;

	 
	typdatum = symtab_search(&args->newp->p_types,
				 sym_name(args->oldp, SYM_TYPES, oldc->type - 1));
	if (!typdatum)
		goto bad;
	newc->type = typdatum->value;

	 
	if (args->oldp->mls_enabled && args->newp->mls_enabled) {
		rc = mls_convert_context(args->oldp, args->newp, oldc, newc);
		if (rc)
			goto bad;
	} else if (!args->oldp->mls_enabled && args->newp->mls_enabled) {
		 
		oc = args->newp->ocontexts[OCON_ISID];
		while (oc && oc->sid[0] != SECINITSID_UNLABELED)
			oc = oc->next;
		if (!oc) {
			pr_err("SELinux:  unable to look up"
				" the initial SIDs list\n");
			goto bad;
		}
		rc = mls_range_set(newc, &oc->context[0].range);
		if (rc)
			goto bad;
	}

	 
	if (!policydb_context_isvalid(args->newp, newc)) {
		rc = convert_context_handle_invalid_context(args->oldp, oldc);
		if (rc)
			goto bad;
	}

	return 0;
bad:
	 
	rc = context_struct_to_string(args->oldp, oldc, &s, &len);
	if (rc)
		return rc;
	context_destroy(newc);
	newc->str = s;
	newc->len = len;
	pr_info("SELinux:  Context %s became invalid (unmapped).\n",
		newc->str);
	return 0;
}

static void security_load_policycaps(struct selinux_policy *policy)
{
	struct policydb *p;
	unsigned int i;
	struct ebitmap_node *node;

	p = &policy->policydb;

	for (i = 0; i < ARRAY_SIZE(selinux_state.policycap); i++)
		WRITE_ONCE(selinux_state.policycap[i],
			ebitmap_get_bit(&p->policycaps, i));

	for (i = 0; i < ARRAY_SIZE(selinux_policycap_names); i++)
		pr_info("SELinux:  policy capability %s=%d\n",
			selinux_policycap_names[i],
			ebitmap_get_bit(&p->policycaps, i));

	ebitmap_for_each_positive_bit(&p->policycaps, node, i) {
		if (i >= ARRAY_SIZE(selinux_policycap_names))
			pr_info("SELinux:  unknown policy capability %u\n",
				i);
	}
}

static int security_preserve_bools(struct selinux_policy *oldpolicy,
				struct selinux_policy *newpolicy);

static void selinux_policy_free(struct selinux_policy *policy)
{
	if (!policy)
		return;

	sidtab_destroy(policy->sidtab);
	kfree(policy->map.mapping);
	policydb_destroy(&policy->policydb);
	kfree(policy->sidtab);
	kfree(policy);
}

static void selinux_policy_cond_free(struct selinux_policy *policy)
{
	cond_policydb_destroy_dup(&policy->policydb);
	kfree(policy);
}

void selinux_policy_cancel(struct selinux_load_state *load_state)
{
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *oldpolicy;

	oldpolicy = rcu_dereference_protected(state->policy,
					lockdep_is_held(&state->policy_mutex));

	sidtab_cancel_convert(oldpolicy->sidtab);
	selinux_policy_free(load_state->policy);
	kfree(load_state->convert_data);
}

static void selinux_notify_policy_change(u32 seqno)
{
	 
	avc_ss_reset(seqno);
	selnl_notify_policyload(seqno);
	selinux_status_update_policyload(seqno);
	selinux_netlbl_cache_invalidate();
	selinux_xfrm_notify_policyload();
	selinux_ima_measure_state_locked();
}

void selinux_policy_commit(struct selinux_load_state *load_state)
{
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *oldpolicy, *newpolicy = load_state->policy;
	unsigned long flags;
	u32 seqno;

	oldpolicy = rcu_dereference_protected(state->policy,
					lockdep_is_held(&state->policy_mutex));

	 
	if (oldpolicy) {
		if (oldpolicy->policydb.mls_enabled && !newpolicy->policydb.mls_enabled)
			pr_info("SELinux: Disabling MLS support...\n");
		else if (!oldpolicy->policydb.mls_enabled && newpolicy->policydb.mls_enabled)
			pr_info("SELinux: Enabling MLS support...\n");
	}

	 
	if (oldpolicy)
		newpolicy->latest_granting = oldpolicy->latest_granting + 1;
	else
		newpolicy->latest_granting = 1;
	seqno = newpolicy->latest_granting;

	 
	if (oldpolicy) {
		sidtab_freeze_begin(oldpolicy->sidtab, &flags);
		rcu_assign_pointer(state->policy, newpolicy);
		sidtab_freeze_end(oldpolicy->sidtab, &flags);
	} else {
		rcu_assign_pointer(state->policy, newpolicy);
	}

	 
	security_load_policycaps(newpolicy);

	if (!selinux_initialized()) {
		 
		selinux_mark_initialized();
		selinux_complete_init();
	}

	 
	synchronize_rcu();
	selinux_policy_free(oldpolicy);
	kfree(load_state->convert_data);

	 
	selinux_notify_policy_change(seqno);
}

 
int security_load_policy(void *data, size_t len,
			 struct selinux_load_state *load_state)
{
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *newpolicy, *oldpolicy;
	struct selinux_policy_convert_data *convert_data;
	int rc = 0;
	struct policy_file file = { data, len }, *fp = &file;

	newpolicy = kzalloc(sizeof(*newpolicy), GFP_KERNEL);
	if (!newpolicy)
		return -ENOMEM;

	newpolicy->sidtab = kzalloc(sizeof(*newpolicy->sidtab), GFP_KERNEL);
	if (!newpolicy->sidtab) {
		rc = -ENOMEM;
		goto err_policy;
	}

	rc = policydb_read(&newpolicy->policydb, fp);
	if (rc)
		goto err_sidtab;

	newpolicy->policydb.len = len;
	rc = selinux_set_mapping(&newpolicy->policydb, secclass_map,
				&newpolicy->map);
	if (rc)
		goto err_policydb;

	rc = policydb_load_isids(&newpolicy->policydb, newpolicy->sidtab);
	if (rc) {
		pr_err("SELinux:  unable to load the initial SIDs\n");
		goto err_mapping;
	}

	if (!selinux_initialized()) {
		 
		load_state->policy = newpolicy;
		load_state->convert_data = NULL;
		return 0;
	}

	oldpolicy = rcu_dereference_protected(state->policy,
					lockdep_is_held(&state->policy_mutex));

	 
	rc = security_preserve_bools(oldpolicy, newpolicy);
	if (rc) {
		pr_err("SELinux:  unable to preserve booleans\n");
		goto err_free_isids;
	}

	 

	convert_data = kmalloc(sizeof(*convert_data), GFP_KERNEL);
	if (!convert_data) {
		rc = -ENOMEM;
		goto err_free_isids;
	}

	convert_data->args.oldp = &oldpolicy->policydb;
	convert_data->args.newp = &newpolicy->policydb;

	convert_data->sidtab_params.args = &convert_data->args;
	convert_data->sidtab_params.target = newpolicy->sidtab;

	rc = sidtab_convert(oldpolicy->sidtab, &convert_data->sidtab_params);
	if (rc) {
		pr_err("SELinux:  unable to convert the internal"
			" representation of contexts in the new SID"
			" table\n");
		goto err_free_convert_data;
	}

	load_state->policy = newpolicy;
	load_state->convert_data = convert_data;
	return 0;

err_free_convert_data:
	kfree(convert_data);
err_free_isids:
	sidtab_destroy(newpolicy->sidtab);
err_mapping:
	kfree(newpolicy->map.mapping);
err_policydb:
	policydb_destroy(&newpolicy->policydb);
err_sidtab:
	kfree(newpolicy->sidtab);
err_policy:
	kfree(newpolicy);

	return rc;
}

 
static int ocontext_to_sid(struct sidtab *sidtab, struct ocontext *c,
			   size_t index, u32 *out_sid)
{
	int rc;
	u32 sid;

	 
	sid = smp_load_acquire(&c->sid[index]);
	if (!sid) {
		rc = sidtab_context_to_sid(sidtab, &c->context[index], &sid);
		if (rc)
			return rc;

		 
		smp_store_release(&c->sid[index], sid);
	}
	*out_sid = sid;
	return 0;
}

 
int security_port_sid(u8 protocol, u16 port, u32 *out_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct ocontext *c;
	int rc;

	if (!selinux_initialized()) {
		*out_sid = SECINITSID_PORT;
		return 0;
	}

retry:
	rc = 0;
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	c = policydb->ocontexts[OCON_PORT];
	while (c) {
		if (c->u.port.protocol == protocol &&
		    c->u.port.low_port <= port &&
		    c->u.port.high_port >= port)
			break;
		c = c->next;
	}

	if (c) {
		rc = ocontext_to_sid(sidtab, c, 0, out_sid);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc)
			goto out;
	} else {
		*out_sid = SECINITSID_PORT;
	}

out:
	rcu_read_unlock();
	return rc;
}

 
int security_ib_pkey_sid(u64 subnet_prefix, u16 pkey_num, u32 *out_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct ocontext *c;
	int rc;

	if (!selinux_initialized()) {
		*out_sid = SECINITSID_UNLABELED;
		return 0;
	}

retry:
	rc = 0;
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	c = policydb->ocontexts[OCON_IBPKEY];
	while (c) {
		if (c->u.ibpkey.low_pkey <= pkey_num &&
		    c->u.ibpkey.high_pkey >= pkey_num &&
		    c->u.ibpkey.subnet_prefix == subnet_prefix)
			break;

		c = c->next;
	}

	if (c) {
		rc = ocontext_to_sid(sidtab, c, 0, out_sid);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc)
			goto out;
	} else
		*out_sid = SECINITSID_UNLABELED;

out:
	rcu_read_unlock();
	return rc;
}

 
int security_ib_endport_sid(const char *dev_name, u8 port_num, u32 *out_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct ocontext *c;
	int rc;

	if (!selinux_initialized()) {
		*out_sid = SECINITSID_UNLABELED;
		return 0;
	}

retry:
	rc = 0;
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	c = policydb->ocontexts[OCON_IBENDPORT];
	while (c) {
		if (c->u.ibendport.port == port_num &&
		    !strncmp(c->u.ibendport.dev_name,
			     dev_name,
			     IB_DEVICE_NAME_MAX))
			break;

		c = c->next;
	}

	if (c) {
		rc = ocontext_to_sid(sidtab, c, 0, out_sid);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc)
			goto out;
	} else
		*out_sid = SECINITSID_UNLABELED;

out:
	rcu_read_unlock();
	return rc;
}

 
int security_netif_sid(char *name, u32 *if_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	int rc;
	struct ocontext *c;

	if (!selinux_initialized()) {
		*if_sid = SECINITSID_NETIF;
		return 0;
	}

retry:
	rc = 0;
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	c = policydb->ocontexts[OCON_NETIF];
	while (c) {
		if (strcmp(name, c->u.name) == 0)
			break;
		c = c->next;
	}

	if (c) {
		rc = ocontext_to_sid(sidtab, c, 0, if_sid);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc)
			goto out;
	} else
		*if_sid = SECINITSID_NETIF;

out:
	rcu_read_unlock();
	return rc;
}

static int match_ipv6_addrmask(u32 *input, u32 *addr, u32 *mask)
{
	int i, fail = 0;

	for (i = 0; i < 4; i++)
		if (addr[i] != (input[i] & mask[i])) {
			fail = 1;
			break;
		}

	return !fail;
}

 
int security_node_sid(u16 domain,
		      void *addrp,
		      u32 addrlen,
		      u32 *out_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	int rc;
	struct ocontext *c;

	if (!selinux_initialized()) {
		*out_sid = SECINITSID_NODE;
		return 0;
	}

retry:
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	switch (domain) {
	case AF_INET: {
		u32 addr;

		rc = -EINVAL;
		if (addrlen != sizeof(u32))
			goto out;

		addr = *((u32 *)addrp);

		c = policydb->ocontexts[OCON_NODE];
		while (c) {
			if (c->u.node.addr == (addr & c->u.node.mask))
				break;
			c = c->next;
		}
		break;
	}

	case AF_INET6:
		rc = -EINVAL;
		if (addrlen != sizeof(u64) * 2)
			goto out;
		c = policydb->ocontexts[OCON_NODE6];
		while (c) {
			if (match_ipv6_addrmask(addrp, c->u.node6.addr,
						c->u.node6.mask))
				break;
			c = c->next;
		}
		break;

	default:
		rc = 0;
		*out_sid = SECINITSID_NODE;
		goto out;
	}

	if (c) {
		rc = ocontext_to_sid(sidtab, c, 0, out_sid);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc)
			goto out;
	} else {
		*out_sid = SECINITSID_NODE;
	}

	rc = 0;
out:
	rcu_read_unlock();
	return rc;
}

#define SIDS_NEL 25

 

int security_get_user_sids(u32 fromsid,
			   char *username,
			   u32 **sids,
			   u32 *nel)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct context *fromcon, usercon;
	u32 *mysids = NULL, *mysids2, sid;
	u32 i, j, mynel, maxnel = SIDS_NEL;
	struct user_datum *user;
	struct role_datum *role;
	struct ebitmap_node *rnode, *tnode;
	int rc;

	*sids = NULL;
	*nel = 0;

	if (!selinux_initialized())
		return 0;

	mysids = kcalloc(maxnel, sizeof(*mysids), GFP_KERNEL);
	if (!mysids)
		return -ENOMEM;

retry:
	mynel = 0;
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	context_init(&usercon);

	rc = -EINVAL;
	fromcon = sidtab_search(sidtab, fromsid);
	if (!fromcon)
		goto out_unlock;

	rc = -EINVAL;
	user = symtab_search(&policydb->p_users, username);
	if (!user)
		goto out_unlock;

	usercon.user = user->value;

	ebitmap_for_each_positive_bit(&user->roles, rnode, i) {
		role = policydb->role_val_to_struct[i];
		usercon.role = i + 1;
		ebitmap_for_each_positive_bit(&role->types, tnode, j) {
			usercon.type = j + 1;

			if (mls_setup_user_range(policydb, fromcon, user,
						 &usercon))
				continue;

			rc = sidtab_context_to_sid(sidtab, &usercon, &sid);
			if (rc == -ESTALE) {
				rcu_read_unlock();
				goto retry;
			}
			if (rc)
				goto out_unlock;
			if (mynel < maxnel) {
				mysids[mynel++] = sid;
			} else {
				rc = -ENOMEM;
				maxnel += SIDS_NEL;
				mysids2 = kcalloc(maxnel, sizeof(*mysids2), GFP_ATOMIC);
				if (!mysids2)
					goto out_unlock;
				memcpy(mysids2, mysids, mynel * sizeof(*mysids2));
				kfree(mysids);
				mysids = mysids2;
				mysids[mynel++] = sid;
			}
		}
	}
	rc = 0;
out_unlock:
	rcu_read_unlock();
	if (rc || !mynel) {
		kfree(mysids);
		return rc;
	}

	rc = -ENOMEM;
	mysids2 = kcalloc(mynel, sizeof(*mysids2), GFP_KERNEL);
	if (!mysids2) {
		kfree(mysids);
		return rc;
	}
	for (i = 0, j = 0; i < mynel; i++) {
		struct av_decision dummy_avd;
		rc = avc_has_perm_noaudit(fromsid, mysids[i],
					  SECCLASS_PROCESS,  
					  PROCESS__TRANSITION, AVC_STRICT,
					  &dummy_avd);
		if (!rc)
			mysids2[j++] = mysids[i];
		cond_resched();
	}
	kfree(mysids);
	*sids = mysids2;
	*nel = j;
	return 0;
}

 
static inline int __security_genfs_sid(struct selinux_policy *policy,
				       const char *fstype,
				       const char *path,
				       u16 orig_sclass,
				       u32 *sid)
{
	struct policydb *policydb = &policy->policydb;
	struct sidtab *sidtab = policy->sidtab;
	u16 sclass;
	struct genfs *genfs;
	struct ocontext *c;
	int cmp = 0;

	while (path[0] == '/' && path[1] == '/')
		path++;

	sclass = unmap_class(&policy->map, orig_sclass);
	*sid = SECINITSID_UNLABELED;

	for (genfs = policydb->genfs; genfs; genfs = genfs->next) {
		cmp = strcmp(fstype, genfs->fstype);
		if (cmp <= 0)
			break;
	}

	if (!genfs || cmp)
		return -ENOENT;

	for (c = genfs->head; c; c = c->next) {
		size_t len = strlen(c->u.name);
		if ((!c->v.sclass || sclass == c->v.sclass) &&
		    (strncmp(c->u.name, path, len) == 0))
			break;
	}

	if (!c)
		return -ENOENT;

	return ocontext_to_sid(sidtab, c, 0, sid);
}

 
int security_genfs_sid(const char *fstype,
		       const char *path,
		       u16 orig_sclass,
		       u32 *sid)
{
	struct selinux_policy *policy;
	int retval;

	if (!selinux_initialized()) {
		*sid = SECINITSID_UNLABELED;
		return 0;
	}

	do {
		rcu_read_lock();
		policy = rcu_dereference(selinux_state.policy);
		retval = __security_genfs_sid(policy, fstype, path,
					      orig_sclass, sid);
		rcu_read_unlock();
	} while (retval == -ESTALE);
	return retval;
}

int selinux_policy_genfs_sid(struct selinux_policy *policy,
			const char *fstype,
			const char *path,
			u16 orig_sclass,
			u32 *sid)
{
	 
	return __security_genfs_sid(policy, fstype, path, orig_sclass, sid);
}

 
int security_fs_use(struct super_block *sb)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	int rc;
	struct ocontext *c;
	struct superblock_security_struct *sbsec = selinux_superblock(sb);
	const char *fstype = sb->s_type->name;

	if (!selinux_initialized()) {
		sbsec->behavior = SECURITY_FS_USE_NONE;
		sbsec->sid = SECINITSID_UNLABELED;
		return 0;
	}

retry:
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	c = policydb->ocontexts[OCON_FSUSE];
	while (c) {
		if (strcmp(fstype, c->u.name) == 0)
			break;
		c = c->next;
	}

	if (c) {
		sbsec->behavior = c->v.behavior;
		rc = ocontext_to_sid(sidtab, c, 0, &sbsec->sid);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc)
			goto out;
	} else {
		rc = __security_genfs_sid(policy, fstype, "/",
					SECCLASS_DIR, &sbsec->sid);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc) {
			sbsec->behavior = SECURITY_FS_USE_NONE;
			rc = 0;
		} else {
			sbsec->behavior = SECURITY_FS_USE_GENFS;
		}
	}

out:
	rcu_read_unlock();
	return rc;
}

int security_get_bools(struct selinux_policy *policy,
		       u32 *len, char ***names, int **values)
{
	struct policydb *policydb;
	u32 i;
	int rc;

	policydb = &policy->policydb;

	*names = NULL;
	*values = NULL;

	rc = 0;
	*len = policydb->p_bools.nprim;
	if (!*len)
		goto out;

	rc = -ENOMEM;
	*names = kcalloc(*len, sizeof(char *), GFP_ATOMIC);
	if (!*names)
		goto err;

	rc = -ENOMEM;
	*values = kcalloc(*len, sizeof(int), GFP_ATOMIC);
	if (!*values)
		goto err;

	for (i = 0; i < *len; i++) {
		(*values)[i] = policydb->bool_val_to_struct[i]->state;

		rc = -ENOMEM;
		(*names)[i] = kstrdup(sym_name(policydb, SYM_BOOLS, i),
				      GFP_ATOMIC);
		if (!(*names)[i])
			goto err;
	}
	rc = 0;
out:
	return rc;
err:
	if (*names) {
		for (i = 0; i < *len; i++)
			kfree((*names)[i]);
		kfree(*names);
	}
	kfree(*values);
	*len = 0;
	*names = NULL;
	*values = NULL;
	goto out;
}


int security_set_bools(u32 len, int *values)
{
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *newpolicy, *oldpolicy;
	int rc;
	u32 i, seqno = 0;

	if (!selinux_initialized())
		return -EINVAL;

	oldpolicy = rcu_dereference_protected(state->policy,
					lockdep_is_held(&state->policy_mutex));

	 
	if (WARN_ON(len != oldpolicy->policydb.p_bools.nprim))
		return -EINVAL;

	newpolicy = kmemdup(oldpolicy, sizeof(*newpolicy), GFP_KERNEL);
	if (!newpolicy)
		return -ENOMEM;

	 
	rc = cond_policydb_dup(&newpolicy->policydb, &oldpolicy->policydb);
	if (rc) {
		kfree(newpolicy);
		return -ENOMEM;
	}

	 
	for (i = 0; i < len; i++) {
		int new_state = !!values[i];
		int old_state = newpolicy->policydb.bool_val_to_struct[i]->state;

		if (new_state != old_state) {
			audit_log(audit_context(), GFP_ATOMIC,
				AUDIT_MAC_CONFIG_CHANGE,
				"bool=%s val=%d old_val=%d auid=%u ses=%u",
				sym_name(&newpolicy->policydb, SYM_BOOLS, i),
				new_state,
				old_state,
				from_kuid(&init_user_ns, audit_get_loginuid(current)),
				audit_get_sessionid(current));
			newpolicy->policydb.bool_val_to_struct[i]->state = new_state;
		}
	}

	 
	evaluate_cond_nodes(&newpolicy->policydb);

	 
	newpolicy->latest_granting = oldpolicy->latest_granting + 1;
	seqno = newpolicy->latest_granting;

	 
	rcu_assign_pointer(state->policy, newpolicy);

	 
	synchronize_rcu();
	selinux_policy_cond_free(oldpolicy);

	 
	selinux_notify_policy_change(seqno);
	return 0;
}

int security_get_bool_value(u32 index)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	int rc;
	u32 len;

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;

	rc = -EFAULT;
	len = policydb->p_bools.nprim;
	if (index >= len)
		goto out;

	rc = policydb->bool_val_to_struct[index]->state;
out:
	rcu_read_unlock();
	return rc;
}

static int security_preserve_bools(struct selinux_policy *oldpolicy,
				struct selinux_policy *newpolicy)
{
	int rc, *bvalues = NULL;
	char **bnames = NULL;
	struct cond_bool_datum *booldatum;
	u32 i, nbools = 0;

	rc = security_get_bools(oldpolicy, &nbools, &bnames, &bvalues);
	if (rc)
		goto out;
	for (i = 0; i < nbools; i++) {
		booldatum = symtab_search(&newpolicy->policydb.p_bools,
					bnames[i]);
		if (booldatum)
			booldatum->state = bvalues[i];
	}
	evaluate_cond_nodes(&newpolicy->policydb);

out:
	if (bnames) {
		for (i = 0; i < nbools; i++)
			kfree(bnames[i]);
	}
	kfree(bnames);
	kfree(bvalues);
	return rc;
}

 
int security_sid_mls_copy(u32 sid, u32 mls_sid, u32 *new_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	struct context *context1;
	struct context *context2;
	struct context newcon;
	char *s;
	u32 len;
	int rc;

	if (!selinux_initialized()) {
		*new_sid = sid;
		return 0;
	}

retry:
	rc = 0;
	context_init(&newcon);

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	if (!policydb->mls_enabled) {
		*new_sid = sid;
		goto out_unlock;
	}

	rc = -EINVAL;
	context1 = sidtab_search(sidtab, sid);
	if (!context1) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
			__func__, sid);
		goto out_unlock;
	}

	rc = -EINVAL;
	context2 = sidtab_search(sidtab, mls_sid);
	if (!context2) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
			__func__, mls_sid);
		goto out_unlock;
	}

	newcon.user = context1->user;
	newcon.role = context1->role;
	newcon.type = context1->type;
	rc = mls_context_cpy(&newcon, context2);
	if (rc)
		goto out_unlock;

	 
	if (!policydb_context_isvalid(policydb, &newcon)) {
		rc = convert_context_handle_invalid_context(policydb,
							&newcon);
		if (rc) {
			if (!context_struct_to_string(policydb, &newcon, &s,
						      &len)) {
				struct audit_buffer *ab;

				ab = audit_log_start(audit_context(),
						     GFP_ATOMIC,
						     AUDIT_SELINUX_ERR);
				audit_log_format(ab,
						 "op=security_sid_mls_copy invalid_context=");
				 
				audit_log_n_untrustedstring(ab, s, len - 1);
				audit_log_end(ab);
				kfree(s);
			}
			goto out_unlock;
		}
	}
	rc = sidtab_context_to_sid(sidtab, &newcon, new_sid);
	if (rc == -ESTALE) {
		rcu_read_unlock();
		context_destroy(&newcon);
		goto retry;
	}
out_unlock:
	rcu_read_unlock();
	context_destroy(&newcon);
	return rc;
}

 
int security_net_peersid_resolve(u32 nlbl_sid, u32 nlbl_type,
				 u32 xfrm_sid,
				 u32 *peer_sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	int rc;
	struct context *nlbl_ctx;
	struct context *xfrm_ctx;

	*peer_sid = SECSID_NULL;

	 
	if (xfrm_sid == SECSID_NULL) {
		*peer_sid = nlbl_sid;
		return 0;
	}
	 
	if (nlbl_sid == SECSID_NULL || nlbl_type == NETLBL_NLTYPE_UNLABELED) {
		*peer_sid = xfrm_sid;
		return 0;
	}

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	 
	if (!policydb->mls_enabled) {
		rc = 0;
		goto out;
	}

	rc = -EINVAL;
	nlbl_ctx = sidtab_search(sidtab, nlbl_sid);
	if (!nlbl_ctx) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, nlbl_sid);
		goto out;
	}
	rc = -EINVAL;
	xfrm_ctx = sidtab_search(sidtab, xfrm_sid);
	if (!xfrm_ctx) {
		pr_err("SELinux: %s:  unrecognized SID %d\n",
		       __func__, xfrm_sid);
		goto out;
	}
	rc = (mls_context_cmp(nlbl_ctx, xfrm_ctx) ? 0 : -EACCES);
	if (rc)
		goto out;

	 
	*peer_sid = xfrm_sid;
out:
	rcu_read_unlock();
	return rc;
}

static int get_classes_callback(void *k, void *d, void *args)
{
	struct class_datum *datum = d;
	char *name = k, **classes = args;
	u32 value = datum->value - 1;

	classes[value] = kstrdup(name, GFP_ATOMIC);
	if (!classes[value])
		return -ENOMEM;

	return 0;
}

int security_get_classes(struct selinux_policy *policy,
			 char ***classes, u32 *nclasses)
{
	struct policydb *policydb;
	int rc;

	policydb = &policy->policydb;

	rc = -ENOMEM;
	*nclasses = policydb->p_classes.nprim;
	*classes = kcalloc(*nclasses, sizeof(**classes), GFP_ATOMIC);
	if (!*classes)
		goto out;

	rc = hashtab_map(&policydb->p_classes.table, get_classes_callback,
			 *classes);
	if (rc) {
		u32 i;

		for (i = 0; i < *nclasses; i++)
			kfree((*classes)[i]);
		kfree(*classes);
	}

out:
	return rc;
}

static int get_permissions_callback(void *k, void *d, void *args)
{
	struct perm_datum *datum = d;
	char *name = k, **perms = args;
	u32 value = datum->value - 1;

	perms[value] = kstrdup(name, GFP_ATOMIC);
	if (!perms[value])
		return -ENOMEM;

	return 0;
}

int security_get_permissions(struct selinux_policy *policy,
			     const char *class, char ***perms, u32 *nperms)
{
	struct policydb *policydb;
	u32 i;
	int rc;
	struct class_datum *match;

	policydb = &policy->policydb;

	rc = -EINVAL;
	match = symtab_search(&policydb->p_classes, class);
	if (!match) {
		pr_err("SELinux: %s:  unrecognized class %s\n",
			__func__, class);
		goto out;
	}

	rc = -ENOMEM;
	*nperms = match->permissions.nprim;
	*perms = kcalloc(*nperms, sizeof(**perms), GFP_ATOMIC);
	if (!*perms)
		goto out;

	if (match->comdatum) {
		rc = hashtab_map(&match->comdatum->permissions.table,
				 get_permissions_callback, *perms);
		if (rc)
			goto err;
	}

	rc = hashtab_map(&match->permissions.table, get_permissions_callback,
			 *perms);
	if (rc)
		goto err;

out:
	return rc;

err:
	for (i = 0; i < *nperms; i++)
		kfree((*perms)[i]);
	kfree(*perms);
	return rc;
}

int security_get_reject_unknown(void)
{
	struct selinux_policy *policy;
	int value;

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	value = policy->policydb.reject_unknown;
	rcu_read_unlock();
	return value;
}

int security_get_allow_unknown(void)
{
	struct selinux_policy *policy;
	int value;

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	value = policy->policydb.allow_unknown;
	rcu_read_unlock();
	return value;
}

 
int security_policycap_supported(unsigned int req_cap)
{
	struct selinux_policy *policy;
	int rc;

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	rc = ebitmap_get_bit(&policy->policydb.policycaps, req_cap);
	rcu_read_unlock();

	return rc;
}

struct selinux_audit_rule {
	u32 au_seqno;
	struct context au_ctxt;
};

void selinux_audit_rule_free(void *vrule)
{
	struct selinux_audit_rule *rule = vrule;

	if (rule) {
		context_destroy(&rule->au_ctxt);
		kfree(rule);
	}
}

int selinux_audit_rule_init(u32 field, u32 op, char *rulestr, void **vrule)
{
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct selinux_audit_rule *tmprule;
	struct role_datum *roledatum;
	struct type_datum *typedatum;
	struct user_datum *userdatum;
	struct selinux_audit_rule **rule = (struct selinux_audit_rule **)vrule;
	int rc = 0;

	*rule = NULL;

	if (!selinux_initialized())
		return -EOPNOTSUPP;

	switch (field) {
	case AUDIT_SUBJ_USER:
	case AUDIT_SUBJ_ROLE:
	case AUDIT_SUBJ_TYPE:
	case AUDIT_OBJ_USER:
	case AUDIT_OBJ_ROLE:
	case AUDIT_OBJ_TYPE:
		 
		if (op != Audit_equal && op != Audit_not_equal)
			return -EINVAL;
		break;
	case AUDIT_SUBJ_SEN:
	case AUDIT_SUBJ_CLR:
	case AUDIT_OBJ_LEV_LOW:
	case AUDIT_OBJ_LEV_HIGH:
		 
		if (strchr(rulestr, '-'))
			return -EINVAL;
		break;
	default:
		 
		return -EINVAL;
	}

	tmprule = kzalloc(sizeof(struct selinux_audit_rule), GFP_KERNEL);
	if (!tmprule)
		return -ENOMEM;
	context_init(&tmprule->au_ctxt);

	rcu_read_lock();
	policy = rcu_dereference(state->policy);
	policydb = &policy->policydb;
	tmprule->au_seqno = policy->latest_granting;
	switch (field) {
	case AUDIT_SUBJ_USER:
	case AUDIT_OBJ_USER:
		userdatum = symtab_search(&policydb->p_users, rulestr);
		if (!userdatum) {
			rc = -EINVAL;
			goto err;
		}
		tmprule->au_ctxt.user = userdatum->value;
		break;
	case AUDIT_SUBJ_ROLE:
	case AUDIT_OBJ_ROLE:
		roledatum = symtab_search(&policydb->p_roles, rulestr);
		if (!roledatum) {
			rc = -EINVAL;
			goto err;
		}
		tmprule->au_ctxt.role = roledatum->value;
		break;
	case AUDIT_SUBJ_TYPE:
	case AUDIT_OBJ_TYPE:
		typedatum = symtab_search(&policydb->p_types, rulestr);
		if (!typedatum) {
			rc = -EINVAL;
			goto err;
		}
		tmprule->au_ctxt.type = typedatum->value;
		break;
	case AUDIT_SUBJ_SEN:
	case AUDIT_SUBJ_CLR:
	case AUDIT_OBJ_LEV_LOW:
	case AUDIT_OBJ_LEV_HIGH:
		rc = mls_from_string(policydb, rulestr, &tmprule->au_ctxt,
				     GFP_ATOMIC);
		if (rc)
			goto err;
		break;
	}
	rcu_read_unlock();

	*rule = tmprule;
	return 0;

err:
	rcu_read_unlock();
	selinux_audit_rule_free(tmprule);
	*rule = NULL;
	return rc;
}

 
int selinux_audit_rule_known(struct audit_krule *rule)
{
	u32 i;

	for (i = 0; i < rule->field_count; i++) {
		struct audit_field *f = &rule->fields[i];
		switch (f->type) {
		case AUDIT_SUBJ_USER:
		case AUDIT_SUBJ_ROLE:
		case AUDIT_SUBJ_TYPE:
		case AUDIT_SUBJ_SEN:
		case AUDIT_SUBJ_CLR:
		case AUDIT_OBJ_USER:
		case AUDIT_OBJ_ROLE:
		case AUDIT_OBJ_TYPE:
		case AUDIT_OBJ_LEV_LOW:
		case AUDIT_OBJ_LEV_HIGH:
			return 1;
		}
	}

	return 0;
}

int selinux_audit_rule_match(u32 sid, u32 field, u32 op, void *vrule)
{
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *policy;
	struct context *ctxt;
	struct mls_level *level;
	struct selinux_audit_rule *rule = vrule;
	int match = 0;

	if (unlikely(!rule)) {
		WARN_ONCE(1, "selinux_audit_rule_match: missing rule\n");
		return -ENOENT;
	}

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();

	policy = rcu_dereference(state->policy);

	if (rule->au_seqno < policy->latest_granting) {
		match = -ESTALE;
		goto out;
	}

	ctxt = sidtab_search(policy->sidtab, sid);
	if (unlikely(!ctxt)) {
		WARN_ONCE(1, "selinux_audit_rule_match: unrecognized SID %d\n",
			  sid);
		match = -ENOENT;
		goto out;
	}

	 
	switch (field) {
	case AUDIT_SUBJ_USER:
	case AUDIT_OBJ_USER:
		switch (op) {
		case Audit_equal:
			match = (ctxt->user == rule->au_ctxt.user);
			break;
		case Audit_not_equal:
			match = (ctxt->user != rule->au_ctxt.user);
			break;
		}
		break;
	case AUDIT_SUBJ_ROLE:
	case AUDIT_OBJ_ROLE:
		switch (op) {
		case Audit_equal:
			match = (ctxt->role == rule->au_ctxt.role);
			break;
		case Audit_not_equal:
			match = (ctxt->role != rule->au_ctxt.role);
			break;
		}
		break;
	case AUDIT_SUBJ_TYPE:
	case AUDIT_OBJ_TYPE:
		switch (op) {
		case Audit_equal:
			match = (ctxt->type == rule->au_ctxt.type);
			break;
		case Audit_not_equal:
			match = (ctxt->type != rule->au_ctxt.type);
			break;
		}
		break;
	case AUDIT_SUBJ_SEN:
	case AUDIT_SUBJ_CLR:
	case AUDIT_OBJ_LEV_LOW:
	case AUDIT_OBJ_LEV_HIGH:
		level = ((field == AUDIT_SUBJ_SEN ||
			  field == AUDIT_OBJ_LEV_LOW) ?
			 &ctxt->range.level[0] : &ctxt->range.level[1]);
		switch (op) {
		case Audit_equal:
			match = mls_level_eq(&rule->au_ctxt.range.level[0],
					     level);
			break;
		case Audit_not_equal:
			match = !mls_level_eq(&rule->au_ctxt.range.level[0],
					      level);
			break;
		case Audit_lt:
			match = (mls_level_dom(&rule->au_ctxt.range.level[0],
					       level) &&
				 !mls_level_eq(&rule->au_ctxt.range.level[0],
					       level));
			break;
		case Audit_le:
			match = mls_level_dom(&rule->au_ctxt.range.level[0],
					      level);
			break;
		case Audit_gt:
			match = (mls_level_dom(level,
					      &rule->au_ctxt.range.level[0]) &&
				 !mls_level_eq(level,
					       &rule->au_ctxt.range.level[0]));
			break;
		case Audit_ge:
			match = mls_level_dom(level,
					      &rule->au_ctxt.range.level[0]);
			break;
		}
	}

out:
	rcu_read_unlock();
	return match;
}

static int aurule_avc_callback(u32 event)
{
	if (event == AVC_CALLBACK_RESET)
		return audit_update_lsm_rules();
	return 0;
}

static int __init aurule_init(void)
{
	int err;

	err = avc_add_callback(aurule_avc_callback, AVC_CALLBACK_RESET);
	if (err)
		panic("avc_add_callback() failed, error %d\n", err);

	return err;
}
__initcall(aurule_init);

#ifdef CONFIG_NETLABEL
 
static void security_netlbl_cache_add(struct netlbl_lsm_secattr *secattr,
				      u32 sid)
{
	u32 *sid_cache;

	sid_cache = kmalloc(sizeof(*sid_cache), GFP_ATOMIC);
	if (sid_cache == NULL)
		return;
	secattr->cache = netlbl_secattr_cache_alloc(GFP_ATOMIC);
	if (secattr->cache == NULL) {
		kfree(sid_cache);
		return;
	}

	*sid_cache = sid;
	secattr->cache->free = kfree;
	secattr->cache->data = sid_cache;
	secattr->flags |= NETLBL_SECATTR_CACHE;
}

 
int security_netlbl_secattr_to_sid(struct netlbl_lsm_secattr *secattr,
				   u32 *sid)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	struct sidtab *sidtab;
	int rc;
	struct context *ctx;
	struct context ctx_new;

	if (!selinux_initialized()) {
		*sid = SECSID_NULL;
		return 0;
	}

retry:
	rc = 0;
	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;
	sidtab = policy->sidtab;

	if (secattr->flags & NETLBL_SECATTR_CACHE)
		*sid = *(u32 *)secattr->cache->data;
	else if (secattr->flags & NETLBL_SECATTR_SECID)
		*sid = secattr->attr.secid;
	else if (secattr->flags & NETLBL_SECATTR_MLS_LVL) {
		rc = -EIDRM;
		ctx = sidtab_search(sidtab, SECINITSID_NETMSG);
		if (ctx == NULL)
			goto out;

		context_init(&ctx_new);
		ctx_new.user = ctx->user;
		ctx_new.role = ctx->role;
		ctx_new.type = ctx->type;
		mls_import_netlbl_lvl(policydb, &ctx_new, secattr);
		if (secattr->flags & NETLBL_SECATTR_MLS_CAT) {
			rc = mls_import_netlbl_cat(policydb, &ctx_new, secattr);
			if (rc)
				goto out;
		}
		rc = -EIDRM;
		if (!mls_context_isvalid(policydb, &ctx_new)) {
			ebitmap_destroy(&ctx_new.range.level[0].cat);
			goto out;
		}

		rc = sidtab_context_to_sid(sidtab, &ctx_new, sid);
		ebitmap_destroy(&ctx_new.range.level[0].cat);
		if (rc == -ESTALE) {
			rcu_read_unlock();
			goto retry;
		}
		if (rc)
			goto out;

		security_netlbl_cache_add(secattr, *sid);
	} else
		*sid = SECSID_NULL;

out:
	rcu_read_unlock();
	return rc;
}

 
int security_netlbl_sid_to_secattr(u32 sid, struct netlbl_lsm_secattr *secattr)
{
	struct selinux_policy *policy;
	struct policydb *policydb;
	int rc;
	struct context *ctx;

	if (!selinux_initialized())
		return 0;

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	policydb = &policy->policydb;

	rc = -ENOENT;
	ctx = sidtab_search(policy->sidtab, sid);
	if (ctx == NULL)
		goto out;

	rc = -ENOMEM;
	secattr->domain = kstrdup(sym_name(policydb, SYM_TYPES, ctx->type - 1),
				  GFP_ATOMIC);
	if (secattr->domain == NULL)
		goto out;

	secattr->attr.secid = sid;
	secattr->flags |= NETLBL_SECATTR_DOMAIN_CPY | NETLBL_SECATTR_SECID;
	mls_export_netlbl_lvl(policydb, ctx, secattr);
	rc = mls_export_netlbl_cat(policydb, ctx, secattr);
out:
	rcu_read_unlock();
	return rc;
}
#endif  

 
static int __security_read_policy(struct selinux_policy *policy,
				  void *data, size_t *len)
{
	int rc;
	struct policy_file fp;

	fp.data = data;
	fp.len = *len;

	rc = policydb_write(&policy->policydb, &fp);
	if (rc)
		return rc;

	*len = (unsigned long)fp.data - (unsigned long)data;
	return 0;
}

 
int security_read_policy(void **data, size_t *len)
{
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *policy;

	policy = rcu_dereference_protected(
			state->policy, lockdep_is_held(&state->policy_mutex));
	if (!policy)
		return -EINVAL;

	*len = policy->policydb.len;
	*data = vmalloc_user(*len);
	if (!*data)
		return -ENOMEM;

	return __security_read_policy(policy, *data, len);
}

 
int security_read_state_kernel(void **data, size_t *len)
{
	int err;
	struct selinux_state *state = &selinux_state;
	struct selinux_policy *policy;

	policy = rcu_dereference_protected(
			state->policy, lockdep_is_held(&state->policy_mutex));
	if (!policy)
		return -EINVAL;

	*len = policy->policydb.len;
	*data = vmalloc(*len);
	if (!*data)
		return -ENOMEM;

	err = __security_read_policy(policy, *data, len);
	if (err) {
		vfree(*data);
		*data = NULL;
		*len = 0;
	}
	return err;
}
