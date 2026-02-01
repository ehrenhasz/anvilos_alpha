 

#include <config.h>

#include <arpa/inet.h>

#include "signature.h"
SIGNATURE_CHECK (inet_pton, int, (int, const char *, void *));

#include <netinet/in.h>
#include <sys/socket.h>

#include "macros.h"

int
main (void)
{
#if defined AF_INET  
  {
     
    const char printable[] = "129.13.115.2";
    struct in_addr internal;
    int ret;

    ret = inet_pton (AF_INET, printable, &internal);
    ASSERT (ret == 1);
     
    ASSERT (((unsigned char *) &internal)[0] == 0x81);
    ASSERT (((unsigned char *) &internal)[1] == 0x0D);
    ASSERT (((unsigned char *) &internal)[2] == 0x73);
    ASSERT (((unsigned char *) &internal)[3] == 0x02);
# ifdef WORDS_BIGENDIAN
    ASSERT (internal.s_addr == 0x810D7302);
# else
    ASSERT (internal.s_addr == 0x02730D81);
# endif
  }
#endif

  return 0;
}
