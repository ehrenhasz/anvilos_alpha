


#ifndef SSHSIG_H
#define SSHSIG_H

struct sshbuf;
struct sshkey;
struct sshsigopt;
struct sshkey_sig_details;

typedef int sshsig_signer(struct sshkey *, u_char **, size_t *,
    const u_char *, size_t, const char *, const char *, const char *,
    u_int, void *);




int sshsig_signb(struct sshkey *key, const char *hashalg,
    const char *sk_provider, const char *sk_pin, const struct sshbuf *message,
    const char *sig_namespace, struct sshbuf **out,
    sshsig_signer *signer, void *signer_ctx);


int sshsig_verifyb(struct sshbuf *signature,
    const struct sshbuf *message, const char *sig_namespace,
    struct sshkey **sign_keyp, struct sshkey_sig_details **sig_details);




int sshsig_sign_fd(struct sshkey *key, const char *hashalg,
    const char *sk_provider, const char *sk_pin,
    int fd, const char *sig_namespace,
    struct sshbuf **out, sshsig_signer *signer, void *signer_ctx);


int sshsig_verify_fd(struct sshbuf *signature, int fd,
    const char *sig_namespace, struct sshkey **sign_keyp,
    struct sshkey_sig_details **sig_details);




int sshsig_armor(const struct sshbuf *blob, struct sshbuf **out);


int sshsig_dearmor(struct sshbuf *sig, struct sshbuf **out);


int sshsig_check_allowed_keys(const char *path, const struct sshkey *sign_key,
    const char *principal, const char *ns, uint64_t verify_time);


struct sshsigopt *sshsigopt_parse(const char *opts,
    const char *path, u_long linenum, const char **errstrp);


void sshsigopt_free(struct sshsigopt *opts);


int sshsig_get_pubkey(struct sshbuf *signature, struct sshkey **pubkey);


int sshsig_find_principals(const char *path, const struct sshkey *sign_key,
    uint64_t verify_time, char **principal);


int sshsig_match_principals(const char *path,
	const char *principal, char ***principalsp, size_t *nprincipalsp);

#endif 
