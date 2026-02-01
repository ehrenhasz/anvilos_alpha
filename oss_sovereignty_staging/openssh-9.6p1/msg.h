 
 
#ifndef SSH_MSG_H
#define SSH_MSG_H

struct sshbuf;
int	 ssh_msg_send(int, u_char, struct sshbuf *);
int	 ssh_msg_recv(int, struct sshbuf *);

#endif
