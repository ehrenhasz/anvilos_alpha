 

#include <sys/cred.h>

static int
cr_groups_search(const struct group_info *group_info, kgid_t grp)
{
	unsigned int left, right, mid;
	int cmp;

	if (!group_info)
		return (0);

	left = 0;
	right = group_info->ngroups;
	while (left < right) {
		mid = (left + right) / 2;
		cmp = KGID_TO_SGID(grp) -
		    KGID_TO_SGID(GROUP_AT(group_info, mid));

		if (cmp > 0)
			left = mid + 1;
		else if (cmp < 0)
			right = mid;
		else
			return (1);
	}
	return (0);
}

 
void
crhold(cred_t *cr)
{
	(void) get_cred((const cred_t *)cr);
}

 
void
crfree(cred_t *cr)
{
	put_cred((const cred_t *)cr);
}

 
int
crgetngroups(const cred_t *cr)
{
	struct group_info *gi;
	int rc;

	gi = cr->group_info;
	rc = gi->ngroups;
#ifndef HAVE_GROUP_INFO_GID
	 
	if (rc > NGROUPS_PER_BLOCK) {
		WARN_ON_ONCE(1);
		rc = NGROUPS_PER_BLOCK;
	}
#endif
	return (rc);
}

 
gid_t *
crgetgroups(const cred_t *cr)
{
	struct group_info *gi;
	gid_t *gids = NULL;

	gi = cr->group_info;
#ifdef HAVE_GROUP_INFO_GID
	gids = KGIDP_TO_SGIDP(gi->gid);
#else
	if (gi->nblocks > 0)
		gids = KGIDP_TO_SGIDP(gi->blocks[0]);
#endif
	return (gids);
}

 
int
groupmember(gid_t gid, const cred_t *cr)
{
	struct group_info *gi;
	int rc;

	gi = cr->group_info;
	rc = cr_groups_search(gi, SGID_TO_KGID(gid));

	return (rc);
}

 
uid_t
crgetuid(const cred_t *cr)
{
	return (KUID_TO_SUID(cr->fsuid));
}

 
uid_t
crgetruid(const cred_t *cr)
{
	return (KUID_TO_SUID(cr->uid));
}

 
gid_t
crgetgid(const cred_t *cr)
{
	return (KGID_TO_SGID(cr->fsgid));
}

 
zidmap_t *
zfs_get_init_idmap(void)
{
#ifdef HAVE_IOPS_CREATE_IDMAP
	return ((zidmap_t *)&nop_mnt_idmap);
#else
	return ((zidmap_t *)&init_user_ns);
#endif
}

EXPORT_SYMBOL(zfs_get_init_idmap);
EXPORT_SYMBOL(crhold);
EXPORT_SYMBOL(crfree);
EXPORT_SYMBOL(crgetuid);
EXPORT_SYMBOL(crgetruid);
EXPORT_SYMBOL(crgetgid);
EXPORT_SYMBOL(crgetngroups);
EXPORT_SYMBOL(crgetgroups);
EXPORT_SYMBOL(groupmember);
