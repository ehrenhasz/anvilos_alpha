 
 

#include "includes.h"

#include <fcntl.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include "openbsd-compat/openssl-compat.h"
#endif

#include "xmalloc.h"
#include "log.h"
#include "sshkey.h"
#include "ssh.h"
#include "ssh2.h"
#include "misc.h"
#include "sshbuf.h"
#include "authfile.h"
#include "msg.h"
#include "canohost.h"
#include "pathnames.h"
#include "readconf.h"
#include "uidswap.h"
#include "ssherr.h"

extern char *__progname;

static int
valid_request(struct passwd *pw, char *host, struct sshkey **ret, char **pkalgp,
    u_char *data, size_t datalen)
{
	struct sshbuf *b;
	struct sshkey *key = NULL;
	u_char type, *pkblob;
	char *p;
	size_t blen, len;
	char *pkalg, *luser;
	int r, pktype, fail;

	if (ret != NULL)
		*ret = NULL;
	if (pkalgp != NULL)
		*pkalgp = NULL;
	fail = 0;

	if ((b = sshbuf_from(data, datalen)) == NULL)
		fatal_f("sshbuf_from failed");

	 
	if ((r = sshbuf_get_string(b, NULL, &len)) != 0)
		fatal_fr(r, "parse session ID");
	if (len != 20 &&  
	    len != 32 &&  
	    len != 48 &&  
	    len != 64)    
		fail++;

	if ((r = sshbuf_get_u8(b, &type)) != 0)
		fatal_fr(r, "parse type");
	if (type != SSH2_MSG_USERAUTH_REQUEST)
		fail++;

	 
	if ((r = sshbuf_skip_string(b)) != 0)
		fatal_fr(r, "parse user");

	 
	if ((r = sshbuf_get_cstring(b, &p, NULL)) != 0)
		fatal_fr(r, "parse service");
	if (strcmp("ssh-connection", p) != 0)
		fail++;
	free(p);

	 
	if ((r = sshbuf_get_cstring(b, &p, NULL)) != 0)
		fatal_fr(r, "parse method");
	if (strcmp("hostbased", p) != 0)
		fail++;
	free(p);

	 
	if ((r = sshbuf_get_cstring(b, &pkalg, NULL)) != 0 ||
	    (r = sshbuf_get_string(b, &pkblob, &blen)) != 0)
		fatal_fr(r, "parse pk");

	pktype = sshkey_type_from_name(pkalg);
	if (pktype == KEY_UNSPEC)
		fail++;
	else if ((r = sshkey_from_blob(pkblob, blen, &key)) != 0) {
		error_fr(r, "decode key");
		fail++;
	} else if (key->type != pktype)
		fail++;

	 
	if ((r = sshbuf_get_cstring(b, &p, &len)) != 0)
		fatal_fr(r, "parse hostname");
	debug2_f("check expect chost %s got %s", host, p);
	if (strlen(host) != len - 1)
		fail++;
	else if (p[len - 1] != '.')
		fail++;
	else if (strncasecmp(host, p, len - 1) != 0)
		fail++;
	free(p);

	 
	if ((r = sshbuf_get_cstring(b, &luser, NULL)) != 0)
		fatal_fr(r, "parse luser");

	if (strcmp(pw->pw_name, luser) != 0)
		fail++;
	free(luser);

	 
	if (sshbuf_len(b) != 0)
		fail++;
	sshbuf_free(b);

	debug3_f("fail %d", fail);

	if (!fail) {
		if (ret != NULL) {
			*ret = key;
			key = NULL;
		}
		if (pkalgp != NULL) {
			*pkalgp = pkalg;
			pkalg = NULL;
		}
	}
	sshkey_free(key);
	free(pkalg);
	free(pkblob);

	return (fail ? -1 : 0);
}

