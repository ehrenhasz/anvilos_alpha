
 

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <keys/user-type.h>
#include <linux/key-type.h>
#include <linux/keyctl.h>
#include <linux/inet.h>
#include "cifsglob.h"
#include "cifs_spnego.h"
#include "cifs_debug.h"
#include "cifsproto.h"
static const struct cred *spnego_cred;

 
static int
cifs_spnego_key_instantiate(struct key *key, struct key_preparsed_payload *prep)
{
	char *payload;
	int ret;

	ret = -ENOMEM;
	payload = kmemdup(prep->data, prep->datalen, GFP_KERNEL);
	if (!payload)
		goto error;

	 
	key->payload.data[0] = payload;
	ret = 0;

error:
	return ret;
}

static void
cifs_spnego_key_destroy(struct key *key)
{
	kfree(key->payload.data[0]);
}


 
struct key_type cifs_spnego_key_type = {
	.name		= "cifs.spnego",
	.instantiate	= cifs_spnego_key_instantiate,
	.destroy	= cifs_spnego_key_destroy,
	.describe	= user_describe,
};

 
#define MAX_VER_STR_LEN		8

 
#define MAX_MECH_STR_LEN	13

 
#define HOST_KEY_LEN		6

 
#define IP_KEY_LEN		5

 
#define UID_KEY_LEN		7

 
#define CREDUID_KEY_LEN		11

 
#define USER_KEY_LEN		6

 
#define PID_KEY_LEN		7

 
struct key *
cifs_get_spnego_key(struct cifs_ses *sesInfo,
		    struct TCP_Server_Info *server)
{
	struct sockaddr_in *sa = (struct sockaddr_in *) &server->dstaddr;
	struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *) &server->dstaddr;
	char *description, *dp;
	size_t desc_len;
	struct key *spnego_key;
	const char *hostname = server->hostname;
	const struct cred *saved_cred;

	 
	desc_len = MAX_VER_STR_LEN +
		   HOST_KEY_LEN + strlen(hostname) +
		   IP_KEY_LEN + INET6_ADDRSTRLEN +
		   MAX_MECH_STR_LEN +
		   UID_KEY_LEN + (sizeof(uid_t) * 2) +
		   CREDUID_KEY_LEN + (sizeof(uid_t) * 2) +
		   PID_KEY_LEN + (sizeof(pid_t) * 2) + 1;

	if (sesInfo->user_name)
		desc_len += USER_KEY_LEN + strlen(sesInfo->user_name);

	spnego_key = ERR_PTR(-ENOMEM);
	description = kzalloc(desc_len, GFP_KERNEL);
	if (description == NULL)
		goto out;

	dp = description;
	 
	spnego_key = ERR_PTR(-EINVAL);
	sprintf(dp, "ver=0x%x;host=%s;", CIFS_SPNEGO_UPCALL_VERSION,
		hostname);
	dp = description + strlen(description);

	 
	if (server->dstaddr.ss_family == AF_INET)
		sprintf(dp, "ip4=%pI4", &sa->sin_addr);
	else if (server->dstaddr.ss_family == AF_INET6)
		sprintf(dp, "ip6=%pI6", &sa6->sin6_addr);
	else
		goto out;

	dp = description + strlen(description);

	 
	if (server->sec_kerberos)
		sprintf(dp, ";sec=krb5");
	else if (server->sec_mskerberos)
		sprintf(dp, ";sec=mskrb5");
	else {
		cifs_dbg(VFS, "unknown or missing server auth type, use krb5\n");
		sprintf(dp, ";sec=krb5");
	}

	dp = description + strlen(description);
	sprintf(dp, ";uid=0x%x",
		from_kuid_munged(&init_user_ns, sesInfo->linux_uid));

	dp = description + strlen(description);
	sprintf(dp, ";creduid=0x%x",
		from_kuid_munged(&init_user_ns, sesInfo->cred_uid));

	if (sesInfo->user_name) {
		dp = description + strlen(description);
		sprintf(dp, ";user=%s", sesInfo->user_name);
	}

	dp = description + strlen(description);
	sprintf(dp, ";pid=0x%x", current->pid);

	cifs_dbg(FYI, "key description = %s\n", description);
	saved_cred = override_creds(spnego_cred);
	spnego_key = request_key(&cifs_spnego_key_type, description, "");
	revert_creds(saved_cred);

#ifdef CONFIG_CIFS_DEBUG2
	if (cifsFYI && !IS_ERR(spnego_key)) {
		struct cifs_spnego_msg *msg = spnego_key->payload.data[0];
		cifs_dump_mem("SPNEGO reply blob:", msg->data, min(1024U,
				msg->secblob_len + msg->sesskey_len));
	}
#endif  

out:
	kfree(description);
	return spnego_key;
}

int
init_cifs_spnego(void)
{
	struct cred *cred;
	struct key *keyring;
	int ret;

	cifs_dbg(FYI, "Registering the %s key type\n",
		 cifs_spnego_key_type.name);

	 

	cred = prepare_kernel_cred(&init_task);
	if (!cred)
		return -ENOMEM;

	keyring = keyring_alloc(".cifs_spnego",
				GLOBAL_ROOT_UID, GLOBAL_ROOT_GID, cred,
				(KEY_POS_ALL & ~KEY_POS_SETATTR) |
				KEY_USR_VIEW | KEY_USR_READ,
				KEY_ALLOC_NOT_IN_QUOTA, NULL, NULL);
	if (IS_ERR(keyring)) {
		ret = PTR_ERR(keyring);
		goto failed_put_cred;
	}

	ret = register_key_type(&cifs_spnego_key_type);
	if (ret < 0)
		goto failed_put_key;

	 
	set_bit(KEY_FLAG_ROOT_CAN_CLEAR, &keyring->flags);
	cred->thread_keyring = keyring;
	cred->jit_keyring = KEY_REQKEY_DEFL_THREAD_KEYRING;
	spnego_cred = cred;

	cifs_dbg(FYI, "cifs spnego keyring: %d\n", key_serial(keyring));
	return 0;

failed_put_key:
	key_put(keyring);
failed_put_cred:
	put_cred(cred);
	return ret;
}

void
exit_cifs_spnego(void)
{
	key_revoke(spnego_cred->thread_keyring);
	unregister_key_type(&cifs_spnego_key_type);
	put_cred(spnego_cred);
	cifs_dbg(FYI, "Unregistered %s key type\n", cifs_spnego_key_type.name);
}
