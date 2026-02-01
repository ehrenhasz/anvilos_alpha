 

#include <config.h>

#include "md5.h"

#include <stdio.h>
#include <string.h>

int
main (void)
{
   

  const char *in1 = "abc";
  const char *out1 =
    "\x90\x01\x50\x98\x3C\xD2\x4F\xB0\xD6\x96\x3F\x7D\x28\xE1\x7F\x72";
  const char *in2 = "message digest";
  const char *out2 =
    "\xF9\x6B\x69\x7D\x7C\xB7\x93\x8D\x52\x5A\x2F\x31\xAA\xF1\x61\xD0";
  char buf[MD5_DIGEST_SIZE];

  if (memcmp (md5_buffer (in1, strlen (in1), buf), out1, MD5_DIGEST_SIZE) != 0)
    {
      size_t i;
      printf ("expected:\n");
      for (i = 0; i < MD5_DIGEST_SIZE; i++)
        printf ("%02x ", out1[i] & 0xFFu);
      printf ("\ncomputed:\n");
      for (i = 0; i < MD5_DIGEST_SIZE; i++)
        printf ("%02x ", buf[i] & 0xFFu);
      printf ("\n");
      return 1;
    }

  if (memcmp (md5_buffer (in2, strlen (in2), buf), out2, MD5_DIGEST_SIZE) != 0)
    {
      size_t i;
      printf ("expected:\n");
      for (i = 0; i < MD5_DIGEST_SIZE; i++)
        printf ("%02x ", out2[i] & 0xFFu);
      printf ("\ncomputed:\n");
      for (i = 0; i < MD5_DIGEST_SIZE; i++)
        printf ("%02x ", buf[i] & 0xFFu);
      printf ("\n");
      return 1;
    }

  return 0;
}
