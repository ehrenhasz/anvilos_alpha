 
 

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/avl.h>
#include <sys/misc.h>
#if defined(_KERNEL)
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <acl/acl_common.h>
#include <sys/debug.h>
#else
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>
#include <acl_common.h>
#endif

#define	ACE_POSIX_SUPPORTED_BITS (ACE_READ_DATA | \
    ACE_WRITE_DATA | ACE_APPEND_DATA | ACE_EXECUTE | \
    ACE_READ_ATTRIBUTES | ACE_READ_ACL | ACE_WRITE_ACL)


#define	ACL_SYNCHRONIZE_SET_DENY		0x0000001
#define	ACL_SYNCHRONIZE_SET_ALLOW		0x0000002
#define	ACL_SYNCHRONIZE_ERR_DENY		0x0000004
#define	ACL_SYNCHRONIZE_ERR_ALLOW		0x0000008

#define	ACL_WRITE_OWNER_SET_DENY		0x0000010
#define	ACL_WRITE_OWNER_SET_ALLOW		0x0000020
#define	ACL_WRITE_OWNER_ERR_DENY		0x0000040
#define	ACL_WRITE_OWNER_ERR_ALLOW		0x0000080

#define	ACL_DELETE_SET_DENY			0x0000100
#define	ACL_DELETE_SET_ALLOW			0x0000200
#define	ACL_DELETE_ERR_DENY			0x0000400
#define	ACL_DELETE_ERR_ALLOW			0x0000800

#define	ACL_WRITE_ATTRS_OWNER_SET_DENY		0x0001000
#define	ACL_WRITE_ATTRS_OWNER_SET_ALLOW		0x0002000
#define	ACL_WRITE_ATTRS_OWNER_ERR_DENY		0x0004000
#define	ACL_WRITE_ATTRS_OWNER_ERR_ALLOW		0x0008000

#define	ACL_WRITE_ATTRS_WRITER_SET_DENY		0x0010000
#define	ACL_WRITE_ATTRS_WRITER_SET_ALLOW	0x0020000
#define	ACL_WRITE_ATTRS_WRITER_ERR_DENY		0x0040000
#define	ACL_WRITE_ATTRS_WRITER_ERR_ALLOW	0x0080000

#define	ACL_WRITE_NAMED_WRITER_SET_DENY		0x0100000
#define	ACL_WRITE_NAMED_WRITER_SET_ALLOW	0x0200000
#define	ACL_WRITE_NAMED_WRITER_ERR_DENY		0x0400000
#define	ACL_WRITE_NAMED_WRITER_ERR_ALLOW	0x0800000

#define	ACL_READ_NAMED_READER_SET_DENY		0x1000000
#define	ACL_READ_NAMED_READER_SET_ALLOW		0x2000000
#define	ACL_READ_NAMED_READER_ERR_DENY		0x4000000
#define	ACL_READ_NAMED_READER_ERR_ALLOW		0x8000000


#define	ACE_VALID_MASK_BITS (\
    ACE_READ_DATA | \
    ACE_LIST_DIRECTORY | \
    ACE_WRITE_DATA | \
    ACE_ADD_FILE | \
    ACE_APPEND_DATA | \
    ACE_ADD_SUBDIRECTORY | \
    ACE_READ_NAMED_ATTRS | \
    ACE_WRITE_NAMED_ATTRS | \
    ACE_EXECUTE | \
    ACE_DELETE_CHILD | \
    ACE_READ_ATTRIBUTES | \
    ACE_WRITE_ATTRIBUTES | \
    ACE_DELETE | \
    ACE_READ_ACL | \
    ACE_WRITE_ACL | \
    ACE_WRITE_OWNER | \
    ACE_SYNCHRONIZE)

#define	ACE_MASK_UNDEFINED			0x80000000

#define	ACE_VALID_FLAG_BITS (ACE_FILE_INHERIT_ACE | \
    ACE_DIRECTORY_INHERIT_ACE | \
    ACE_NO_PROPAGATE_INHERIT_ACE | ACE_INHERIT_ONLY_ACE | \
    ACE_SUCCESSFUL_ACCESS_ACE_FLAG | ACE_FAILED_ACCESS_ACE_FLAG | \
    ACE_IDENTIFIER_GROUP | ACE_OWNER | ACE_GROUP | ACE_EVERYONE)

 

typedef enum {
	ace_unused,
	ace_user_obj,
	ace_user,
	ace_group,  
	ace_other_obj
} ace_to_aent_state_t;

typedef struct acevals {
	uid_t key;
	avl_node_t avl;
	uint32_t mask;
	uint32_t allowed;
	uint32_t denied;
	int aent_type;
} acevals_t;

typedef struct ace_list {
	acevals_t user_obj;
	avl_tree_t user;
	int numusers;
	acevals_t group_obj;
	avl_tree_t group;
	int numgroups;
	acevals_t other_obj;
	uint32_t acl_mask;
	int hasmask;
	int dfacl_flag;
	ace_to_aent_state_t state;
	int seen;  
} ace_list_t;

 
void
ksort(caddr_t v, int n, int s, int (*f)(void *, void *))
{
	int g, i, j, ii;
	unsigned int *p1, *p2;
	unsigned int tmp;

	 
	if (v == NULL || n <= 1)
		return;

	 
	ASSERT3U(((uintptr_t)v & 0x3), ==, 0);
	ASSERT3S((s & 0x3), ==, 0);
	ASSERT3S(s, >, 0);
	for (g = n / 2; g > 0; g /= 2) {
		for (i = g; i < n; i++) {
			for (j = i - g; j >= 0 &&
			    (*f)(v + j * s, v + (j + g) * s) == 1;
			    j -= g) {
				p1 = (void *)(v + j * s);
				p2 = (void *)(v + (j + g) * s);
				for (ii = 0; ii < s / 4; ii++) {
					tmp = *p1;
					*p1++ = *p2;
					*p2++ = tmp;
				}
			}
		}
	}
}

 
int
cmp2acls(void *a, void *b)
{
	aclent_t *x = (aclent_t *)a;
	aclent_t *y = (aclent_t *)b;

	 
	if (x->a_type < y->a_type)
		return (-1);
	if (x->a_type > y->a_type)
		return (1);
	 
	if (x->a_id < y->a_id)
		return (-1);
	if (x->a_id > y->a_id)
		return (1);
	 
	if (x->a_perm < y->a_perm)
		return (-1);
	if (x->a_perm > y->a_perm)
		return (1);
	 
	return (0);
}

