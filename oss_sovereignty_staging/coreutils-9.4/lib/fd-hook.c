 
#include "fd-hook.h"

#include <stdlib.h>

 
#if WINDOWS_SOCKETS

 
static struct fd_hook anchor = { &anchor, &anchor, NULL, NULL };

int
execute_close_hooks (const struct fd_hook *remaining_list, gl_close_fn primary,
                     int fd)
{
  if (remaining_list == &anchor)
     
    return primary (fd);
  else
    return remaining_list->private_close_fn (remaining_list->private_next,
                                             primary, fd);
}

int
execute_all_close_hooks (gl_close_fn primary, int fd)
{
  return execute_close_hooks (anchor.private_next, primary, fd);
}

int
execute_ioctl_hooks (const struct fd_hook *remaining_list, gl_ioctl_fn primary,
                     int fd, int request, void *arg)
{
  if (remaining_list == &anchor)
     
    return primary (fd, request, arg);
  else
    return remaining_list->private_ioctl_fn (remaining_list->private_next,
                                             primary, fd, request, arg);
}

int
execute_all_ioctl_hooks (gl_ioctl_fn primary,
                         int fd, int request, void *arg)
{
  return execute_ioctl_hooks (anchor.private_next, primary, fd, request, arg);
}

void
register_fd_hook (close_hook_fn close_hook, ioctl_hook_fn ioctl_hook, struct fd_hook *link)
{
  if (close_hook == NULL)
    close_hook = execute_close_hooks;
  if (ioctl_hook == NULL)
    ioctl_hook = execute_ioctl_hooks;

  if (link->private_next == NULL && link->private_prev == NULL)
    {
       
      link->private_next = anchor.private_next;
      link->private_prev = &anchor;
      link->private_close_fn = close_hook;
      link->private_ioctl_fn = ioctl_hook;
      anchor.private_next->private_prev = link;
      anchor.private_next = link;
    }
  else
    {
       
      if (link->private_close_fn != close_hook
          || link->private_ioctl_fn != ioctl_hook)
        abort ();
    }
}

void
unregister_fd_hook (struct fd_hook *link)
{
  struct fd_hook *next = link->private_next;
  struct fd_hook *prev = link->private_prev;

  if (next != NULL && prev != NULL)
    {
       
      prev->private_next = next;
      next->private_prev = prev;
       
      link->private_next = NULL;
      link->private_prev = NULL;
      link->private_close_fn = NULL;
      link->private_ioctl_fn = NULL;
    }
}

#endif
