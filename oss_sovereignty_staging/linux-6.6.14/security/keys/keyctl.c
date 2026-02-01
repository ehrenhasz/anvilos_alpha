
 

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/key.h>
#include <linux/keyctl.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/vmalloc.h>
#include <linux/security.h>
#include <linux/uio.h>
#include <linux/uaccess.h>
#include <keys/request_key_auth-type.h>
#include "internal.h"

#define KEY_MAX_DESC_SIZE 4096

static const unsigned char keyrings_capabilities[2] = {
	[0] = (KEYCTL_CAPS0_CAPABILITIES |
	       (IS_ENABLED(CONFIG_PERSISTENT_KEYRINGS)	? KEYCTL_CAPS0_PERSISTENT_KEYRINGS : 0) |
	       (IS_ENABLED(CONFIG_KEY_DH_OPERATIONS)	? KEYCTL_CAPS0_DIFFIE_HELLMAN : 0) |
	       (IS_ENABLED(CONFIG_ASYMMETRIC_KEY_TYPE)	? KEYCTL_CAPS0_PUBLIC_KEY : 0) |
	       (IS_ENABLED(CONFIG_BIG_KEYS)		? KEYCTL_CAPS0_BIG_KEY : 0) |
	       KEYCTL_CAPS0_INVALIDATE |
	       KEYCTL_CAPS0_RESTRICT_KEYRING |
	       KEYCTL_CAPS0_MOVE
	       ),
	[1] = (KEYCTL_CAPS1_NS_KEYRING_NAME |
	       KEYCTL_CAPS1_NS_KEY_TAG |
	       (IS_ENABLED(CONFIG_KEY_NOTIFICATIONS)	? KEYCTL_CAPS1_NOTIFICATIONS : 0)
	       ),
};

static int key_get_type_from_user(char *type,
				  const char __user *_type,
				  unsigned len)
{
	int ret;

	ret = strncpy_from_user(type, _type, len);
	if (ret < 0)
		return ret;
	if (ret == 0 || ret >= len)
		return -EINVAL;
	if (type[0] == '.')
		return -EPERM;
	type[len - 1] = '\0';
	return 0;
}

 
SYSCALL_DEFINE5(add_key, const char __user *, _type,
		const char __user *, _description,
		const void __user *, _payload,
		size_t, plen,
		key_serial_t, ringid)
{
	key_ref_t keyring_ref, key_ref;
	char type[32], *description;
	void *payload;
	long ret;

	ret = -EINVAL;
	if (plen > 1024 * 1024 - 1)
		goto error;

	 
	ret = key_get_type_from_user(type, _type, sizeof(type));
	if (ret < 0)
		goto error;

	description = NULL;
	if (_description) {
		description = strndup_user(_description, KEY_MAX_DESC_SIZE);
		if (IS_ERR(description)) {
			ret = PTR_ERR(description);
			goto error;
		}
		if (!*description) {
			kfree(description);
			description = NULL;
		} else if ((description[0] == '.') &&
			   (strncmp(type, "keyring", 7) == 0)) {
			ret = -EPERM;
			goto error2;
		}
	}

	 
	payload = NULL;

	if (plen) {
		ret = -ENOMEM;
		payload = kvmalloc(plen, GFP_KERNEL);
		if (!payload)
			goto error2;

		ret = -EFAULT;
		if (copy_from_user(payload, _payload, plen) != 0)
			goto error3;
	}

	 
	keyring_ref = lookup_user_key(ringid, KEY_LOOKUP_CREATE, KEY_NEED_WRITE);
	if (IS_ERR(keyring_ref)) {
		ret = PTR_ERR(keyring_ref);
		goto error3;
	}

	 
	key_ref = key_create_or_update(keyring_ref, type, description,
				       payload, plen, KEY_PERM_UNDEF,
				       KEY_ALLOC_IN_QUOTA);
	if (!IS_ERR(key_ref)) {
		ret = key_ref_to_ptr(key_ref)->serial;
		key_ref_put(key_ref);
	}
	else {
		ret = PTR_ERR(key_ref);
	}

	key_ref_put(keyring_ref);
 error3:
	kvfree_sensitive(payload, plen);
 error2:
	kfree(description);
 error:
	return ret;
}

 
SYSCALL_DEFINE4(request_key, const char __user *, _type,
		const char __user *, _description,
		const char __user *, _callout_info,
		key_serial_t, destringid)
{
	struct key_type *ktype;
	struct key *key;
	key_ref_t dest_ref;
	size_t callout_len;
	char type[32], *description, *callout_info;
	long ret;

	 
	ret = key_get_type_from_user(type, _type, sizeof(type));
	if (ret < 0)
		goto error;

	 
	description = strndup_user(_description, KEY_MAX_DESC_SIZE);
	if (IS_ERR(description)) {
		ret = PTR_ERR(description);
		goto error;
	}

	 
	callout_info = NULL;
	callout_len = 0;
	if (_callout_info) {
		callout_info = strndup_user(_callout_info, PAGE_SIZE);
		if (IS_ERR(callout_info)) {
			ret = PTR_ERR(callout_info);
			goto error2;
		}
		callout_len = strlen(callout_info);
	}

	 
	dest_ref = NULL;
	if (destringid) {
		dest_ref = lookup_user_key(destringid, KEY_LOOKUP_CREATE,
					   KEY_NEED_WRITE);
		if (IS_ERR(dest_ref)) {
			ret = PTR_ERR(dest_ref);
			goto error3;
		}
	}

	 
	ktype = key_type_lookup(type);
	if (IS_ERR(ktype)) {
		ret = PTR_ERR(ktype);
		goto error4;
	}

	 
	key = request_key_and_link(ktype, description, NULL, callout_info,
				   callout_len, NULL, key_ref_to_ptr(dest_ref),
				   KEY_ALLOC_IN_QUOTA);
	if (IS_ERR(key)) {
		ret = PTR_ERR(key);
		goto error5;
	}

	 
	ret = wait_for_key_construction(key, 1);
	if (ret < 0)
		goto error6;

	ret = key->serial;

error6:
 	key_put(key);
error5:
	key_type_put(ktype);
error4:
	key_ref_put(dest_ref);
error3:
	kfree(callout_info);
error2:
	kfree(description);
error:
	return ret;
}

 
long keyctl_get_keyring_ID(key_serial_t id, int create)
{
	key_ref_t key_ref;
	unsigned long lflags;
	long ret;

	lflags = create ? KEY_LOOKUP_CREATE : 0;
	key_ref = lookup_user_key(id, lflags, KEY_NEED_SEARCH);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);
		goto error;
	}

	ret = key_ref_to_ptr(key_ref)->serial;
	key_ref_put(key_ref);
