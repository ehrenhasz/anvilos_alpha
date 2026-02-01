 

#include <config.h>

#include <sys/uio.h>

 
size_t a;
ssize_t b;
struct iovec c;

int
main (void)
{
  return a + b + !!c.iov_base + c.iov_len;
}
