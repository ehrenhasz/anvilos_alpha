



#include <termios.h>

struct termios *get_saved_tio(void);
void	 leave_raw_mode(int);
void	 enter_raw_mode(int);

int	 pty_allocate(int *, int *, char *, size_t);
void	 pty_release(const char *);
void	 pty_make_controlling_tty(int *, const char *);
void	 pty_change_window_size(int, u_int, u_int, u_int, u_int);
void	 pty_setowner(struct passwd *, const char *);
void	 disconnect_controlling_tty(void);
