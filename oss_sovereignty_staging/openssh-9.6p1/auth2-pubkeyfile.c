 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ssh.h"
#include "log.h"
#include "misc.h"
#include "sshkey.h"
#include "digest.h"
#include "hostfile.h"
#include "auth.h"
#include "auth-options.h"
#include "authfile.h"
#include "match.h"
#include "ssherr.h"

int
auth_authorise_keyopts(struct passwd *pw, struct sshauthopt *opts,
    int allow_cert_authority, const char *remote_ip, const char *remote_host,
    const char *loc)
{
	time_t now = time(NULL);
	char buf[64];

	 
	if (opts->valid_before && now > 0 &&
	    opts->valid_before < (uint64_t)now) {
		format_absolute_time(opts->valid_before, buf, sizeof(buf));
		debug("%s: entry expired at %s", loc, buf);
		auth_debug_add("%s: entry expired at %s", loc, buf);
		return -1;
	}
	 
	if (opts->cert_principals != NULL && !opts->cert_authority) {
		debug("%s: principals on non-CA key", loc);
		auth_debug_add("%s: principals on non-CA key", loc);
		 
		return -1;
	}
	 
	if (!allow_cert_authority && opts->cert_authority) {
		debug("%s: cert-authority flag invalid here", loc);
		auth_debug_add("%s: cert-authority flag invalid here", loc);
		 
		return -1;
	}

	 
	if (opts->required_from_host_keys != NULL) {
		switch (match_host_and_ip(remote_host, remote_ip,
		    opts->required_from_host_keys )) {
		case 1:
			 
			break;
		case -1:
		default:
			debug("%s: invalid from criteria", loc);
			auth_debug_add("%s: invalid from criteria", loc);
			 
		case 0:
			logit("%s: Authentication tried for %.100s with "
			    "correct key but not from a permitted "
			    "host (host=%.200s, ip=%.200s, required=%.200s).",
			    loc, pw->pw_name, remote_host, remote_ip,
			    opts->required_from_host_keys);
			auth_debug_add("%s: Your host '%.200s' is not "
			    "permitted to use this key for login.",
			    loc, remote_host);
			 
			return -1;
		}
	}
	 
	if (opts->required_from_host_cert != NULL) {
		switch (addr_match_cidr_list(remote_ip,
		    opts->required_from_host_cert)) {
		case 1:
			 
			break;
		case -1:
		default:
			 
			error("%s: Certificate source-address invalid", loc);
			 
		case 0:
			logit("%s: Authentication tried for %.100s with valid "
			    "certificate but not from a permitted source "
			    "address (%.200s).", loc, pw->pw_name, remote_ip);
			auth_debug_add("%s: Your address '%.200s' is not "
			    "permitted to use this certificate for login.",
			    loc, remote_ip);
			return -1;
		}
	}
	 
	auth_log_authopts(loc, opts, 1);

	return 0;
}

static int
match_principals_option(const char *principal_list, struct sshkey_cert *cert)
{
	char *result;
	u_int i;

	 

	for (i = 0; i < cert->nprincipals; i++) {
		if ((result = match_list(cert->principals[i],
		    principal_list, NULL)) != NULL) {
			debug3("matched principal from key options \"%.100s\"",
			    result);
			free(result);
			return 1;
		}
	}
	return 0;
}

 
int
auth_check_principals_line(char *cp, const struct sshkey_cert *cert,
    const char *loc, struct sshauthopt **authoptsp)
{
	u_int i, found = 0;
	char *ep, *line_opts;
	const char *reason = NULL;
	struct sshauthopt *opts = NULL;

	if (authoptsp != NULL)
		*authoptsp = NULL;

	 
	ep = cp + strlen(cp) - 1;
	while (ep > cp && (*ep == '\n' || *ep == ' ' || *ep == '\t'))
		*ep-- = '\0';

	 
	line_opts = NULL;
	if ((ep = strrchr(cp, ' ')) != NULL ||
	    (ep = strrchr(cp, '\t')) != NULL) {
		for (; *ep == ' ' || *ep == '\t'; ep++)
			;
		line_opts = cp;
		cp = ep;
	}
	if ((opts = sshauthopt_parse(line_opts, &reason)) == NULL) {
		debug("%s: bad principals options: %s", loc, reason);
		auth_debug_add("%s: bad principals options: %s", loc, reason);
		return -1;
	}
	 
	for (i = 0; i < cert->nprincipals; i++) {
		if (strcmp(cp, cert->principals[i]) != 0)
			continue;
		debug3("%s: matched principal \"%.100s\"",
		    loc, cert->principals[i]);
		found = 1;
	}
	if (found && authoptsp != NULL) {
		*authoptsp = opts;
		opts = NULL;
	}
	sshauthopt_free(opts);
	return found ? 0 : -1;
}