static int
cacl_malloc(void **ptr, size_t size)
{
	*ptr = kmem_zalloc(size, KM_SLEEP);
	return (0);
}


#if !defined(_KERNEL)
acl_t *
acl_alloc(enum acl_type type)
{
	acl_t *aclp;

	if (cacl_malloc((void **)&aclp, sizeof (acl_t)) != 0)
		return (NULL);

	aclp->acl_aclp = NULL;
	aclp->acl_cnt = 0;

	switch (type) {
	case ACE_T:
		aclp->acl_type = ACE_T;
		aclp->acl_entry_size = sizeof (ace_t);
		break;
	case ACLENT_T:
		aclp->acl_type = ACLENT_T;
		aclp->acl_entry_size = sizeof (aclent_t);
		break;
	default:
		acl_free(aclp);
		aclp = NULL;
	}
	return (aclp);
}

 
void
acl_free(acl_t *aclp)
{
	int acl_size;

	if (aclp == NULL)
		return;

	if (aclp->acl_aclp) {
		acl_size = aclp->acl_cnt * aclp->acl_entry_size;
		cacl_free(aclp->acl_aclp, acl_size);
	}

	cacl_free(aclp, sizeof (acl_t));
}

static uint32_t
access_mask_set(int haswriteperm, int hasreadperm, int isowner, int isallow)
{
	uint32_t access_mask = 0;
	int acl_produce;
	int synchronize_set = 0, write_owner_set = 0;
	int delete_set = 0, write_attrs_set = 0;
	int read_named_set = 0, write_named_set = 0;

	acl_produce = (ACL_SYNCHRONIZE_SET_ALLOW |
	    ACL_WRITE_ATTRS_OWNER_SET_ALLOW |
	    ACL_WRITE_ATTRS_WRITER_SET_DENY);

	if (isallow) {
		synchronize_set = ACL_SYNCHRONIZE_SET_ALLOW;
		write_owner_set = ACL_WRITE_OWNER_SET_ALLOW;
		delete_set = ACL_DELETE_SET_ALLOW;
		if (hasreadperm)
			read_named_set = ACL_READ_NAMED_READER_SET_ALLOW;
		if (haswriteperm)
			write_named_set = ACL_WRITE_NAMED_WRITER_SET_ALLOW;
		if (isowner)
			write_attrs_set = ACL_WRITE_ATTRS_OWNER_SET_ALLOW;
		else if (haswriteperm)
			write_attrs_set = ACL_WRITE_ATTRS_WRITER_SET_ALLOW;
	} else {

		synchronize_set = ACL_SYNCHRONIZE_SET_DENY;
		write_owner_set = ACL_WRITE_OWNER_SET_DENY;
		delete_set = ACL_DELETE_SET_DENY;
		if (hasreadperm)
			read_named_set = ACL_READ_NAMED_READER_SET_DENY;
		if (haswriteperm)
			write_named_set = ACL_WRITE_NAMED_WRITER_SET_DENY;
		if (isowner)
			write_attrs_set = ACL_WRITE_ATTRS_OWNER_SET_DENY;
		else if (haswriteperm)
			write_attrs_set = ACL_WRITE_ATTRS_WRITER_SET_DENY;
		else
			 
			access_mask |= ACE_WRITE_ATTRIBUTES;
	}

	if (acl_produce & synchronize_set)
		access_mask |= ACE_SYNCHRONIZE;
	if (acl_produce & write_owner_set)
		access_mask |= ACE_WRITE_OWNER;
	if (acl_produce & delete_set)
		access_mask |= ACE_DELETE;
	if (acl_produce & write_attrs_set)
		access_mask |= ACE_WRITE_ATTRIBUTES;
	if (acl_produce & read_named_set)
		access_mask |= ACE_READ_NAMED_ATTRS;
	if (acl_produce & write_named_set)
		access_mask |= ACE_WRITE_NAMED_ATTRS;

	return (access_mask);
}

 
static uint32_t
mode_to_ace_access(mode_t mode, boolean_t isdir, int isowner, int isallow)
{
	uint32_t access = 0;
	int haswriteperm = 0;
	int hasreadperm = 0;

	if (isallow) {
		haswriteperm = (mode & S_IWOTH);
		hasreadperm = (mode & S_IROTH);
	} else {
		haswriteperm = !(mode & S_IWOTH);
		hasreadperm = !(mode & S_IROTH);
	}

	 
	access = access_mask_set(haswriteperm, hasreadperm, isowner, isallow);

	if (isallow) {
		access |= ACE_READ_ACL | ACE_READ_ATTRIBUTES;
		if (isowner)
			access |= ACE_WRITE_ACL;
	} else {
		if (! isowner)
			access |= ACE_WRITE_ACL;
	}

	 
	if (mode & S_IROTH) {
		access |= ACE_READ_DATA;
	}
	 
	if (mode & S_IWOTH) {
		access |= ACE_WRITE_DATA |
		    ACE_APPEND_DATA;
		if (isdir)
			access |= ACE_DELETE_CHILD;
	}
	 
	if (mode & S_IXOTH) {
		access |= ACE_EXECUTE;
	}

	return (access);
}

 
static void
ace_make_deny(ace_t *allow, ace_t *deny, int isdir, int isowner)
{
	(void) memcpy(deny, allow, sizeof (ace_t));

	deny->a_who = allow->a_who;

	deny->a_type = ACE_ACCESS_DENIED_ACE_TYPE;
	deny->a_access_mask ^= ACE_POSIX_SUPPORTED_BITS;
	if (isdir)
		deny->a_access_mask ^= ACE_DELETE_CHILD;

	deny->a_access_mask &= ~(ACE_SYNCHRONIZE | ACE_WRITE_OWNER |
	    ACE_DELETE | ACE_WRITE_ATTRIBUTES | ACE_READ_NAMED_ATTRS |
	    ACE_WRITE_NAMED_ATTRS);
	deny->a_access_mask |= access_mask_set((allow->a_access_mask &
	    ACE_WRITE_DATA), (allow->a_access_mask & ACE_READ_DATA), isowner,
	    B_FALSE);
}
 
