


#ifndef _SSH_SK_H
#define _SSH_SK_H 1

struct sshbuf;
struct sshkey;
struct sk_option;


#define SSH_SK_HELPER_VERSION		5


#define SSH_SK_HELPER_ERROR		0	
#define SSH_SK_HELPER_SIGN		1
#define SSH_SK_HELPER_ENROLL		2
#define SSH_SK_HELPER_LOAD_RESIDENT	3

struct sshsk_resident_key {
	struct sshkey *key;
	uint8_t *user_id;
	size_t user_id_len;
};


int sshsk_enroll(int type, const char *provider_path, const char *device,
    const char *application, const char *userid, uint8_t flags,
    const char *pin, struct sshbuf *challenge_buf,
    struct sshkey **keyp, struct sshbuf *attest);


int sshsk_sign(const char *provider_path, struct sshkey *key,
    u_char **sigp, size_t *lenp, const u_char *data, size_t datalen,
    u_int compat, const char *pin);


int sshsk_load_resident(const char *provider_path, const char *device,
    const char *pin, u_int flags, struct sshsk_resident_key ***srksp,
    size_t *nsrksp);


void sshsk_free_resident_keys(struct sshsk_resident_key **srks, size_t nsrks);

#endif 