int
auth_process_principals(FILE *f, const char *file,
    const struct sshkey_cert *cert, struct sshauthopt **authoptsp)
{
	char loc[256], *line = NULL, *cp, *ep;
	size_t linesize = 0;
	u_long linenum = 0, nonblank = 0;
	u_int found_principal = 0;

	if (authoptsp != NULL)
		*authoptsp = NULL;

	while (getline(&line, &linesize, f) != -1) {
		linenum++;
		 
		if (found_principal)
			continue;

		 
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		 
		if ((ep = strchr(cp, '#')) != NULL)
			*ep = '\0';
		if (!*cp || *cp == '\n')
			continue;

		nonblank++;
		snprintf(loc, sizeof(loc), "%.200s:%lu", file, linenum);
		if (auth_check_principals_line(cp, cert, loc, authoptsp) == 0)
			found_principal = 1;
	}
	debug2_f("%s: processed %lu/%lu lines", file, nonblank, linenum);
	free(line);
	return found_principal;
}

 
int
auth_check_authkey_line(struct passwd *pw, struct sshkey *key,
    char *cp, const char *remote_ip, const char *remote_host, const char *loc,
    struct sshauthopt **authoptsp)
{
	int want_keytype = sshkey_is_cert(key) ? KEY_UNSPEC : key->type;
	struct sshkey *found = NULL;
	struct sshauthopt *keyopts = NULL, *certopts = NULL, *finalopts = NULL;
	char *key_options = NULL, *fp = NULL;
	const char *reason = NULL;
	int ret = -1;

	if (authoptsp != NULL)
		*authoptsp = NULL;

	if ((found = sshkey_new(want_keytype)) == NULL) {
		debug3_f("keytype %d failed", want_keytype);
		goto out;
	}

	 

	if (sshkey_read(found, &cp) != 0) {
		 
		debug2("%s: check options: '%s'", loc, cp);
		key_options = cp;
		if (sshkey_advance_past_options(&cp) != 0) {
			reason = "invalid key option string";
			goto fail_reason;
		}
		skip_space(&cp);
		if (sshkey_read(found, &cp) != 0) {
			 
			debug2("%s: advance: '%s'", loc, cp);
			goto out;
		}
	}
	 
	if ((keyopts = sshauthopt_parse(key_options, &reason)) == NULL) {
		debug("%s: bad key options: %s", loc, reason);
		auth_debug_add("%s: bad key options: %s", loc, reason);
		goto out;
	}
	 
	if (sshkey_is_cert(key)) {
		 
		if (!sshkey_equal(found, key->cert->signature_key) ||
		    !keyopts->cert_authority)
			goto out;
	} else {
		 
		if (!sshkey_equal(found, key) || keyopts->cert_authority)
			goto out;
	}

	 
	if ((fp = sshkey_fingerprint(found,
	    SSH_FP_HASH_DEFAULT, SSH_FP_DEFAULT)) == NULL)
		fatal_f("fingerprint failed");

	debug("%s: matching %s found: %s %s", loc,
	    sshkey_is_cert(key) ? "CA" : "key", sshkey_type(found), fp);

	if (auth_authorise_keyopts(pw, keyopts,
	    sshkey_is_cert(key), remote_ip, remote_host, loc) != 0) {
		reason = "Refused by key options";
		goto fail_reason;
	}
	 