static int
ln_aent_preprocess(aclent_t *aclent, int n,
    int *hasmask, mode_t *mask,
    int *numuser, int *numgroup, int *needsort)
{
	int error = 0;
	int i;
	int curtype = 0;

	*hasmask = 0;
	*mask = 07;
	*needsort = 0;
	*numuser = 0;
	*numgroup = 0;

	for (i = 0; i < n; i++) {
		if (aclent[i].a_type < curtype)
			*needsort = 1;
		else if (aclent[i].a_type > curtype)
			curtype = aclent[i].a_type;
		if (aclent[i].a_type & USER)
			(*numuser)++;
		if (aclent[i].a_type & (GROUP | GROUP_OBJ))
			(*numgroup)++;
		if (aclent[i].a_type & CLASS_OBJ) {
			if (*hasmask) {
				error = EINVAL;
				goto out;
			} else {
				*hasmask = 1;
				*mask = aclent[i].a_perm;
			}
		}
	}

	if ((! *hasmask) && (*numuser + *numgroup > 1)) {
		error = EINVAL;
		goto out;
	}

out:
	return (error);
}

 
static int
ln_aent_to_ace(aclent_t *aclent, int n, ace_t **acepp, int *rescount, int isdir)
{
	int error = 0;
	mode_t mask;
	int numuser, numgroup, needsort;
	int resultsize = 0;
	int i, groupi = 0, skip;
	ace_t *acep, *result = NULL;
	int hasmask;

	error = ln_aent_preprocess(aclent, n, &hasmask, &mask,
	    &numuser, &numgroup, &needsort);
	if (error != 0)
		goto out;

	 
	resultsize = n * 2;
	if (hasmask) {
		 
		resultsize += numuser + numgroup;
		 
		resultsize -= 2;
	}

	 
	if (needsort)
		ksort((caddr_t)aclent, n, sizeof (aclent_t), cmp2acls);

	if (cacl_malloc((void **)&result, resultsize * sizeof (ace_t)) != 0)
		goto out;

	acep = result;

	for (i = 0; i < n; i++) {
		 
		if (aclent[i].a_type & CLASS_OBJ)
			continue;

		 
		if ((hasmask) &&
		    (aclent[i].a_type & (USER | GROUP | GROUP_OBJ))) {
			acep->a_type = ACE_ACCESS_DENIED_ACE_TYPE;
			acep->a_flags = 0;
			if (aclent[i].a_type & GROUP_OBJ) {
				acep->a_who = (uid_t)-1;
				acep->a_flags |=
				    (ACE_IDENTIFIER_GROUP|ACE_GROUP);
			} else if (aclent[i].a_type & USER) {
				acep->a_who = aclent[i].a_id;
			} else {
				acep->a_who = aclent[i].a_id;
				acep->a_flags |= ACE_IDENTIFIER_GROUP;
			}
			if (aclent[i].a_type & ACL_DEFAULT) {
				acep->a_flags |= ACE_INHERIT_ONLY_ACE |
				    ACE_FILE_INHERIT_ACE |
				    ACE_DIRECTORY_INHERIT_ACE;
			}
			 
			acep->a_access_mask = mode_to_ace_access((mask ^ 07),
			    isdir, 0, 0);
			acep += 1;
		}

		 
		acep->a_access_mask = mode_to_ace_access(aclent[i].a_perm,
		    isdir, aclent[i].a_type & USER_OBJ, 1);

		 
		if (aclent[i].a_type & ACL_DEFAULT) {
			acep->a_flags |= ACE_INHERIT_ONLY_ACE |
			    ACE_FILE_INHERIT_ACE |
			    ACE_DIRECTORY_INHERIT_ACE;
		}

		 
		if (aclent[i].a_type & USER_OBJ) {
			acep->a_who = (uid_t)-1;
			acep->a_flags |= ACE_OWNER;
			ace_make_deny(acep, acep + 1, isdir, B_TRUE);
			acep += 2;
		} else if (aclent[i].a_type & USER) {
			acep->a_who = aclent[i].a_id;
			ace_make_deny(acep, acep + 1, isdir, B_FALSE);
			acep += 2;
		} else if (aclent[i].a_type & (GROUP_OBJ | GROUP)) {
			if (aclent[i].a_type & GROUP_OBJ) {
				acep->a_who = (uid_t)-1;
				acep->a_flags |= ACE_GROUP;
			} else {
				acep->a_who = aclent[i].a_id;
			}
			acep->a_flags |= ACE_IDENTIFIER_GROUP;
			 
			skip = (2 * numgroup) - 1 - groupi;
			ace_make_deny(acep, acep + skip, isdir, B_FALSE);
			 
			if (++groupi >= numgroup)
				acep += numgroup + 1;
			else
				acep += 1;
		} else if (aclent[i].a_type & OTHER_OBJ) {
			acep->a_who = (uid_t)-1;
			acep->a_flags |= ACE_EVERYONE;
			ace_make_deny(acep, acep + 1, isdir, B_FALSE);
			acep += 2;
		} else {
			error = EINVAL;
			goto out;
		}
	}

	*acepp = result;
	*rescount = resultsize;

out:
	if (error != 0) {
		if ((result != NULL) && (resultsize > 0)) {
			cacl_free(result, resultsize * sizeof (ace_t));
		}
	}

	return (error);
}

static int
convert_aent_to_ace(aclent_t *aclentp, int aclcnt, boolean_t isdir,
    ace_t **retacep, int *retacecnt)
{
	ace_t *acep;
	ace_t *dfacep;
	int acecnt = 0;
	int dfacecnt = 0;
	int dfaclstart = 0;
	int dfaclcnt = 0;
	aclent_t *aclp;
	int i;
	int error;
	int acesz, dfacesz;

	ksort((caddr_t)aclentp, aclcnt, sizeof (aclent_t), cmp2acls);

	for (i = 0, aclp = aclentp; i < aclcnt; aclp++, i++) {
		if (aclp->a_type & ACL_DEFAULT)
			break;
	}

	if (i < aclcnt) {
		dfaclstart = i;
		dfaclcnt = aclcnt - i;
	}

	if (dfaclcnt && !isdir) {
		return (EINVAL);
	}

	error = ln_aent_to_ace(aclentp, i,  &acep, &acecnt, isdir);
	if (error)
		return (error);

	if (dfaclcnt) {
		error = ln_aent_to_ace(&aclentp[dfaclstart], dfaclcnt,
		    &dfacep, &dfacecnt, isdir);
		if (error) {
			if (acep) {
				cacl_free(acep, acecnt * sizeof (ace_t));
			}
			return (error);
		}
	}

	if (dfacecnt != 0) {
		acesz = sizeof (ace_t) * acecnt;
		dfacesz = sizeof (ace_t) * dfacecnt;
		acep = cacl_realloc(acep, acesz, acesz + dfacesz);
		if (acep == NULL)
			return (ENOMEM);
		if (dfaclcnt) {
			(void) memcpy(acep + acecnt, dfacep, dfacesz);
		}
	}
	if (dfaclcnt)
		cacl_free(dfacep, dfacecnt * sizeof (ace_t));

	*retacecnt = acecnt + dfacecnt;
	*retacep = acep;
	return (0);
}

