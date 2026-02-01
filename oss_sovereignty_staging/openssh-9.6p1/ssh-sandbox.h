 
 

struct monitor;
struct ssh_sandbox;

struct ssh_sandbox *ssh_sandbox_init(struct monitor *);
void ssh_sandbox_child(struct ssh_sandbox *);
void ssh_sandbox_parent_finish(struct ssh_sandbox *);
void ssh_sandbox_parent_preauth(struct ssh_sandbox *, pid_t);
