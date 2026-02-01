 
 

#ifndef SSHMAC_H
#define SSHMAC_H

#include <sys/types.h>

struct sshmac {
	char	*name;
	int	enabled;
	u_int	mac_len;
	u_char	*key;
	u_int	key_len;
	int	type;
	int	etm;		 
	struct ssh_hmac_ctx	*hmac_ctx;
	struct umac_ctx		*umac_ctx;
};

int	 mac_valid(const char *);
char	*mac_alg_list(char);
int	 mac_setup(struct sshmac *, char *);
int	 mac_init(struct sshmac *);
int	 mac_compute(struct sshmac *, u_int32_t, const u_char *, int,
    u_char *, size_t);
int	 mac_check(struct sshmac *, u_int32_t, const u_char *, size_t,
    const u_char *, size_t);
void	 mac_clear(struct sshmac *);

#endif  