	if (!sshkey_is_cert(key)) {
		verbose("Accepted key %s %s found at %s",
		    sshkey_type(found), fp, loc);
		finalopts = keyopts;
		keyopts = NULL;
		goto success;
	}

	 

	 
	if ((certopts = sshauthopt_from_cert(key)) == NULL) {
		reason = "Invalid certificate options";
		goto fail_reason;
	}
	if (auth_authorise_keyopts(pw, certopts, 0,
	    remote_ip, remote_host, loc) != 0) {
		reason = "Refused by certificate options";
		goto fail_reason;
	}
	if ((finalopts = sshauthopt_merge(keyopts, certopts, &reason)) == NULL)
		goto fail_reason;

	 
	if (keyopts->cert_principals != NULL &&
	    !match_principals_option(keyopts->cert_principals, key->cert)) {
		reason = "Certificate does not contain an authorized principal";
		goto fail_reason;
	}
	if (sshkey_cert_check_authority_now(key, 0, 0, 0,
	    keyopts->cert_principals == NULL ? pw->pw_name : NULL,
	    &reason) != 0)
		goto fail_reason;

	verbose("Accepted certificate ID \"%s\" (serial %llu) "
	    "signed by CA %s %s found at %s",
	    key->cert->key_id,
	    (unsigned long long)key->cert->serial,
	    sshkey_type(found), fp, loc);

 success:
	if (finalopts == NULL)
		fatal_f("internal error: missing options");
	if (authoptsp != NULL) {
		*authoptsp = finalopts;
		finalopts = NULL;
	}
	 
	ret = 0;
	goto out;

 fail_reason:
	error("%s", reason);
	auth_debug_add("%s", reason);
 out:
	free(fp);
	sshauthopt_free(keyopts);
	sshauthopt_free(certopts);
	sshauthopt_free(finalopts);
	sshkey_free(found);
	return ret;
}

 
int
auth_check_authkeys_file(struct passwd *pw, FILE *f, char *file,
    struct sshkey *key, const char *remote_ip,
    const char *remote_host, struct sshauthopt **authoptsp)
{
	char *cp, *line = NULL, loc[256];
	size_t linesize = 0;
	int found_key = 0;
	u_long linenum = 0, nonblank = 0;

	if (authoptsp != NULL)
		*authoptsp = NULL;

	while (getline(&line, &linesize, f) != -1) {
		linenum++;
		 
		if (found_key)
			continue;

		 
		cp = line;
		skip_space(&cp);
		if (!*cp || *cp == '\n' || *cp == '#')
			continue;

		nonblank++;
		snprintf(loc, sizeof(loc), "%.200s:%lu", file, linenum);
		if (auth_check_authkey_line(pw, key, cp,
		    remote_ip, remote_host, loc, authoptsp) == 0)
			found_key = 1;
	}
	free(line);
	debug2_f("%s: processed %lu/%lu lines", file, nonblank, linenum);
	return found_key;
}

static FILE *
auth_openfile(const char *file, struct passwd *pw, int strict_modes,
    int log_missing, char *file_type)
{
	char line[1024];
	struct stat st;
	int fd;
	FILE *f;

	if ((fd = open(file, O_RDONLY|O_NONBLOCK)) == -1) {
		if (errno != ENOENT) {
			logit("Could not open user '%s' %s '%s': %s",
			    pw->pw_name, file_type, file, strerror(errno));
		} else if (log_missing) {
			debug("Could not open user '%s' %s '%s': %s",
			    pw->pw_name, file_type, file, strerror(errno));
		}
		return NULL;
	}

	if (fstat(fd, &st) == -1) {
		close(fd);
		return NULL;
	}
	if (!S_ISREG(st.st_mode)) {
		logit("User '%s' %s '%s' is not a regular file",
		    pw->pw_name, file_type, file);
		close(fd);
		return NULL;
	}
	unset_nonblock(fd);
	if ((f = fdopen(fd, "r")) == NULL) {
		close(fd);
		return NULL;
	}
	if (strict_modes &&
	    safe_path_fd(fileno(f), file, pw, line, sizeof(line)) != 0) {
		fclose(f);
		logit("Authentication refused: %s", line);
		auth_debug_add("Ignored %s: %s", file_type, line);
		return NULL;
	}

	return f;
}


FILE *
auth_openkeyfile(const char *file, struct passwd *pw, int strict_modes)
{
	return auth_openfile(file, pw, strict_modes, 1, "authorized keys");
}

FILE *
auth_openprincipals(const char *file, struct passwd *pw, int strict_modes)
{
	return auth_openfile(file, pw, strict_modes, 0,
	    "authorized principals");
}