error:
	return ret;
}

 
long keyctl_join_session_keyring(const char __user *_name)
{
	char *name;
	long ret;

	 
	name = NULL;
	if (_name) {
		name = strndup_user(_name, KEY_MAX_DESC_SIZE);
		if (IS_ERR(name)) {
			ret = PTR_ERR(name);
			goto error;
		}

		ret = -EPERM;
		if (name[0] == '.')
			goto error_name;
	}

	 
	ret = join_session_keyring(name);
error_name:
	kfree(name);
error:
	return ret;
}

 
long keyctl_update_key(key_serial_t id,
		       const void __user *_payload,
		       size_t plen)
{
	key_ref_t key_ref;
	void *payload;
	long ret;

	ret = -EINVAL;
	if (plen > PAGE_SIZE)
		goto error;

	 
	payload = NULL;
	if (plen) {
		ret = -ENOMEM;
		payload = kvmalloc(plen, GFP_KERNEL);
		if (!payload)
			goto error;

		ret = -EFAULT;
		if (copy_from_user(payload, _payload, plen) != 0)
			goto error2;
	}

	 
	key_ref = lookup_user_key(id, 0, KEY_NEED_WRITE);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);
		goto error2;
	}

	 
	ret = key_update(key_ref, payload, plen);

	key_ref_put(key_ref);
error2:
	kvfree_sensitive(payload, plen);
error:
	return ret;
}

 
long keyctl_revoke_key(key_serial_t id)
{
	key_ref_t key_ref;
	struct key *key;
	long ret;

	key_ref = lookup_user_key(id, 0, KEY_NEED_WRITE);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);
		if (ret != -EACCES)
			goto error;
		key_ref = lookup_user_key(id, 0, KEY_NEED_SETATTR);
		if (IS_ERR(key_ref)) {
			ret = PTR_ERR(key_ref);
			goto error;
		}
	}

	key = key_ref_to_ptr(key_ref);
	ret = 0;
	if (test_bit(KEY_FLAG_KEEP, &key->flags))
		ret = -EPERM;
	else
		key_revoke(key);

	key_ref_put(key_ref);
error:
	return ret;
}

 
long keyctl_invalidate_key(key_serial_t id)
{
	key_ref_t key_ref;
	struct key *key;
	long ret;

	kenter("%d", id);

	key_ref = lookup_user_key(id, 0, KEY_NEED_SEARCH);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);

		 
		if (capable(CAP_SYS_ADMIN)) {
			key_ref = lookup_user_key(id, 0, KEY_SYSADMIN_OVERRIDE);
			if (IS_ERR(key_ref))
				goto error;
			if (test_bit(KEY_FLAG_ROOT_CAN_INVAL,
				     &key_ref_to_ptr(key_ref)->flags))
				goto invalidate;
			goto error_put;
		}

		goto error;
	}

invalidate:
	key = key_ref_to_ptr(key_ref);
	ret = 0;
	if (test_bit(KEY_FLAG_KEEP, &key->flags))
		ret = -EPERM;
	else
		key_invalidate(key);
error_put:
	key_ref_put(key_ref);
error:
	kleave(" = %ld", ret);
	return ret;
}

 
long keyctl_keyring_clear(key_serial_t ringid)
{
	key_ref_t keyring_ref;
	struct key *keyring;
	long ret;

	keyring_ref = lookup_user_key(ringid, KEY_LOOKUP_CREATE, KEY_NEED_WRITE);
	if (IS_ERR(keyring_ref)) {
		ret = PTR_ERR(keyring_ref);

		 
		if (capable(CAP_SYS_ADMIN)) {
			keyring_ref = lookup_user_key(ringid, 0,
						      KEY_SYSADMIN_OVERRIDE);
			if (IS_ERR(keyring_ref))
				goto error;
			if (test_bit(KEY_FLAG_ROOT_CAN_CLEAR,
				     &key_ref_to_ptr(keyring_ref)->flags))
				goto clear;
			goto error_put;
		}

		goto error;
	}

clear:
	keyring = key_ref_to_ptr(keyring_ref);
	if (test_bit(KEY_FLAG_KEEP, &keyring->flags))
		ret = -EPERM;
	else
		ret = keyring_clear(keyring);
error_put:
	key_ref_put(keyring_ref);
error:
	return ret;
}

 
long keyctl_keyring_link(key_serial_t id, key_serial_t ringid)
{
	key_ref_t keyring_ref, key_ref;
	long ret;

	keyring_ref = lookup_user_key(ringid, KEY_LOOKUP_CREATE, KEY_NEED_WRITE);
	if (IS_ERR(keyring_ref)) {
		ret = PTR_ERR(keyring_ref);
		goto error;
	}

	key_ref = lookup_user_key(id, KEY_LOOKUP_CREATE, KEY_NEED_LINK);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);
		goto error2;
	}

	ret = key_link(key_ref_to_ptr(keyring_ref), key_ref_to_ptr(key_ref));

	key_ref_put(key_ref);
