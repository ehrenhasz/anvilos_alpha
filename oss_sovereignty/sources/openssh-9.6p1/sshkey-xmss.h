

#ifndef SSHKEY_XMSS_H
#define SSHKEY_XMSS_H

#define XMSS_SHA2_256_W16_H10_NAME	"XMSS_SHA2-256_W16_H10"
#define XMSS_SHA2_256_W16_H16_NAME	"XMSS_SHA2-256_W16_H16"
#define XMSS_SHA2_256_W16_H20_NAME	"XMSS_SHA2-256_W16_H20"
#define XMSS_DEFAULT_NAME		XMSS_SHA2_256_W16_H10_NAME

size_t	 sshkey_xmss_pklen(const struct sshkey *);
size_t	 sshkey_xmss_sklen(const struct sshkey *);
int	 sshkey_xmss_init(struct sshkey *, const char *);
void	 sshkey_xmss_free_state(struct sshkey *);
int	 sshkey_xmss_generate_private_key(struct sshkey *, int);
int	 sshkey_xmss_serialize_state(const struct sshkey *, struct sshbuf *);
int	 sshkey_xmss_serialize_state_opt(const struct sshkey *, struct sshbuf *,
	    enum sshkey_serialize_rep);
int	 sshkey_xmss_serialize_pk_info(const struct sshkey *, struct sshbuf *,
	    enum sshkey_serialize_rep);
int	 sshkey_xmss_deserialize_state(struct sshkey *, struct sshbuf *);
int	 sshkey_xmss_deserialize_state_opt(struct sshkey *, struct sshbuf *);
int	 sshkey_xmss_deserialize_pk_info(struct sshkey *, struct sshbuf *);

int	 sshkey_xmss_siglen(const struct sshkey *, size_t *);
void	*sshkey_xmss_params(const struct sshkey *);
void	*sshkey_xmss_bds_state(const struct sshkey *);
int	 sshkey_xmss_get_state(const struct sshkey *, int);
int	 sshkey_xmss_enable_maxsign(struct sshkey *, u_int32_t);
int	 sshkey_xmss_forward_state(const struct sshkey *, u_int32_t);
int	 sshkey_xmss_update_state(const struct sshkey *, int);
u_int32_t sshkey_xmss_signatures_left(const struct sshkey *);

#endif 
