 

#include <config.h>

 
#include <unistd.h>

#if defined _WIN32 && ! defined __CYGWIN__
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
 
# undef GetUserName
# define GetUserName GetUserNameA
#endif

char *
getlogin (void)
{
#if defined _WIN32 && ! defined __CYGWIN__
  static char login_name[1024];
  DWORD sz = sizeof (login_name);

  if (GetUserName (login_name, &sz))
    return login_name;
#endif
  return NULL;
}