error2:
	key_ref_put(keyring_ref);
error:
	return ret;
}

 
long keyctl_keyring_unlink(key_serial_t id, key_serial_t ringid)
{
	key_ref_t keyring_ref, key_ref;
	struct key *keyring, *key;
	long ret;

	keyring_ref = lookup_user_key(ringid, 0, KEY_NEED_WRITE);
	if (IS_ERR(keyring_ref)) {
		ret = PTR_ERR(keyring_ref);
		goto error;
	}

	key_ref = lookup_user_key(id, KEY_LOOKUP_PARTIAL, KEY_NEED_UNLINK);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);
		goto error2;
	}

	keyring = key_ref_to_ptr(keyring_ref);
	key = key_ref_to_ptr(key_ref);
	if (test_bit(KEY_FLAG_KEEP, &keyring->flags) &&
	    test_bit(KEY_FLAG_KEEP, &key->flags))
		ret = -EPERM;
	else
		ret = key_unlink(keyring, key);

	key_ref_put(key_ref);
error2:
	key_ref_put(keyring_ref);
error:
	return ret;
}

 
long keyctl_keyring_move(key_serial_t id, key_serial_t from_ringid,
			 key_serial_t to_ringid, unsigned int flags)
{
	key_ref_t key_ref, from_ref, to_ref;
	long ret;

	if (flags & ~KEYCTL_MOVE_EXCL)
		return -EINVAL;

	key_ref = lookup_user_key(id, KEY_LOOKUP_CREATE, KEY_NEED_LINK);
	if (IS_ERR(key_ref))
		return PTR_ERR(key_ref);

	from_ref = lookup_user_key(from_ringid, 0, KEY_NEED_WRITE);
	if (IS_ERR(from_ref)) {
		ret = PTR_ERR(from_ref);
		goto error2;
	}

	to_ref = lookup_user_key(to_ringid, KEY_LOOKUP_CREATE, KEY_NEED_WRITE);
	if (IS_ERR(to_ref)) {
		ret = PTR_ERR(to_ref);
		goto error3;
	}

	ret = key_move(key_ref_to_ptr(key_ref), key_ref_to_ptr(from_ref),
		       key_ref_to_ptr(to_ref), flags);

	key_ref_put(to_ref);
error3:
	key_ref_put(from_ref);
error2:
	key_ref_put(key_ref);
	return ret;
}

 
long keyctl_describe_key(key_serial_t keyid,
			 char __user *buffer,
			 size_t buflen)
{
	struct key *key, *instkey;
	key_ref_t key_ref;
	char *infobuf;
	long ret;
	int desclen, infolen;

	key_ref = lookup_user_key(keyid, KEY_LOOKUP_PARTIAL, KEY_NEED_VIEW);
	if (IS_ERR(key_ref)) {
		 
		if (PTR_ERR(key_ref) == -EACCES) {
			instkey = key_get_instantiation_authkey(keyid);
			if (!IS_ERR(instkey)) {
				key_put(instkey);
				key_ref = lookup_user_key(keyid,
							  KEY_LOOKUP_PARTIAL,
							  KEY_AUTHTOKEN_OVERRIDE);
				if (!IS_ERR(key_ref))
					goto okay;
			}
		}

		ret = PTR_ERR(key_ref);
		goto error;
	}

okay:
	key = key_ref_to_ptr(key_ref);
	desclen = strlen(key->description);

	 
	ret = -ENOMEM;
	infobuf = kasprintf(GFP_KERNEL,
			    "%s;%d;%d;%08x;",
			    key->type->name,
			    from_kuid_munged(current_user_ns(), key->uid),
			    from_kgid_munged(current_user_ns(), key->gid),
			    key->perm);
	if (!infobuf)
		goto error2;
	infolen = strlen(infobuf);
	ret = infolen + desclen + 1;

	 
	if (buffer && buflen >= ret) {
		if (copy_to_user(buffer, infobuf, infolen) != 0 ||
		    copy_to_user(buffer + infolen, key->description,
				 desclen + 1) != 0)
			ret = -EFAULT;
	}

	kfree(infobuf);
error2:
	key_ref_put(key_ref);
error:
	return ret;
}

 
long keyctl_keyring_search(key_serial_t ringid,
			   const char __user *_type,
			   const char __user *_description,
			   key_serial_t destringid)
{
	struct key_type *ktype;
	key_ref_t keyring_ref, key_ref, dest_ref;
	char type[32], *description;
	long ret;

	 
	ret = key_get_type_from_user(type, _type, sizeof(type));
	if (ret < 0)
		goto error;

	description = strndup_user(_description, KEY_MAX_DESC_SIZE);
	if (IS_ERR(description)) {
		ret = PTR_ERR(description);
		goto error;
	}

	 
	keyring_ref = lookup_user_key(ringid, 0, KEY_NEED_SEARCH);
	if (IS_ERR(keyring_ref)) {
		ret = PTR_ERR(keyring_ref);
		goto error2;
	}

	 
	dest_ref = NULL;
	if (destringid) {
		dest_ref = lookup_user_key(destringid, KEY_LOOKUP_CREATE,
					   KEY_NEED_WRITE);
		if (IS_ERR(dest_ref)) {
			ret = PTR_ERR(dest_ref);
			goto error3;
		}
	}

	 
	ktype = key_type_lookup(type);
	if (IS_ERR(ktype)) {
		ret = PTR_ERR(ktype);
		goto error4;
	}

	 
	key_ref = keyring_search(keyring_ref, ktype, description, true);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);

		 
		if (ret == -EAGAIN)
			ret = -ENOKEY;
		goto error5;
	}

	 
	if (dest_ref) {
		ret = key_permission(key_ref, KEY_NEED_LINK);
		if (ret < 0)
			goto error6;

		ret = key_link(key_ref_to_ptr(dest_ref), key_ref_to_ptr(key_ref));
		if (ret < 0)
			goto error6;
	}

	ret = key_ref_to_ptr(key_ref)->serial;

