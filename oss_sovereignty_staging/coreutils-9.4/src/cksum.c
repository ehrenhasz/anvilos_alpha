 

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include "system.h"

#include <byteswap.h>
#ifdef WORDS_BIGENDIAN
# define SWAP(n) (n)
#else
# define SWAP(n) bswap_32 (n)
#endif

#ifdef CRCTAB

# define BIT(x)	((uint_fast32_t) 1 << (x))
# define SBIT	BIT (31)

 

# define GEN	(BIT (26) | BIT (23) | BIT (22) | BIT (16) | BIT (12) \
                 | BIT (11) | BIT (10) | BIT (8) | BIT (7) | BIT (5) \
                 | BIT (4) | BIT (2) | BIT (1) | BIT (0))

static uint_fast32_t r[8];

static void
fill_r (void)
{
  r[0] = GEN;
  for (int i = 1; i < 8; i++)
    r[i] = (r[i - 1] << 1) ^ ((r[i - 1] & SBIT) ? GEN : 0);
}

static uint_fast32_t
crc_remainder (int m)
{
  uint_fast32_t rem = 0;

  for (int i = 0; i < 8; i++)
    if (BIT (i) & m)
      rem ^= r[i];

  return rem & 0xFFFFFFFF;	 
}

int
main (void)
{
  int i;
  static uint_fast32_t crctab[8][256];

  fill_r ();

  for (i = 0; i < 256; i++)
    {
      crctab[0][i] = crc_remainder (i);
    }

   
  for (i = 0; i < 256; i++)
    {
      uint32_t crc = 0;

      crc = (crc << 8) ^ crctab[0][((crc >> 24) ^ (i & 0xFF)) & 0xFF];
      for (idx_t offset = 1; offset < 8; offset++)
        {
          crc = (crc << 8) ^ crctab[0][((crc >> 24) ^ 0x00) & 0xFF];
          crctab[offset][i] = crc;
        }
    }

  printf ("#include <config.h>\n");
  printf ("#include <stdint.h>\n");
  printf ("\nuint_fast32_t const crctab[8][256] = {\n");
  for (int y = 0; y < 8; y++)
    {
      printf ("{\n  0x%08x", crctab[y][0]);
      for (i = 0; i < 51; i++)
        {
          printf (",\n  0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x",
                  crctab[y][i * 5 + 1], crctab[y][i * 5 + 2],
                  crctab[y][i * 5 + 3], crctab[y][i * 5 + 4],
                  crctab[y][i * 5 + 5]);
        }
        printf ("\n},\n");
    }
  printf ("};\n");
  return EXIT_SUCCESS;
}

#else  

# include "cksum.h"

 
# define BUFLEN (1 << 16)

# if USE_PCLMUL_CRC32
static bool
pclmul_supported (void)
{
  bool pclmul_enabled = (0 < __builtin_cpu_supports ("pclmul")
                         && 0 < __builtin_cpu_supports ("avx"));

  if (cksum_debug)
    error (0, 0, "%s",
           (pclmul_enabled
            ? _("using pclmul hardware support")
            : _("pclmul support not detected")));

  return pclmul_enabled;
}
# endif  

static bool
cksum_slice8 (FILE *fp, uint_fast32_t *crc_out, uintmax_t *length_out)
{
  uint32_t buf[BUFLEN / sizeof (uint32_t)];
  uint_fast32_t crc = 0;
  uintmax_t length = 0;
  size_t bytes_read;

  if (!fp || !crc_out || !length_out)
    return false;

  while ((bytes_read = fread (buf, 1, BUFLEN, fp)) > 0)
    {
      uint32_t *datap;

      if (length + bytes_read < length)
        {
          errno = EOVERFLOW;
          return false;
        }
      length += bytes_read;

       
      datap = (uint32_t *)buf;
      while (bytes_read >= 8)
        {
          uint32_t first = *datap++, second = *datap++;
          crc ^= SWAP (first);
          second = SWAP (second);
          crc = (crctab[7][(crc >> 24) & 0xFF]
                 ^ crctab[6][(crc >> 16) & 0xFF]
                 ^ crctab[5][(crc >> 8) & 0xFF]
                 ^ crctab[4][(crc) & 0xFF]
                 ^ crctab[3][(second >> 24) & 0xFF]
                 ^ crctab[2][(second >> 16) & 0xFF]
                 ^ crctab[1][(second >> 8) & 0xFF]
                 ^ crctab[0][(second) & 0xFF]);
          bytes_read -= 8;
        }

       
      unsigned char *cp = (unsigned char *)datap;
      while (bytes_read--)
        crc = (crc << 8) ^ crctab[0][((crc >> 24) ^ *cp++) & 0xFF];
      if (feof (fp))
        break;
    }

  *crc_out = crc;
  *length_out = length;

  return !ferror (fp);
}

 

int
crc_sum_stream (FILE *stream, void *resstream, uintmax_t *length)
{
  uintmax_t total_bytes = 0;
  uint_fast32_t crc = 0;

# if USE_PCLMUL_CRC32
  static bool (*cksum_fp) (FILE *, uint_fast32_t *, uintmax_t *);
  if (! cksum_fp)
    cksum_fp = pclmul_supported () ? cksum_pclmul : cksum_slice8;
# else
  bool (*cksum_fp) (FILE *, uint_fast32_t *, uintmax_t *) = cksum_slice8;
# endif

  if (! cksum_fp (stream, &crc, &total_bytes))
    return -1;

  *length = total_bytes;

  for (; total_bytes; total_bytes >>= 8)
    crc = (crc << 8) ^ crctab[0][((crc >> 24) ^ total_bytes) & 0xFF];
  crc = ~crc & 0xFFFFFFFF;

  unsigned int crc_out = crc;
  memcpy (resstream, &crc_out, sizeof crc_out);

  return 0;
}

 

void
output_crc (char const *file, int binary_file, void const *digest, bool raw,
            bool tagged, unsigned char delim, bool args, uintmax_t length)
{
  if (raw)
    {
       
      uint32_t out_int = SWAP (*(uint32_t *)digest);
      fwrite (&out_int, 1, 32/8, stdout);
      return;
    }

  char length_buf[INT_BUFSIZE_BOUND (uintmax_t)];
  printf ("%u %s", *(unsigned int *)digest, umaxtostr (length, length_buf));
  if (args)
    printf (" %s", file);
  putchar (delim);
}

#endif  
