 

 

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include "system.h"
#include "human.h"
#include "sum.h"

#include <byteswap.h>
#ifdef WORDS_BIGENDIAN
# define SWAP(n) (n)
#else
# define SWAP(n) bswap_16 (n)
#endif

 

int
bsd_sum_stream (FILE *stream, void *resstream, uintmax_t *length)
{
  int ret = -1;
  size_t sum, n;
  int checksum = 0;	 
  uintmax_t total_bytes = 0;	 
  static const size_t buffer_length = 32768;
  uint8_t *buffer = malloc (buffer_length);

  if (! buffer)
    return -1;

   
  while (true)
  {
    sum = 0;

     
    while (true)
    {
      n = fread (buffer + sum, 1, buffer_length - sum, stream);
      sum += n;

      if (buffer_length == sum)
        break;

      if (n == 0)
        {
          if (ferror (stream))
            goto cleanup_buffer;
          goto final_process;
        }

      if (feof (stream))
        goto final_process;
    }

    for (size_t i = 0; i < sum; i++)
      {
        checksum = (checksum >> 1) + ((checksum & 1) << 15);
        checksum += buffer[i];
        checksum &= 0xffff;	 
      }
    if (total_bytes + sum < total_bytes)
      {
        errno = EOVERFLOW;
        goto cleanup_buffer;
      }
    total_bytes += sum;
  }

final_process:;

  for (size_t i = 0; i < sum; i++)
    {
      checksum = (checksum >> 1) + ((checksum & 1) << 15);
      checksum += buffer[i];
      checksum &= 0xffff;	 
    }
  if (total_bytes + sum < total_bytes)
    {
      errno = EOVERFLOW;
      goto cleanup_buffer;
    }
  total_bytes += sum;

  memcpy (resstream, &checksum, sizeof checksum);
  *length = total_bytes;
  ret = 0;
cleanup_buffer:
  free (buffer);
  return ret;
}

 

int
sysv_sum_stream (FILE *stream, void *resstream, uintmax_t *length)
{
  int ret = -1;
  size_t sum, n;
  uintmax_t total_bytes = 0;
  static const size_t buffer_length = 32768;
  uint8_t *buffer = malloc (buffer_length);

  if (! buffer)
    return -1;

   
  unsigned int s = 0;

   
  while (true)
  {
    sum = 0;

     
    while (true)
    {
      n = fread (buffer + sum, 1, buffer_length - sum, stream);
      sum += n;

      if (buffer_length == sum)
        break;

      if (n == 0)
        {
          if (ferror (stream))
            goto cleanup_buffer;
          goto final_process;
        }

      if (feof (stream))
        goto final_process;
    }

    for (size_t i = 0; i < sum; i++)
      s += buffer[i];
    if (total_bytes + sum < total_bytes)
      {
        errno = EOVERFLOW;
        goto cleanup_buffer;
      }
    total_bytes += sum;
  }

final_process:;

  for (size_t i = 0; i < sum; i++)
    s += buffer[i];
  if (total_bytes + sum < total_bytes)
    {
      errno = EOVERFLOW;
      goto cleanup_buffer;
    }
  total_bytes += sum;

  int r = (s & 0xffff) + ((s & 0xffffffff) >> 16);
  int checksum = (r & 0xffff) + (r >> 16);

  memcpy (resstream, &checksum, sizeof checksum);
  *length = total_bytes;
  ret = 0;
cleanup_buffer:
  free (buffer);
  return ret;
}

 

void
output_bsd (char const *file, int binary_file, void const *digest,
            bool raw, bool tagged, unsigned char delim, bool args,
            uintmax_t length)
{
  if (raw)
    {
       
      uint16_t out_int = *(int *)digest;
      out_int = SWAP (out_int);
      fwrite (&out_int, 1, 16/8, stdout);
      return;
    }

  char hbuf[LONGEST_HUMAN_READABLE + 1];
  printf ("%05d %5s", *(int *)digest,
          human_readable (length, hbuf, human_ceiling, 1, 1024));
  if (args)
    printf (" %s", file);
  putchar (delim);
}

 

void
output_sysv (char const *file, int binary_file, void const *digest,
             bool raw, bool tagged, unsigned char delim, bool args,
             uintmax_t length)
{
  if (raw)
    {
       
      uint16_t out_int = *(int *)digest;
      out_int = SWAP (out_int);
      fwrite (&out_int, 1, 16/8, stdout);
      return;
    }

  char hbuf[LONGEST_HUMAN_READABLE + 1];
  printf ("%d %s", *(int *)digest,
          human_readable (length, hbuf, human_ceiling, 1, 512));
  if (args)
    printf (" %s", file);
  putchar (delim);
}