error6:
	key_ref_put(key_ref);
error5:
	key_type_put(ktype);
error4:
	key_ref_put(dest_ref);
error3:
	key_ref_put(keyring_ref);
error2:
	kfree(description);
error:
	return ret;
}

 
static long __keyctl_read_key(struct key *key, char *buffer, size_t buflen)
{
	long ret;

	down_read(&key->sem);
	ret = key_validate(key);
	if (ret == 0)
		ret = key->type->read(key, buffer, buflen);
	up_read(&key->sem);
	return ret;
}

 
long keyctl_read_key(key_serial_t keyid, char __user *buffer, size_t buflen)
{
	struct key *key;
	key_ref_t key_ref;
	long ret;
	char *key_data = NULL;
	size_t key_data_len;

	 
	key_ref = lookup_user_key(keyid, 0, KEY_DEFER_PERM_CHECK);
	if (IS_ERR(key_ref)) {
		ret = -ENOKEY;
		goto out;
	}

	key = key_ref_to_ptr(key_ref);

	ret = key_read_state(key);
	if (ret < 0)
		goto key_put_out;  

	 
	ret = key_permission(key_ref, KEY_NEED_READ);
	if (ret == 0)
		goto can_read_key;
	if (ret != -EACCES)
		goto key_put_out;

	 
	if (!is_key_possessed(key_ref)) {
		ret = -EACCES;
		goto key_put_out;
	}

	 
can_read_key:
	if (!key->type->read) {
		ret = -EOPNOTSUPP;
		goto key_put_out;
	}

	if (!buffer || !buflen) {
		 
		ret = __keyctl_read_key(key, NULL, 0);
		goto key_put_out;
	}

	 
	key_data_len = (buflen <= PAGE_SIZE) ? buflen : 0;
	for (;;) {
		if (key_data_len) {
			key_data = kvmalloc(key_data_len, GFP_KERNEL);
			if (!key_data) {
				ret = -ENOMEM;
				goto key_put_out;
			}
		}

		ret = __keyctl_read_key(key, key_data, key_data_len);

		 
		if (ret <= 0 || ret > buflen)
			break;

		 
		if (ret > key_data_len) {
			if (unlikely(key_data))
				kvfree_sensitive(key_data, key_data_len);
			key_data_len = ret;
			continue;	 
		}

		if (copy_to_user(buffer, key_data, ret))
			ret = -EFAULT;
		break;
	}
	kvfree_sensitive(key_data, key_data_len);

key_put_out:
	key_put(key);
out:
	return ret;
}

 
long keyctl_chown_key(key_serial_t id, uid_t user, gid_t group)
{
	struct key_user *newowner, *zapowner = NULL;
	struct key *key;
	key_ref_t key_ref;
	long ret;
	kuid_t uid;
	kgid_t gid;

	uid = make_kuid(current_user_ns(), user);
	gid = make_kgid(current_user_ns(), group);
	ret = -EINVAL;
	if ((user != (uid_t) -1) && !uid_valid(uid))
		goto error;
	if ((group != (gid_t) -1) && !gid_valid(gid))
		goto error;

	ret = 0;
	if (user == (uid_t) -1 && group == (gid_t) -1)
		goto error;

	key_ref = lookup_user_key(id, KEY_LOOKUP_CREATE | KEY_LOOKUP_PARTIAL,
				  KEY_NEED_SETATTR);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);
		goto error;
	}

	key = key_ref_to_ptr(key_ref);

	 
	ret = -EACCES;
	down_write(&key->sem);

	{
		bool is_privileged_op = false;

		 
		if (user != (uid_t) -1 && !uid_eq(key->uid, uid))
			is_privileged_op = true;

		 
		if (group != (gid_t) -1 && !gid_eq(gid, key->gid) && !in_group_p(gid))
			is_privileged_op = true;

		if (is_privileged_op && !capable(CAP_SYS_ADMIN))
			goto error_put;
	}

	 
	if (user != (uid_t) -1 && !uid_eq(uid, key->uid)) {
		ret = -ENOMEM;
		newowner = key_user_lookup(uid);
		if (!newowner)
			goto error_put;

		 
		if (test_bit(KEY_FLAG_IN_QUOTA, &key->flags)) {
			unsigned maxkeys = uid_eq(uid, GLOBAL_ROOT_UID) ?
				key_quota_root_maxkeys : key_quota_maxkeys;
			unsigned maxbytes = uid_eq(uid, GLOBAL_ROOT_UID) ?
				key_quota_root_maxbytes : key_quota_maxbytes;

			spin_lock(&newowner->lock);
			if (newowner->qnkeys + 1 > maxkeys ||
			    newowner->qnbytes + key->quotalen > maxbytes ||
			    newowner->qnbytes + key->quotalen <
			    newowner->qnbytes)
				goto quota_overrun;

			newowner->qnkeys++;
			newowner->qnbytes += key->quotalen;
			spin_unlock(&newowner->lock);

			spin_lock(&key->user->lock);
			key->user->qnkeys--;
			key->user->qnbytes -= key->quotalen;
			spin_unlock(&key->user->lock);
		}

		atomic_dec(&key->user->nkeys);
		atomic_inc(&newowner->nkeys);

		if (key->state != KEY_IS_UNINSTANTIATED) {
			atomic_dec(&key->user->nikeys);
			atomic_inc(&newowner->nikeys);
		}

		zapowner = key->user;
		key->user = newowner;
		key->uid = uid;
	}

	 
	if (group != (gid_t) -1)
		key->gid = gid;

	notify_key(key, NOTIFY_KEY_SETATTR, 0);
	ret = 0;