static int
ace_mask_to_mode(uint32_t  mask, o_mode_t *modep, boolean_t isdir)
{
	int error = 0;
	o_mode_t mode = 0;
	uint32_t bits, wantbits;

	 
	if (mask & ACE_READ_DATA)
		mode |= S_IROTH;

	 
	wantbits = (ACE_WRITE_DATA | ACE_APPEND_DATA);
	if (isdir)
		wantbits |= ACE_DELETE_CHILD;
	bits = mask & wantbits;
	if (bits != 0) {
		if (bits != wantbits) {
			error = ENOTSUP;
			goto out;
		}
		mode |= S_IWOTH;
	}

	 
	if (mask & ACE_EXECUTE) {
		mode |= S_IXOTH;
	}

	*modep = mode;

out:
	return (error);
}

static void
acevals_init(acevals_t *vals, uid_t key)
{
	memset(vals, 0, sizeof (*vals));
	vals->allowed = ACE_MASK_UNDEFINED;
	vals->denied = ACE_MASK_UNDEFINED;
	vals->mask = ACE_MASK_UNDEFINED;
	vals->key = key;
}

static void
ace_list_init(ace_list_t *al, int dfacl_flag)
{
	acevals_init(&al->user_obj, 0);
	acevals_init(&al->group_obj, 0);
	acevals_init(&al->other_obj, 0);
	al->numusers = 0;
	al->numgroups = 0;
	al->acl_mask = 0;
	al->hasmask = 0;
	al->state = ace_unused;
	al->seen = 0;
	al->dfacl_flag = dfacl_flag;
}

 
static acevals_t *
acevals_find(ace_t *ace, avl_tree_t *avl, int *num)
{
	acevals_t key, *rc;
	avl_index_t where;

	key.key = ace->a_who;
	rc = avl_find(avl, &key, &where);
	if (rc != NULL)
		return (rc);

	 
	if (cacl_malloc((void **)&rc, sizeof (acevals_t)) != 0)
		return (NULL);

	acevals_init(rc, ace->a_who);
	avl_insert(avl, rc, where);
	(*num)++;

	return (rc);
}

static int
access_mask_check(ace_t *acep, int mask_bit, int isowner)
{
	int set_deny, err_deny;
	int set_allow, err_allow;
	int acl_consume;
	int haswriteperm, hasreadperm;

	if (acep->a_type == ACE_ACCESS_DENIED_ACE_TYPE) {
		haswriteperm = (acep->a_access_mask & ACE_WRITE_DATA) ? 0 : 1;
		hasreadperm = (acep->a_access_mask & ACE_READ_DATA) ? 0 : 1;
	} else {
		haswriteperm = (acep->a_access_mask & ACE_WRITE_DATA) ? 1 : 0;
		hasreadperm = (acep->a_access_mask & ACE_READ_DATA) ? 1 : 0;
	}

	acl_consume = (ACL_SYNCHRONIZE_ERR_DENY |
	    ACL_DELETE_ERR_DENY |
	    ACL_WRITE_OWNER_ERR_DENY |
	    ACL_WRITE_OWNER_ERR_ALLOW |
	    ACL_WRITE_ATTRS_OWNER_SET_ALLOW |
	    ACL_WRITE_ATTRS_OWNER_ERR_DENY |
	    ACL_WRITE_ATTRS_WRITER_SET_DENY |
	    ACL_WRITE_ATTRS_WRITER_ERR_ALLOW |
	    ACL_WRITE_NAMED_WRITER_ERR_DENY |
	    ACL_READ_NAMED_READER_ERR_DENY);

	if (mask_bit == ACE_SYNCHRONIZE) {
		set_deny = ACL_SYNCHRONIZE_SET_DENY;
		err_deny =  ACL_SYNCHRONIZE_ERR_DENY;
		set_allow = ACL_SYNCHRONIZE_SET_ALLOW;
		err_allow = ACL_SYNCHRONIZE_ERR_ALLOW;
	} else if (mask_bit == ACE_WRITE_OWNER) {
		set_deny = ACL_WRITE_OWNER_SET_DENY;
		err_deny =  ACL_WRITE_OWNER_ERR_DENY;
		set_allow = ACL_WRITE_OWNER_SET_ALLOW;
		err_allow = ACL_WRITE_OWNER_ERR_ALLOW;
	} else if (mask_bit == ACE_DELETE) {
		set_deny = ACL_DELETE_SET_DENY;
		err_deny =  ACL_DELETE_ERR_DENY;
		set_allow = ACL_DELETE_SET_ALLOW;
		err_allow = ACL_DELETE_ERR_ALLOW;
	} else if (mask_bit == ACE_WRITE_ATTRIBUTES) {
		if (isowner) {
			set_deny = ACL_WRITE_ATTRS_OWNER_SET_DENY;
			err_deny =  ACL_WRITE_ATTRS_OWNER_ERR_DENY;
			set_allow = ACL_WRITE_ATTRS_OWNER_SET_ALLOW;
			err_allow = ACL_WRITE_ATTRS_OWNER_ERR_ALLOW;
		} else if (haswriteperm) {
			set_deny = ACL_WRITE_ATTRS_WRITER_SET_DENY;
			err_deny =  ACL_WRITE_ATTRS_WRITER_ERR_DENY;
			set_allow = ACL_WRITE_ATTRS_WRITER_SET_ALLOW;
			err_allow = ACL_WRITE_ATTRS_WRITER_ERR_ALLOW;
		} else {
			if ((acep->a_access_mask & mask_bit) &&
			    (acep->a_type & ACE_ACCESS_ALLOWED_ACE_TYPE)) {
				return (ENOTSUP);
			}
			return (0);
		}
	} else if (mask_bit == ACE_READ_NAMED_ATTRS) {
		if (!hasreadperm)
			return (0);

		set_deny = ACL_READ_NAMED_READER_SET_DENY;
		err_deny = ACL_READ_NAMED_READER_ERR_DENY;
		set_allow = ACL_READ_NAMED_READER_SET_ALLOW;
		err_allow = ACL_READ_NAMED_READER_ERR_ALLOW;
	} else if (mask_bit == ACE_WRITE_NAMED_ATTRS) {
		if (!haswriteperm)
			return (0);

		set_deny = ACL_WRITE_NAMED_WRITER_SET_DENY;
		err_deny = ACL_WRITE_NAMED_WRITER_ERR_DENY;
		set_allow = ACL_WRITE_NAMED_WRITER_SET_ALLOW;
		err_allow = ACL_WRITE_NAMED_WRITER_ERR_ALLOW;
	} else {
		return (EINVAL);
	}

	if (acep->a_type == ACE_ACCESS_DENIED_ACE_TYPE) {
		if (acl_consume & set_deny) {
			if (!(acep->a_access_mask & mask_bit)) {
				return (ENOTSUP);
			}
		} else if (acl_consume & err_deny) {
			if (acep->a_access_mask & mask_bit) {
				return (ENOTSUP);
			}
		}
	} else {
		 
		if (acl_consume & set_allow) {
			if (!(acep->a_access_mask & mask_bit)) {
				return (ENOTSUP);
			}
		} else if (acl_consume & err_allow) {
			if (acep->a_access_mask & mask_bit) {
				return (ENOTSUP);
			}
		}
	}
	return (0);
}