int
main(int argc, char **argv)
{
	struct sshbuf *b;
	Options options;
#define NUM_KEYTYPES 5
	struct sshkey *keys[NUM_KEYTYPES], *key = NULL;
	struct passwd *pw;
	int r, key_fd[NUM_KEYTYPES], i, found, version = 2, fd;
	u_char *signature, *data, rver;
	char *host, *fp, *pkalg;
	size_t slen, dlen;

	if (pledge("stdio rpath getpw dns id", NULL) != 0)
		fatal("%s: pledge: %s", __progname, strerror(errno));

	 
	if ((fd = open(_PATH_DEVNULL, O_RDWR)) < 2)
		exit(1);
	 
	if (fd > 2)
		close(fd);

	i = 0;
	 
	key_fd[i++] = open(_PATH_HOST_DSA_KEY_FILE, O_RDONLY);
	key_fd[i++] = open(_PATH_HOST_ECDSA_KEY_FILE, O_RDONLY);
	key_fd[i++] = open(_PATH_HOST_ED25519_KEY_FILE, O_RDONLY);
	key_fd[i++] = open(_PATH_HOST_XMSS_KEY_FILE, O_RDONLY);
	key_fd[i++] = open(_PATH_HOST_RSA_KEY_FILE, O_RDONLY);

	if ((pw = getpwuid(getuid())) == NULL)
		fatal("getpwuid failed");
	pw = pwcopy(pw);

	permanently_set_uid(pw);

	seed_rng();

#ifdef DEBUG_SSH_KEYSIGN
	log_init("ssh-keysign", SYSLOG_LEVEL_DEBUG3, SYSLOG_FACILITY_AUTH, 0);
#endif

	 
	initialize_options(&options);
	(void)read_config_file(_PATH_HOST_CONFIG_FILE, pw, "", "",
	    &options, 0, NULL);
	(void)fill_default_options(&options);
	if (options.enable_ssh_keysign != 1)
		fatal("ssh-keysign not enabled in %s",
		    _PATH_HOST_CONFIG_FILE);

	if (pledge("stdio dns", NULL) != 0)
		fatal("%s: pledge: %s", __progname, strerror(errno));

	for (i = found = 0; i < NUM_KEYTYPES; i++) {
		if (key_fd[i] != -1)
			found = 1;
	}
	if (found == 0)
		fatal("could not open any host key");

	found = 0;
	for (i = 0; i < NUM_KEYTYPES; i++) {
		keys[i] = NULL;
		if (key_fd[i] == -1)
			continue;
		r = sshkey_load_private_type_fd(key_fd[i], KEY_UNSPEC,
		    NULL, &key, NULL);
		close(key_fd[i]);
		if (r != 0)
			debug_r(r, "parse key %d", i);
		else if (key != NULL) {
			keys[i] = key;
			found = 1;
		}
	}
	if (!found)
		fatal("no hostkey found");

	if ((b = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __progname);
	if (ssh_msg_recv(STDIN_FILENO, b) < 0)
		fatal("%s: ssh_msg_recv failed", __progname);
	if ((r = sshbuf_get_u8(b, &rver)) != 0)
		fatal_r(r, "%s: buffer error", __progname);
	if (rver != version)
		fatal("%s: bad version: received %d, expected %d",
		    __progname, rver, version);
	if ((r = sshbuf_get_u32(b, (u_int *)&fd)) != 0)
		fatal_r(r, "%s: buffer error", __progname);
	if (fd < 0 || fd == STDIN_FILENO || fd == STDOUT_FILENO)
		fatal("%s: bad fd = %d", __progname, fd);
	if ((host = get_local_name(fd)) == NULL)
		fatal("%s: cannot get local name for fd", __progname);

	if ((r = sshbuf_get_string(b, &data, &dlen)) != 0)
		fatal_r(r, "%s: buffer error", __progname);
	if (valid_request(pw, host, &key, &pkalg, data, dlen) < 0)
		fatal("%s: not a valid request", __progname);
	free(host);

	found = 0;
	for (i = 0; i < NUM_KEYTYPES; i++) {
		if (keys[i] != NULL &&
		    sshkey_equal_public(key, keys[i])) {
			found = 1;
			break;
		}
	}
	if (!found) {
		if ((fp = sshkey_fingerprint(key, options.fingerprint_hash,
		    SSH_FP_DEFAULT)) == NULL)
			fatal("%s: sshkey_fingerprint failed", __progname);
		fatal("%s: no matching hostkey found for key %s %s", __progname,
		    sshkey_type(key), fp ? fp : "");
	}

	if ((r = sshkey_sign(keys[i], &signature, &slen, data, dlen,
	    pkalg, NULL, NULL, 0)) != 0)
		fatal_r(r, "%s: sshkey_sign failed", __progname);
	free(data);

	 
	sshbuf_reset(b);
	if ((r = sshbuf_put_string(b, signature, slen)) != 0)
		fatal_r(r, "%s: buffer error", __progname);
	if (ssh_msg_send(STDOUT_FILENO, version, b) == -1)
		fatal("%s: ssh_msg_send failed", __progname);

	return (0);
}