error_put:
	up_write(&key->sem);
	key_put(key);
	if (zapowner)
		key_user_put(zapowner);
error:
	return ret;

quota_overrun:
	spin_unlock(&newowner->lock);
	zapowner = newowner;
	ret = -EDQUOT;
	goto error_put;
}

 
long keyctl_setperm_key(key_serial_t id, key_perm_t perm)
{
	struct key *key;
	key_ref_t key_ref;
	long ret;

	ret = -EINVAL;
	if (perm & ~(KEY_POS_ALL | KEY_USR_ALL | KEY_GRP_ALL | KEY_OTH_ALL))
		goto error;

	key_ref = lookup_user_key(id, KEY_LOOKUP_CREATE | KEY_LOOKUP_PARTIAL,
				  KEY_NEED_SETATTR);
	if (IS_ERR(key_ref)) {
		ret = PTR_ERR(key_ref);
		goto error;
	}

	key = key_ref_to_ptr(key_ref);

	 
	ret = -EACCES;
	down_write(&key->sem);

	 
	if (uid_eq(key->uid, current_fsuid()) || capable(CAP_SYS_ADMIN)) {
		key->perm = perm;
		notify_key(key, NOTIFY_KEY_SETATTR, 0);
		ret = 0;
	}

	up_write(&key->sem);
	key_put(key);
error:
	return ret;
}

 
static long get_instantiation_keyring(key_serial_t ringid,
				      struct request_key_auth *rka,
				      struct key **_dest_keyring)
{
	key_ref_t dkref;

	*_dest_keyring = NULL;

	 
	if (ringid == 0)
		return 0;

	 
	if (ringid > 0) {
		dkref = lookup_user_key(ringid, KEY_LOOKUP_CREATE, KEY_NEED_WRITE);
		if (IS_ERR(dkref))
			return PTR_ERR(dkref);
		*_dest_keyring = key_ref_to_ptr(dkref);
		return 0;
	}

	if (ringid == KEY_SPEC_REQKEY_AUTH_KEY)
		return -EINVAL;

	 
	if (ringid >= KEY_SPEC_REQUESTOR_KEYRING) {
		*_dest_keyring = key_get(rka->dest_keyring);
		return 0;
	}

