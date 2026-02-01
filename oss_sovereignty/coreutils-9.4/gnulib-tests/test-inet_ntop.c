 

#include <config.h>

#include <arpa/inet.h>

#include "signature.h"
SIGNATURE_CHECK (inet_ntop, char const *, (int, void const *, char *,
                                           socklen_t));

#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

#include "macros.h"

int
main (void)
{
#if defined AF_INET  
  {
    struct in_addr internal;
    char printable[16];
    const char *result;

     
# ifdef WORDS_BIGENDIAN
    internal.s_addr = 0x810D7302;
# else
    internal.s_addr = 0x02730D81;
# endif
    result = inet_ntop (AF_INET, &internal, printable, sizeof (printable));
    ASSERT (result != NULL);
    ASSERT (strcmp (result, "129.13.115.2") == 0);
  }
#endif

  return 0;
}