static int
ace_to_aent_legal(ace_t *acep)
{
	int error = 0;
	int isowner;

	 
	if ((acep->a_type != ACE_ACCESS_ALLOWED_ACE_TYPE) &&
	    (acep->a_type != ACE_ACCESS_DENIED_ACE_TYPE)) {
		error = ENOTSUP;
		goto out;
	}

	 
	if (acep->a_flags & ~(ACE_VALID_FLAG_BITS)) {
		error = EINVAL;
		goto out;
	}

	 
	if (acep->a_flags & (ACE_SUCCESSFUL_ACCESS_ACE_FLAG |
	    ACE_FAILED_ACCESS_ACE_FLAG |
	    ACE_NO_PROPAGATE_INHERIT_ACE)) {
		error = ENOTSUP;
		goto out;
	}

	 
	if (acep->a_access_mask & ~(ACE_VALID_MASK_BITS)) {
		error = EINVAL;
		goto out;
	}

	if ((acep->a_flags & ACE_OWNER)) {
		isowner = 1;
	} else {
		isowner = 0;
	}

	error = access_mask_check(acep, ACE_SYNCHRONIZE, isowner);
	if (error)
		goto out;

	error = access_mask_check(acep, ACE_WRITE_OWNER, isowner);
	if (error)
		goto out;

	error = access_mask_check(acep, ACE_DELETE, isowner);
	if (error)
		goto out;

	error = access_mask_check(acep, ACE_WRITE_ATTRIBUTES, isowner);
	if (error)
		goto out;

	error = access_mask_check(acep, ACE_READ_NAMED_ATTRS, isowner);
	if (error)
		goto out;

	error = access_mask_check(acep, ACE_WRITE_NAMED_ATTRS, isowner);
	if (error)
		goto out;

	 
	if (acep->a_type == ACE_ACCESS_ALLOWED_ACE_TYPE) {
		if (! (acep->a_access_mask & ACE_READ_ATTRIBUTES)) {
			error = ENOTSUP;
			goto out;
		}
		if ((acep->a_access_mask & ACE_WRITE_DATA) &&
		    (! (acep->a_access_mask & ACE_APPEND_DATA))) {
			error = ENOTSUP;
			goto out;
		}
		if ((! (acep->a_access_mask & ACE_WRITE_DATA)) &&
		    (acep->a_access_mask & ACE_APPEND_DATA)) {
			error = ENOTSUP;
			goto out;
		}
	}

	 
	if ((acep->a_access_mask & ACE_READ_ACL) &&
	    (acep->a_type != ACE_ACCESS_ALLOWED_ACE_TYPE)) {
		error = ENOTSUP;
		goto out;
	}
	if (acep->a_access_mask & ACE_WRITE_ACL) {
		if ((acep->a_type == ACE_ACCESS_DENIED_ACE_TYPE) &&
		    (isowner)) {
			error = ENOTSUP;
			goto out;
		}
		if ((acep->a_type == ACE_ACCESS_ALLOWED_ACE_TYPE) &&
		    (! isowner)) {
			error = ENOTSUP;
			goto out;
		}
	}

out:
	return (error);
}

static int
ace_allow_to_mode(uint32_t mask, o_mode_t *modep, boolean_t isdir)
{
	 
	if ((mask & (ACE_READ_ACL | ACE_READ_ATTRIBUTES)) !=
	    (ACE_READ_ACL | ACE_READ_ATTRIBUTES)) {
		return (ENOTSUP);
	}

	return (ace_mask_to_mode(mask, modep, isdir));
}

static int
acevals_to_aent(acevals_t *vals, aclent_t *dest, ace_list_t *list,
    uid_t owner, gid_t group, boolean_t isdir)
{
	int error;
	uint32_t  flips = ACE_POSIX_SUPPORTED_BITS;

	if (isdir)
		flips |= ACE_DELETE_CHILD;
	if (vals->allowed != (vals->denied ^ flips)) {
		error = ENOTSUP;
		goto out;
	}
	if ((list->hasmask) && (list->acl_mask != vals->mask) &&
	    (vals->aent_type & (USER | GROUP | GROUP_OBJ))) {
		error = ENOTSUP;
		goto out;
	}
	error = ace_allow_to_mode(vals->allowed, &dest->a_perm, isdir);
	if (error != 0)
		goto out;
	dest->a_type = vals->aent_type;
	if (dest->a_type & (USER | GROUP)) {
		dest->a_id = vals->key;
	} else if (dest->a_type & USER_OBJ) {
		dest->a_id = owner;
	} else if (dest->a_type & GROUP_OBJ) {
		dest->a_id = group;
	} else if (dest->a_type & OTHER_OBJ) {
		dest->a_id = 0;
	} else {
		error = EINVAL;
		goto out;
	}

out:
	return (error);
}