	return -ENOKEY;
}

 
static int keyctl_change_reqkey_auth(struct key *key)
{
	struct cred *new;

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	key_put(new->request_key_auth);
	new->request_key_auth = key_get(key);

	return commit_creds(new);
}

 
static long keyctl_instantiate_key_common(key_serial_t id,
				   struct iov_iter *from,
				   key_serial_t ringid)
{
	const struct cred *cred = current_cred();
	struct request_key_auth *rka;
	struct key *instkey, *dest_keyring;
	size_t plen = from ? iov_iter_count(from) : 0;
	void *payload;
	long ret;

	kenter("%d,,%zu,%d", id, plen, ringid);

	if (!plen)
		from = NULL;

	ret = -EINVAL;
	if (plen > 1024 * 1024 - 1)
		goto error;

	 
	ret = -EPERM;
	instkey = cred->request_key_auth;
	if (!instkey)
		goto error;

	rka = instkey->payload.data[0];
	if (rka->target_key->serial != id)
		goto error;

	 
	payload = NULL;

	if (from) {
		ret = -ENOMEM;
		payload = kvmalloc(plen, GFP_KERNEL);
		if (!payload)
			goto error;

		ret = -EFAULT;
		if (!copy_from_iter_full(payload, plen, from))
			goto error2;
	}

	 
	ret = get_instantiation_keyring(ringid, rka, &dest_keyring);
	if (ret < 0)
		goto error2;

	 
	ret = key_instantiate_and_link(rka->target_key, payload, plen,
				       dest_keyring, instkey);

	key_put(dest_keyring);

	 
	if (ret == 0)
		keyctl_change_reqkey_auth(NULL);

error2:
	kvfree_sensitive(payload, plen);
error:
	return ret;
}

 
long keyctl_instantiate_key(key_serial_t id,
			    const void __user *_payload,
			    size_t plen,
			    key_serial_t ringid)
{
	if (_payload && plen) {
		struct iovec iov;
		struct iov_iter from;
		int ret;

		ret = import_single_range(ITER_SOURCE, (void __user *)_payload, plen,
					  &iov, &from);
		if (unlikely(ret))
			return ret;

		return keyctl_instantiate_key_common(id, &from, ringid);
	}

	return keyctl_instantiate_key_common(id, NULL, ringid);
}

 
long keyctl_instantiate_key_iov(key_serial_t id,
				const struct iovec __user *_payload_iov,
				unsigned ioc,
				key_serial_t ringid)
{
	struct iovec iovstack[UIO_FASTIOV], *iov = iovstack;
	struct iov_iter from;
	long ret;

	if (!_payload_iov)
		ioc = 0;

	ret = import_iovec(ITER_SOURCE, _payload_iov, ioc,
				    ARRAY_SIZE(iovstack), &iov, &from);
	if (ret < 0)
		return ret;
	ret = keyctl_instantiate_key_common(id, &from, ringid);
	kfree(iov);
	return ret;
}

 
long keyctl_negate_key(key_serial_t id, unsigned timeout, key_serial_t ringid)
{
	return keyctl_reject_key(id, timeout, ENOKEY, ringid);
}

 
long keyctl_reject_key(key_serial_t id, unsigned timeout, unsigned error,
		       key_serial_t ringid)
{
	const struct cred *cred = current_cred();
	struct request_key_auth *rka;
	struct key *instkey, *dest_keyring;
	long ret;

	kenter("%d,%u,%u,%d", id, timeout, error, ringid);

	 
	if (error <= 0 ||
	    error >= MAX_ERRNO ||
	    error == ERESTARTSYS ||
	    error == ERESTARTNOINTR ||
	    error == ERESTARTNOHAND ||
	    error == ERESTART_RESTARTBLOCK)
		return -EINVAL;

	 
	ret = -EPERM;
	instkey = cred->request_key_auth;
	if (!instkey)
		goto error;

	rka = instkey->payload.data[0];
	if (rka->target_key->serial != id)
		goto error;

	 
	ret = get_instantiation_keyring(ringid, rka, &dest_keyring);
	if (ret < 0)
		goto error;

	 
	ret = key_reject_and_link(rka->target_key, timeout, error,
				  dest_keyring, instkey);

	key_put(dest_keyring);

	 
	if (ret == 0)
		keyctl_change_reqkey_auth(NULL);

error:
	return ret;
}

 
long keyctl_set_reqkey_keyring(int reqkey_defl)
{
	struct cred *new;
	int ret, old_setting;

	old_setting = current_cred_xxx(jit_keyring);

	if (reqkey_defl == KEY_REQKEY_DEFL_NO_CHANGE)
		return old_setting;

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	switch (reqkey_defl) {
	case KEY_REQKEY_DEFL_THREAD_KEYRING:
		ret = install_thread_keyring_to_cred(new);
		if (ret < 0)
			goto error;
		goto set;

	case KEY_REQKEY_DEFL_PROCESS_KEYRING:
		ret = install_process_keyring_to_cred(new);
		if (ret < 0)
			goto error;
		goto set;

	case KEY_REQKEY_DEFL_DEFAULT:
	case KEY_REQKEY_DEFL_SESSION_KEYRING:
	case KEY_REQKEY_DEFL_USER_KEYRING:
	case KEY_REQKEY_DEFL_USER_SESSION_KEYRING:
	case KEY_REQKEY_DEFL_REQUESTOR_KEYRING:
		goto set;

	case KEY_REQKEY_DEFL_NO_CHANGE:
	case KEY_REQKEY_DEFL_GROUP_KEYRING:
	default:
		ret = -EINVAL;
		goto error;
	}

set:
	new->jit_keyring = reqkey_defl;
	commit_creds(new);
	return old_setting;
error:
	abort_creds(new);
	return ret;
}

 
long keyctl_set_timeout(key_serial_t id, unsigned timeout)
{
	struct key *key, *instkey;
	key_ref_t key_ref;
	long ret;

	key_ref = lookup_user_key(id, KEY_LOOKUP_CREATE | KEY_LOOKUP_PARTIAL,
				  KEY_NEED_SETATTR);
	if (IS_ERR(key_ref)) {
		 
		if (PTR_ERR(key_ref) == -EACCES) {
			instkey = key_get_instantiation_authkey(id);
			if (!IS_ERR(instkey)) {
				key_put(instkey);
				key_ref = lookup_user_key(id,
							  KEY_LOOKUP_PARTIAL,
							  KEY_AUTHTOKEN_OVERRIDE);
				if (!IS_ERR(key_ref))
					goto okay;
			}
		}

		ret = PTR_ERR(key_ref);
		goto error;
	}

okay:
	key = key_ref_to_ptr(key_ref);
	ret = 0;
	if (test_bit(KEY_FLAG_KEEP, &key->flags)) {
		ret = -EPERM;
	} else {
		key_set_timeout(key, timeout);
		notify_key(key, NOTIFY_KEY_SETATTR, 0);
	}
	key_put(key);

error:
	return ret;
}

 
long keyctl_assume_authority(key_serial_t id)
{
	struct key *authkey;
	long ret;

	 
	ret = -EINVAL;
	if (id < 0)
		goto error;

	 
	if (id == 0) {
		ret = keyctl_change_reqkey_auth(NULL);
		goto error;
	}

	 
	authkey = key_get_instantiation_authkey(id);
	if (IS_ERR(authkey)) {
		ret = PTR_ERR(authkey);
		goto error;
	}

	ret = keyctl_change_reqkey_auth(authkey);
	if (ret == 0)
		ret = authkey->serial;
	key_put(authkey);
error:
	return ret;
}

 
long keyctl_get_security(key_serial_t keyid,
			 char __user *buffer,
			 size_t buflen)
{
	struct key *key, *instkey;
	key_ref_t key_ref;
	char *context;
	long ret;

	key_ref = lookup_user_key(keyid, KEY_LOOKUP_PARTIAL, KEY_NEED_VIEW);
	if (IS_ERR(key_ref)) {
		if (PTR_ERR(key_ref) != -EACCES)
			return PTR_ERR(key_ref);

		 
		instkey = key_get_instantiation_authkey(keyid);
		if (IS_ERR(instkey))
			return PTR_ERR(instkey);
		key_put(instkey);

		key_ref = lookup_user_key(keyid, KEY_LOOKUP_PARTIAL,
					  KEY_AUTHTOKEN_OVERRIDE);
		if (IS_ERR(key_ref))
			return PTR_ERR(key_ref);
	}

	key = key_ref_to_ptr(key_ref);
	ret = security_key_getsecurity(key, &context);
	if (ret == 0) {
		 
		ret = 1;
		if (buffer && buflen > 0 &&
		    copy_to_user(buffer, "", 1) != 0)
			ret = -EFAULT;
	} else if (ret > 0) {
		 
		if (buffer && buflen > 0) {
			if (buflen > ret)
				buflen = ret;

			if (copy_to_user(buffer, context, buflen) != 0)
				ret = -EFAULT;
		}

		kfree(context);
	}

	key_ref_put(key_ref);
	return ret;
}

 
long keyctl_session_to_parent(void)
{
	struct task_struct *me, *parent;
	const struct cred *mycred, *pcred;
	struct callback_head *newwork, *oldwork;
	key_ref_t keyring_r;
	struct cred *cred;
	int ret;

	keyring_r = lookup_user_key(KEY_SPEC_SESSION_KEYRING, 0, KEY_NEED_LINK);
	if (IS_ERR(keyring_r))
		return PTR_ERR(keyring_r);

	ret = -ENOMEM;

	 
	cred = cred_alloc_blank();
	if (!cred)
		goto error_keyring;
	newwork = &cred->rcu;

	cred->session_keyring = key_ref_to_ptr(keyring_r);
	keyring_r = NULL;
	init_task_work(newwork, key_change_session_keyring);

	me = current;
	rcu_read_lock();
	write_lock_irq(&tasklist_lock);

	ret = -EPERM;
	oldwork = NULL;
	parent = rcu_dereference_protected(me->real_parent,
					   lockdep_is_held(&tasklist_lock));

	 
	if (parent->pid <= 1 || !parent->mm)
		goto unlock;

	 
	if (!thread_group_empty(parent))
		goto unlock;

	 
	mycred = current_cred();
	pcred = __task_cred(parent);
	if (mycred == pcred ||
	    mycred->session_keyring == pcred->session_keyring) {
		ret = 0;
		goto unlock;
	}

	 
	if (!uid_eq(pcred->uid,	 mycred->euid) ||
	    !uid_eq(pcred->euid, mycred->euid) ||
	    !uid_eq(pcred->suid, mycred->euid) ||
	    !gid_eq(pcred->gid,	 mycred->egid) ||
	    !gid_eq(pcred->egid, mycred->egid) ||
	    !gid_eq(pcred->sgid, mycred->egid))
		goto unlock;

	 
	if ((pcred->session_keyring &&
	     !uid_eq(pcred->session_keyring->uid, mycred->euid)) ||
	    !uid_eq(mycred->session_keyring->uid, mycred->euid))
		goto unlock;

	 
	oldwork = task_work_cancel(parent, key_change_session_keyring);

	 
	ret = task_work_add(parent, newwork, TWA_RESUME);
	if (!ret)
		newwork = NULL;
unlock:
	write_unlock_irq(&tasklist_lock);
	rcu_read_unlock();
	if (oldwork)
		put_cred(container_of(oldwork, struct cred, rcu));
	if (newwork)
		put_cred(cred);
	return ret;

error_keyring:
	key_ref_put(keyring_r);
	return ret;
}

 
long keyctl_restrict_keyring(key_serial_t id, const char __user *_type,
			     const char __user *_restriction)
{
	key_ref_t key_ref;
	char type[32];
	char *restriction = NULL;
	long ret;

	key_ref = lookup_user_key(id, 0, KEY_NEED_SETATTR);
	if (IS_ERR(key_ref))
		return PTR_ERR(key_ref);

	ret = -EINVAL;
	if (_type) {
		if (!_restriction)
			goto error;

		ret = key_get_type_from_user(type, _type, sizeof(type));
		if (ret < 0)
			goto error;

		restriction = strndup_user(_restriction, PAGE_SIZE);
		if (IS_ERR(restriction)) {
			ret = PTR_ERR(restriction);
			goto error;
		}
	} else {
		if (_restriction)
			goto error;
	}

	ret = keyring_restrict(key_ref, _type ? type : NULL, restriction);
	kfree(restriction);
error:
	key_ref_put(key_ref);
	return ret;
}

