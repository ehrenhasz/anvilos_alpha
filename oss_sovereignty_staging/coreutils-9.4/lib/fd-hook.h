 
#if WINDOWS_SOCKETS


 
typedef int (*gl_close_fn) (int fd);

 
typedef int (*gl_ioctl_fn) (int fd, int request, void *arg);

 
struct fd_hook
{
   
  struct fd_hook *private_next;
  struct fd_hook *private_prev;
   
  int (*private_close_fn) (const struct fd_hook *remaining_list,
                           gl_close_fn primary,
                           int fd);
   
  int (*private_ioctl_fn) (const struct fd_hook *remaining_list,
                           gl_ioctl_fn primary,
                           int fd, int request, void *arg);
};

 
typedef int (*close_hook_fn) (const struct fd_hook *remaining_list,
                              gl_close_fn primary,
                              int fd);

 
extern int execute_close_hooks (const struct fd_hook *remaining_list,
                                gl_close_fn primary,
                                int fd);

 
extern int execute_all_close_hooks (gl_close_fn primary, int fd);

 
typedef int (*ioctl_hook_fn) (const struct fd_hook *remaining_list,
                              gl_ioctl_fn primary,
                              int fd, int request, void *arg);

 
extern int execute_ioctl_hooks (const struct fd_hook *remaining_list,
                                gl_ioctl_fn primary,
                                int fd, int request, void *arg);

 
extern int execute_all_ioctl_hooks (gl_ioctl_fn primary,
                                    int fd, int request, void *arg);

 
extern void register_fd_hook (close_hook_fn close_hook, ioctl_hook_fn ioctl_hook,
                              struct fd_hook *link);

 
extern void unregister_fd_hook (struct fd_hook *link);


#endif


#ifdef __cplusplus
}
#endif

#endif  
