 
 

 
struct sshbuf *load_file(const char *name);

 
struct sshbuf *load_text_file(const char *name);

 
BIGNUM *load_bignum(const char *name);

 
const BIGNUM *rsa_n(struct sshkey *k);
const BIGNUM *rsa_e(struct sshkey *k);
const BIGNUM *rsa_p(struct sshkey *k);
const BIGNUM *rsa_q(struct sshkey *k);
const BIGNUM *dsa_g(struct sshkey *k);
const BIGNUM *dsa_pub_key(struct sshkey *k);
const BIGNUM *dsa_priv_key(struct sshkey *k);