#ifdef CONFIG_KEY_NOTIFICATIONS
 
long keyctl_watch_key(key_serial_t id, int watch_queue_fd, int watch_id)
{
	struct watch_queue *wqueue;
	struct watch_list *wlist = NULL;
	struct watch *watch = NULL;
	struct key *key;
	key_ref_t key_ref;
	long ret;

	if (watch_id < -1 || watch_id > 0xff)
		return -EINVAL;

	key_ref = lookup_user_key(id, KEY_LOOKUP_CREATE, KEY_NEED_VIEW);
	if (IS_ERR(key_ref))
		return PTR_ERR(key_ref);
	key = key_ref_to_ptr(key_ref);

	wqueue = get_watch_queue(watch_queue_fd);
	if (IS_ERR(wqueue)) {
		ret = PTR_ERR(wqueue);
		goto err_key;
	}

	if (watch_id >= 0) {
		ret = -ENOMEM;
		if (!key->watchers) {
			wlist = kzalloc(sizeof(*wlist), GFP_KERNEL);
			if (!wlist)
				goto err_wqueue;
			init_watch_list(wlist, NULL);
		}

		watch = kzalloc(sizeof(*watch), GFP_KERNEL);
		if (!watch)
			goto err_wlist;

		init_watch(watch, wqueue);
		watch->id	= key->serial;
		watch->info_id	= (u32)watch_id << WATCH_INFO_ID__SHIFT;

		ret = security_watch_key(key);
		if (ret < 0)
			goto err_watch;

		down_write(&key->sem);
		if (!key->watchers) {
			key->watchers = wlist;
			wlist = NULL;
		}

		ret = add_watch_to_object(watch, key->watchers);
		up_write(&key->sem);

		if (ret == 0)
			watch = NULL;
	} else {
		ret = -EBADSLT;
		if (key->watchers) {
			down_write(&key->sem);
			ret = remove_watch_from_object(key->watchers,
						       wqueue, key_serial(key),
						       false);
			up_write(&key->sem);
		}
	}

err_watch:
	kfree(watch);
err_wlist:
	kfree(wlist);
err_wqueue:
	put_watch_queue(wqueue);
err_key:
	key_put(key);
	return ret;
}
#endif  

 
long keyctl_capabilities(unsigned char __user *_buffer, size_t buflen)
{
	size_t size = buflen;

	if (size > 0) {
		if (size > sizeof(keyrings_capabilities))
			size = sizeof(keyrings_capabilities);
		if (copy_to_user(_buffer, keyrings_capabilities, size) != 0)
			return -EFAULT;
		if (size < buflen &&
		    clear_user(_buffer + size, buflen - size) != 0)
			return -EFAULT;
	}

	return sizeof(keyrings_capabilities);
}

 
SYSCALL_DEFINE5(keyctl, int, option, unsigned long, arg2, unsigned long, arg3,
		unsigned long, arg4, unsigned long, arg5)
{
	switch (option) {
	case KEYCTL_GET_KEYRING_ID:
		return keyctl_get_keyring_ID((key_serial_t) arg2,
					     (int) arg3);

	case KEYCTL_JOIN_SESSION_KEYRING:
		return keyctl_join_session_keyring((const char __user *) arg2);

	case KEYCTL_UPDATE:
		return keyctl_update_key((key_serial_t) arg2,
					 (const void __user *) arg3,
					 (size_t) arg4);

	case KEYCTL_REVOKE:
		return keyctl_revoke_key((key_serial_t) arg2);

	case KEYCTL_DESCRIBE:
		return keyctl_describe_key((key_serial_t) arg2,
					   (char __user *) arg3,
					   (unsigned) arg4);

	case KEYCTL_CLEAR:
		return keyctl_keyring_clear((key_serial_t) arg2);

	case KEYCTL_LINK:
		return keyctl_keyring_link((key_serial_t) arg2,
					   (key_serial_t) arg3);

	case KEYCTL_UNLINK:
		return keyctl_keyring_unlink((key_serial_t) arg2,
					     (key_serial_t) arg3);

	case KEYCTL_SEARCH:
		return keyctl_keyring_search((key_serial_t) arg2,
					     (const char __user *) arg3,
					     (const char __user *) arg4,
					     (key_serial_t) arg5);

	case KEYCTL_READ:
		return keyctl_read_key((key_serial_t) arg2,
				       (char __user *) arg3,
				       (size_t) arg4);

	case KEYCTL_CHOWN:
		return keyctl_chown_key((key_serial_t) arg2,
					(uid_t) arg3,
					(gid_t) arg4);

	case KEYCTL_SETPERM:
		return keyctl_setperm_key((key_serial_t) arg2,
					  (key_perm_t) arg3);

	case KEYCTL_INSTANTIATE:
		return keyctl_instantiate_key((key_serial_t) arg2,
					      (const void __user *) arg3,
					      (size_t) arg4,
					      (key_serial_t) arg5);

	case KEYCTL_NEGATE:
		return keyctl_negate_key((key_serial_t) arg2,
					 (unsigned) arg3,
					 (key_serial_t) arg4);

	case KEYCTL_SET_REQKEY_KEYRING:
		return keyctl_set_reqkey_keyring(arg2);

	case KEYCTL_SET_TIMEOUT:
		return keyctl_set_timeout((key_serial_t) arg2,
					  (unsigned) arg3);

	case KEYCTL_ASSUME_AUTHORITY:
		return keyctl_assume_authority((key_serial_t) arg2);

	case KEYCTL_GET_SECURITY:
		return keyctl_get_security((key_serial_t) arg2,
					   (char __user *) arg3,
					   (size_t) arg4);

	case KEYCTL_SESSION_TO_PARENT:
		return keyctl_session_to_parent();

	case KEYCTL_REJECT:
		return keyctl_reject_key((key_serial_t) arg2,
					 (unsigned) arg3,
					 (unsigned) arg4,
					 (key_serial_t) arg5);

	case KEYCTL_INSTANTIATE_IOV:
		return keyctl_instantiate_key_iov(
			(key_serial_t) arg2,
			(const struct iovec __user *) arg3,
			(unsigned) arg4,
			(key_serial_t) arg5);

	case KEYCTL_INVALIDATE:
		return keyctl_invalidate_key((key_serial_t) arg2);

	case KEYCTL_GET_PERSISTENT:
		return keyctl_get_persistent((uid_t)arg2, (key_serial_t)arg3);

	case KEYCTL_DH_COMPUTE:
		return keyctl_dh_compute((struct keyctl_dh_params __user *) arg2,
					 (char __user *) arg3, (size_t) arg4,
					 (struct keyctl_kdf_params __user *) arg5);

	case KEYCTL_RESTRICT_KEYRING:
		return keyctl_restrict_keyring((key_serial_t) arg2,
					       (const char __user *) arg3,
					       (const char __user *) arg4);

	case KEYCTL_PKEY_QUERY:
		if (arg3 != 0)
			return -EINVAL;
		return keyctl_pkey_query((key_serial_t)arg2,
					 (const char __user *)arg4,
					 (struct keyctl_pkey_query __user *)arg5);

	case KEYCTL_PKEY_ENCRYPT:
	case KEYCTL_PKEY_DECRYPT:
	case KEYCTL_PKEY_SIGN:
		return keyctl_pkey_e_d_s(
			option,
			(const struct keyctl_pkey_params __user *)arg2,
			(const char __user *)arg3,
			(const void __user *)arg4,
			(void __user *)arg5);

	case KEYCTL_PKEY_VERIFY:
		return keyctl_pkey_verify(
			(const struct keyctl_pkey_params __user *)arg2,
			(const char __user *)arg3,
			(const void __user *)arg4,
			(const void __user *)arg5);

	case KEYCTL_MOVE:
		return keyctl_keyring_move((key_serial_t)arg2,
					   (key_serial_t)arg3,
					   (key_serial_t)arg4,
					   (unsigned int)arg5);

	case KEYCTL_CAPABILITIES:
		return keyctl_capabilities((unsigned char __user *)arg2, (size_t)arg3);

	case KEYCTL_WATCH_KEY:
		return keyctl_watch_key((key_serial_t)arg2, (int)arg3, (int)arg4);

	default:
		return -EOPNOTSUPP;
	}
}
