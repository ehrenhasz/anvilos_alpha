 
 

#ifndef API_H
#define API_H

#include <sys/types.h>
#include <signal.h>

#include "openbsd-compat/sys-queue.h"

#include "cipher.h"
#include "sshkey.h"
#include "kex.h"
#include "ssh.h"
#include "ssh2.h"
#include "packet.h"

struct kex_params {
	char *proposal[PROPOSAL_MAX];
};

 

 
int	ssh_init(struct ssh **, int is_server, struct kex_params *kex_params);

 
void	ssh_free(struct ssh *);

 
void	ssh_set_app_data(struct ssh *, void *);
void	*ssh_get_app_data(struct ssh *);

 
int	ssh_add_hostkey(struct ssh *ssh, struct sshkey *key);

 
int	ssh_set_verify_host_key_callback(struct ssh *ssh,
    int (*cb)(struct sshkey *, struct ssh *));

 
int	ssh_packet_next(struct ssh *ssh, u_char *typep);

 
const u_char	*ssh_packet_payload(struct ssh *ssh, size_t *lenp);

 
int	ssh_packet_put(struct ssh *ssh, int type, const u_char *data,
    size_t len);

 
int	ssh_input_space(struct ssh *ssh, size_t len);

 
int	ssh_input_append(struct ssh *ssh, const u_char *data, size_t len);

 
int	ssh_output_space(struct ssh *ssh, size_t len);

 
const u_char	*ssh_output_ptr(struct ssh *ssh, size_t *len);

 
int	ssh_output_consume(struct ssh *ssh, size_t len);

#endif
