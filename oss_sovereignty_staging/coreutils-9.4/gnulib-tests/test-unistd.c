 

#include <config.h>

#include <unistd.h>

 
static_assert (sizeof NULL == sizeof (void *));

 
int sk[] = { SEEK_CUR, SEEK_END, SEEK_SET };

 
#if ! (defined STDIN_FILENO                                     \
       && (STDIN_FILENO + STDOUT_FILENO + STDERR_FILENO == 3))
missing or broken *_FILENO macros
#endif

 
size_t t1;
ssize_t t2;
#ifdef TODO  
uid_t t3;
gid_t t4;
#endif
off_t t5;
pid_t t6;
#ifdef TODO
useconds_t t7;
intptr_t t8;
#endif

int
main (void)
{
  return 0;
}