static int
ace_list_to_aent(ace_list_t *list, aclent_t **aclentp, int *aclcnt,
    uid_t owner, gid_t group, boolean_t isdir)
{
	int error = 0;
	aclent_t *aent, *result = NULL;
	acevals_t *vals;
	int resultcount;

	if ((list->seen & (USER_OBJ | GROUP_OBJ | OTHER_OBJ)) !=
	    (USER_OBJ | GROUP_OBJ | OTHER_OBJ)) {
		error = ENOTSUP;
		goto out;
	}
	if ((! list->hasmask) && (list->numusers + list->numgroups > 0)) {
		error = ENOTSUP;
		goto out;
	}

	resultcount = 3 + list->numusers + list->numgroups;
	 
	if ((list->hasmask) || (! list->dfacl_flag))
		resultcount += 1;

	if (cacl_malloc((void **)&result,
	    resultcount * sizeof (aclent_t)) != 0) {
		error = ENOMEM;
		goto out;
	}
	aent = result;

	 
	if (!(list->user_obj.aent_type & USER_OBJ)) {
		error = EINVAL;
		goto out;
	}

	error = acevals_to_aent(&list->user_obj, aent, list, owner, group,
	    isdir);

	if (error != 0)
		goto out;
	++aent;
	 
	vals = NULL;
	for (vals = avl_first(&list->user); vals != NULL;
	    vals = AVL_NEXT(&list->user, vals)) {
		if (!(vals->aent_type & USER)) {
			error = EINVAL;
			goto out;
		}
		error = acevals_to_aent(vals, aent, list, owner, group,
		    isdir);
		if (error != 0)
			goto out;
		++aent;
	}
	 
	if (!(list->group_obj.aent_type & GROUP_OBJ)) {
		error = EINVAL;
		goto out;
	}
	error = acevals_to_aent(&list->group_obj, aent, list, owner, group,
	    isdir);
	if (error != 0)
		goto out;
	++aent;
	 
	vals = NULL;
	for (vals = avl_first(&list->group); vals != NULL;
	    vals = AVL_NEXT(&list->group, vals)) {
		if (!(vals->aent_type & GROUP)) {
			error = EINVAL;
			goto out;
		}
		error = acevals_to_aent(vals, aent, list, owner, group,
		    isdir);
		if (error != 0)
			goto out;
		++aent;
	}
	 
	if ((list->hasmask) || (! list->dfacl_flag)) {
		if (list->hasmask) {
			uint32_t flips = ACE_POSIX_SUPPORTED_BITS;
			if (isdir)
				flips |= ACE_DELETE_CHILD;
			error = ace_mask_to_mode(list->acl_mask ^ flips,
			    &aent->a_perm, isdir);
			if (error != 0)
				goto out;
		} else {
			 
			error = ace_mask_to_mode(list->group_obj.allowed,
			    &aent->a_perm, isdir);
			if (error != 0)
				goto out;
		}
		aent->a_id = 0;
		aent->a_type = CLASS_OBJ | list->dfacl_flag;
		++aent;
	}
	 
	if (!(list->other_obj.aent_type & OTHER_OBJ)) {
		error = EINVAL;
		goto out;
	}
	error = acevals_to_aent(&list->other_obj, aent, list, owner, group,
	    isdir);
	if (error != 0)
		goto out;
	++aent;

	*aclentp = result;
	*aclcnt = resultcount;

out:
	if (error != 0) {
		if (result != NULL)
			cacl_free(result, resultcount * sizeof (aclent_t));
	}

	return (error);
}


 
static void
ace_list_free(ace_list_t *al)
{
	acevals_t *node;
	void *cookie;

	if (al == NULL)
		return;

	cookie = NULL;
	while ((node = avl_destroy_nodes(&al->user, &cookie)) != NULL)
		cacl_free(node, sizeof (acevals_t));
	cookie = NULL;
	while ((node = avl_destroy_nodes(&al->group, &cookie)) != NULL)
		cacl_free(node, sizeof (acevals_t));

	avl_destroy(&al->user);
	avl_destroy(&al->group);

	 
	cacl_free(al, sizeof (ace_list_t));
}

