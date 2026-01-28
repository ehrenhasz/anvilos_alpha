

#ifndef _RANDOMS_H
#define _RANDOMS_H

struct sshbuf;

void seed_rng(void);
void rexec_send_rng_seed(struct sshbuf *);
void rexec_recv_rng_seed(struct sshbuf *);

#endif 
