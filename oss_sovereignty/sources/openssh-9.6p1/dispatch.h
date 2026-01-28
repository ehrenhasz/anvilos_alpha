



#ifndef DISPATCH_H
#define DISPATCH_H

#define DISPATCH_MAX	255

enum {
	DISPATCH_BLOCK,
	DISPATCH_NONBLOCK
};

struct ssh;

typedef int dispatch_fn(int, u_int32_t, struct ssh *);

int	dispatch_protocol_error(int, u_int32_t, struct ssh *);
int	dispatch_protocol_ignore(int, u_int32_t, struct ssh *);
void	ssh_dispatch_init(struct ssh *, dispatch_fn *);
void	ssh_dispatch_set(struct ssh *, int, dispatch_fn *);
void	ssh_dispatch_range(struct ssh *, u_int, u_int, dispatch_fn *);
int	ssh_dispatch_run(struct ssh *, int, volatile sig_atomic_t *);
void	ssh_dispatch_run_fatal(struct ssh *, int, volatile sig_atomic_t *);

#endif