static int
acevals_compare(const void *va, const void *vb)
{
	const acevals_t *a = va, *b = vb;

	if (a->key == b->key)
		return (0);

	if (a->key > b->key)
		return (1);

	else
		return (-1);
}

 
static int
ln_ace_to_aent(ace_t *ace, int n, uid_t owner, gid_t group,
    aclent_t **aclentp, int *aclcnt, aclent_t **dfaclentp, int *dfaclcnt,
    boolean_t isdir)
{
	int error = 0;
	ace_t *acep;
	uint32_t bits;
	int i;
	ace_list_t *normacl = NULL, *dfacl = NULL, *acl;
	acevals_t *vals;

	*aclentp = NULL;
	*aclcnt = 0;
	*dfaclentp = NULL;
	*dfaclcnt = 0;

	 
	if (n < 6) {
		error = ENOTSUP;
		goto out;
	}
	if (ace == NULL) {
		error = EINVAL;
		goto out;
	}

	error = cacl_malloc((void **)&normacl, sizeof (ace_list_t));
	if (error != 0)
		goto out;

	avl_create(&normacl->user, acevals_compare, sizeof (acevals_t),
	    offsetof(acevals_t, avl));
	avl_create(&normacl->group, acevals_compare, sizeof (acevals_t),
	    offsetof(acevals_t, avl));

	ace_list_init(normacl, 0);

	error = cacl_malloc((void **)&dfacl, sizeof (ace_list_t));
	if (error != 0)
		goto out;

	avl_create(&dfacl->user, acevals_compare, sizeof (acevals_t),
	    offsetof(acevals_t, avl));
	avl_create(&dfacl->group, acevals_compare, sizeof (acevals_t),
	    offsetof(acevals_t, avl));
	ace_list_init(dfacl, ACL_DEFAULT);

	 
	for (i = 0; i < n; i++) {
		acep = &ace[i];

		 
		error = ace_to_aent_legal(acep);
		if (error != 0)
			goto out;

		 
		acep->a_access_mask &= ~(ACE_WRITE_OWNER | ACE_DELETE |
		    ACE_SYNCHRONIZE | ACE_WRITE_ATTRIBUTES |
		    ACE_READ_NAMED_ATTRS | ACE_WRITE_NAMED_ATTRS);

		 
		bits = acep->a_flags &
		    (ACE_INHERIT_ONLY_ACE |
		    ACE_FILE_INHERIT_ACE |
		    ACE_DIRECTORY_INHERIT_ACE);
		if (bits != 0) {
			 
			if (bits != (ACE_INHERIT_ONLY_ACE |
			    ACE_FILE_INHERIT_ACE |
			    ACE_DIRECTORY_INHERIT_ACE)) {
				error = ENOTSUP;
				goto out;
			}
			acl = dfacl;
		} else {
			acl = normacl;
		}

		if ((acep->a_flags & ACE_OWNER)) {
			if (acl->state > ace_user_obj) {
				error = ENOTSUP;
				goto out;
			}
			acl->state = ace_user_obj;
			acl->seen |= USER_OBJ;
			vals = &acl->user_obj;
			vals->aent_type = USER_OBJ | acl->dfacl_flag;
		} else if ((acep->a_flags & ACE_EVERYONE)) {
			acl->state = ace_other_obj;
			acl->seen |= OTHER_OBJ;
			vals = &acl->other_obj;
			vals->aent_type = OTHER_OBJ | acl->dfacl_flag;
		} else if (acep->a_flags & ACE_IDENTIFIER_GROUP) {
			if (acl->state > ace_group) {
				error = ENOTSUP;
				goto out;
			}
			if ((acep->a_flags & ACE_GROUP)) {
				acl->seen |= GROUP_OBJ;
				vals = &acl->group_obj;
				vals->aent_type = GROUP_OBJ | acl->dfacl_flag;
			} else {
				acl->seen |= GROUP;
				vals = acevals_find(acep, &acl->group,
				    &acl->numgroups);
				if (vals == NULL) {
					error = ENOMEM;
					goto out;
				}
				vals->aent_type = GROUP | acl->dfacl_flag;
			}
			acl->state = ace_group;
		} else {
			if (acl->state > ace_user) {
				error = ENOTSUP;
				goto out;
			}
			acl->state = ace_user;
			acl->seen |= USER;
			vals = acevals_find(acep, &acl->user,
			    &acl->numusers);
			if (vals == NULL) {
				error = ENOMEM;
				goto out;
			}
			vals->aent_type = USER | acl->dfacl_flag;
		}

		if (!(acl->state > ace_unused)) {
			error = EINVAL;
			goto out;
		}

		if (acep->a_type == ACE_ACCESS_ALLOWED_ACE_TYPE) {
			 
			if (vals->allowed != ACE_MASK_UNDEFINED) {
				error = ENOTSUP;
				goto out;
			}
			vals->allowed = acep->a_access_mask;
		} else {
			 
			if (vals->denied != ACE_MASK_UNDEFINED) {
				 
				if ((acl->state != ace_user) &&
				    (acl->state != ace_group)) {
					error = ENOTSUP;
					goto out;
				}

				if (! acl->hasmask) {
					acl->hasmask = 1;
					acl->acl_mask = vals->denied;
				 
				} else if (acl->acl_mask != vals->denied) {
					error = ENOTSUP;
					goto out;
				}
				vals->mask = vals->denied;
			}
			vals->denied = acep->a_access_mask;
		}
	}

	 
	if (normacl->state != ace_unused) {
		error = ace_list_to_aent(normacl, aclentp, aclcnt,
		    owner, group, isdir);
		if (error != 0) {
			goto out;
		}
	}
	if (dfacl->state != ace_unused) {
		error = ace_list_to_aent(dfacl, dfaclentp, dfaclcnt,
		    owner, group, isdir);
		if (error != 0) {
			goto out;
		}
	}

out:
	if (normacl != NULL)
		ace_list_free(normacl);
	if (dfacl != NULL)
		ace_list_free(dfacl);

	return (error);
}

static int
convert_ace_to_aent(ace_t *acebufp, int acecnt, boolean_t isdir,
    uid_t owner, gid_t group, aclent_t **retaclentp, int *retaclcnt)
{
	int error = 0;
	aclent_t *aclentp, *dfaclentp;
	int aclcnt, dfaclcnt;
	int aclsz, dfaclsz;

	error = ln_ace_to_aent(acebufp, acecnt, owner, group,
	    &aclentp, &aclcnt, &dfaclentp, &dfaclcnt, isdir);

	if (error)
		return (error);


	if (dfaclcnt != 0) {
		 
		aclsz = sizeof (aclent_t) * aclcnt;
		dfaclsz = sizeof (aclent_t) * dfaclcnt;
		aclentp = cacl_realloc(aclentp, aclsz, aclsz + dfaclsz);
		if (aclentp != NULL) {
			(void) memcpy(aclentp + aclcnt, dfaclentp, dfaclsz);
		} else {
			error = ENOMEM;
		}
	}

	if (aclentp) {
		*retaclentp = aclentp;
		*retaclcnt = aclcnt + dfaclcnt;
	}

	if (dfaclentp)
		cacl_free(dfaclentp, dfaclsz);

	return (error);
}


int
acl_translate(acl_t *aclp, int target_flavor, boolean_t isdir, uid_t owner,
    gid_t group)
{
	int aclcnt;
	void *acldata;
	int error;

	 
	if ((target_flavor == _ACL_ACE_ENABLED && aclp->acl_type == ACE_T) ||
	    (target_flavor == _ACL_ACLENT_ENABLED &&
	    aclp->acl_type == ACLENT_T))
		return (0);

	if (target_flavor == -1) {
		error = EINVAL;
		goto out;
	}

	if (target_flavor ==  _ACL_ACE_ENABLED &&
	    aclp->acl_type == ACLENT_T) {
		error = convert_aent_to_ace(aclp->acl_aclp,
		    aclp->acl_cnt, isdir, (ace_t **)&acldata, &aclcnt);
		if (error)
			goto out;

	} else if (target_flavor == _ACL_ACLENT_ENABLED &&
	    aclp->acl_type == ACE_T) {
		error = convert_ace_to_aent(aclp->acl_aclp, aclp->acl_cnt,
		    isdir, owner, group, (aclent_t **)&acldata, &aclcnt);
		if (error)
			goto out;
	} else {
		error = ENOTSUP;
		goto out;
	}

	 
	cacl_free(aclp->acl_aclp, aclp->acl_cnt * aclp->acl_entry_size);
	aclp->acl_aclp = acldata;
	aclp->acl_cnt = aclcnt;
	if (target_flavor == _ACL_ACE_ENABLED) {
		aclp->acl_type = ACE_T;
		aclp->acl_entry_size = sizeof (ace_t);
	} else {
		aclp->acl_type = ACLENT_T;
		aclp->acl_entry_size = sizeof (aclent_t);
	}
	return (0);

out:

#if !defined(_KERNEL)
	errno = error;
	return (-1);
#else
	return (error);
#endif
}
#endif  

