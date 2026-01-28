



#ifndef _KRL_H
#define _KRL_H



#define KRL_MAGIC		"SSHKRL\n\0"
#define KRL_FORMAT_VERSION	1


#define KRL_SECTION_CERTIFICATES	1
#define KRL_SECTION_EXPLICIT_KEY	2
#define KRL_SECTION_FINGERPRINT_SHA1	3
#define KRL_SECTION_SIGNATURE		4
#define KRL_SECTION_FINGERPRINT_SHA256	5
#define KRL_SECTION_EXTENSION		255


#define KRL_SECTION_CERT_SERIAL_LIST	0x20
#define KRL_SECTION_CERT_SERIAL_RANGE	0x21
#define KRL_SECTION_CERT_SERIAL_BITMAP	0x22
#define KRL_SECTION_CERT_KEY_ID		0x23
#define KRL_SECTION_CERT_EXTENSION	0x39

struct sshkey;
struct sshbuf;
struct ssh_krl;

struct ssh_krl *ssh_krl_init(void);
void ssh_krl_free(struct ssh_krl *krl);
void ssh_krl_set_version(struct ssh_krl *krl, u_int64_t version);
int ssh_krl_set_comment(struct ssh_krl *krl, const char *comment);
int ssh_krl_revoke_cert_by_serial(struct ssh_krl *krl,
    const struct sshkey *ca_key, u_int64_t serial);
int ssh_krl_revoke_cert_by_serial_range(struct ssh_krl *krl,
    const struct sshkey *ca_key, u_int64_t lo, u_int64_t hi);
int ssh_krl_revoke_cert_by_key_id(struct ssh_krl *krl,
    const struct sshkey *ca_key, const char *key_id);
int ssh_krl_revoke_key_explicit(struct ssh_krl *krl, const struct sshkey *key);
int ssh_krl_revoke_key_sha1(struct ssh_krl *krl, const u_char *p, size_t len);
int ssh_krl_revoke_key_sha256(struct ssh_krl *krl, const u_char *p, size_t len);
int ssh_krl_revoke_key(struct ssh_krl *krl, const struct sshkey *key);
int ssh_krl_to_blob(struct ssh_krl *krl, struct sshbuf *buf);
int ssh_krl_from_blob(struct sshbuf *buf, struct ssh_krl **krlp);
int ssh_krl_check_key(struct ssh_krl *krl, const struct sshkey *key);
int ssh_krl_file_contains_key(const char *path, const struct sshkey *key);
int krl_dump(struct ssh_krl *krl, FILE *f);

#endif 