#define	SET_ACE(acl, index, who, mask, type, flags) { \
	acl[0][index].a_who = (uint32_t)who; \
	acl[0][index].a_type = type; \
	acl[0][index].a_flags = flags; \
	acl[0][index++].a_access_mask = mask; \
}

void
acl_trivial_access_masks(mode_t mode, boolean_t isdir, trivial_acl_t *masks)
{
	uint32_t read_mask = ACE_READ_DATA;
	uint32_t write_mask = ACE_WRITE_DATA|ACE_APPEND_DATA;
	uint32_t execute_mask = ACE_EXECUTE;

	(void) isdir;	 

	masks->deny1 = 0;
	if (!(mode & S_IRUSR) && (mode & (S_IRGRP|S_IROTH)))
		masks->deny1 |= read_mask;
	if (!(mode & S_IWUSR) && (mode & (S_IWGRP|S_IWOTH)))
		masks->deny1 |= write_mask;
	if (!(mode & S_IXUSR) && (mode & (S_IXGRP|S_IXOTH)))
		masks->deny1 |= execute_mask;

	masks->deny2 = 0;
	if (!(mode & S_IRGRP) && (mode & S_IROTH))
		masks->deny2 |= read_mask;
	if (!(mode & S_IWGRP) && (mode & S_IWOTH))
		masks->deny2 |= write_mask;
	if (!(mode & S_IXGRP) && (mode & S_IXOTH))
		masks->deny2 |= execute_mask;

	masks->allow0 = 0;
	if ((mode & S_IRUSR) && (!(mode & S_IRGRP) && (mode & S_IROTH)))
		masks->allow0 |= read_mask;
	if ((mode & S_IWUSR) && (!(mode & S_IWGRP) && (mode & S_IWOTH)))
		masks->allow0 |= write_mask;
	if ((mode & S_IXUSR) && (!(mode & S_IXGRP) && (mode & S_IXOTH)))
		masks->allow0 |= execute_mask;

	masks->owner = ACE_WRITE_ATTRIBUTES|ACE_WRITE_OWNER|ACE_WRITE_ACL|
	    ACE_WRITE_NAMED_ATTRS|ACE_READ_ACL|ACE_READ_ATTRIBUTES|
	    ACE_READ_NAMED_ATTRS|ACE_SYNCHRONIZE;
	if (mode & S_IRUSR)
		masks->owner |= read_mask;
	if (mode & S_IWUSR)
		masks->owner |= write_mask;
	if (mode & S_IXUSR)
		masks->owner |= execute_mask;

	masks->group = ACE_READ_ACL|ACE_READ_ATTRIBUTES|ACE_READ_NAMED_ATTRS|
	    ACE_SYNCHRONIZE;
	if (mode & S_IRGRP)
		masks->group |= read_mask;
	if (mode & S_IWGRP)
		masks->group |= write_mask;
	if (mode & S_IXGRP)
		masks->group |= execute_mask;

	masks->everyone = ACE_READ_ACL|ACE_READ_ATTRIBUTES|ACE_READ_NAMED_ATTRS|
	    ACE_SYNCHRONIZE;
	if (mode & S_IROTH)
		masks->everyone |= read_mask;
	if (mode & S_IWOTH)
		masks->everyone |= write_mask;
	if (mode & S_IXOTH)
		masks->everyone |= execute_mask;
}

int
acl_trivial_create(mode_t mode, boolean_t isdir, ace_t **acl, int *count)
{
	int		index = 0;
	int		error;
	trivial_acl_t	masks;

	*count = 3;
	acl_trivial_access_masks(mode, isdir, &masks);

	if (masks.allow0)
		(*count)++;
	if (masks.deny1)
		(*count)++;
	if (masks.deny2)
		(*count)++;

	if ((error = cacl_malloc((void **)acl, *count * sizeof (ace_t))) != 0)
		return (error);

	if (masks.allow0) {
		SET_ACE(acl, index, -1, masks.allow0,
		    ACE_ACCESS_ALLOWED_ACE_TYPE, ACE_OWNER);
	}
	if (masks.deny1) {
		SET_ACE(acl, index, -1, masks.deny1,
		    ACE_ACCESS_DENIED_ACE_TYPE, ACE_OWNER);
	}
	if (masks.deny2) {
		SET_ACE(acl, index, -1, masks.deny2,
		    ACE_ACCESS_DENIED_ACE_TYPE, ACE_GROUP|ACE_IDENTIFIER_GROUP);
	}

	SET_ACE(acl, index, -1, masks.owner, ACE_ACCESS_ALLOWED_ACE_TYPE,
	    ACE_OWNER);
	SET_ACE(acl, index, -1, masks.group, ACE_ACCESS_ALLOWED_ACE_TYPE,
	    ACE_IDENTIFIER_GROUP|ACE_GROUP);
	SET_ACE(acl, index, -1, masks.everyone, ACE_ACCESS_ALLOWED_ACE_TYPE,
	    ACE_EVERYONE);

	return (0);
}

 
int
ace_trivial_common(void *acep, int aclcnt,
    uintptr_t (*walk)(void *, uintptr_t, int aclcnt,
    uint16_t *, uint16_t *, uint32_t *))
{
	uint16_t flags;
	uint32_t mask;
	uint16_t type;
	uintptr_t cookie = 0;

	while ((cookie = walk(acep, cookie, aclcnt, &flags, &type, &mask))) {
		switch (flags & ACE_TYPE_FLAGS) {
		case ACE_OWNER:
		case ACE_GROUP|ACE_IDENTIFIER_GROUP:
		case ACE_EVERYONE:
			break;
		default:
			return (1);

		}

		if (flags & (ACE_FILE_INHERIT_ACE|
		    ACE_DIRECTORY_INHERIT_ACE|ACE_NO_PROPAGATE_INHERIT_ACE|
		    ACE_INHERIT_ONLY_ACE))
			return (1);

		 
		if ((mask & (ACE_READ_ACL|ACE_READ_ATTRIBUTES)) &&
		    (type == ACE_ACCESS_DENIED_ACE_TYPE))
			return (1);

		 
		if (mask & (ACE_DELETE|ACE_DELETE_CHILD))
			return (1);
		 
		if (type == ACE_ACCESS_ALLOWED_ACE_TYPE &&
		    (!(flags & ACE_OWNER) && (mask &
		    (ACE_WRITE_OWNER|ACE_WRITE_ACL| ACE_WRITE_ATTRIBUTES|
		    ACE_WRITE_NAMED_ATTRS))))
			return (1);

	}
	return (0);
}
