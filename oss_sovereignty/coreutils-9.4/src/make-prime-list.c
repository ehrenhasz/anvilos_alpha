 
#undef fclose
#undef free
#undef malloc
#undef strerror

 
#ifndef wide_uint
# if 4 < __GNUC__ + (6 <= __GNUC_MINOR__) && ULONG_MAX >> 31 >> 31 >> 1 != 0
typedef unsigned __int128 wide_uint;
# else
typedef uintmax_t wide_uint;
# endif
#endif

struct prime
{
  unsigned p;
  wide_uint pinv;  
  wide_uint lim;  
};

ATTRIBUTE_CONST
static wide_uint
binvert (wide_uint a)
{
  wide_uint x = 0xf5397db1 >> (4 * ((a / 2) & 0x7));
  for (;;)
    {
      wide_uint y = 2 * x - x * x * a;
      if (y == x)
        return x;
      x = y;
    }
}

static void
process_prime (struct prime *info, unsigned p)
{
  wide_uint max = -1;
  info->p = p;
  info->pinv = binvert (p);
  info->lim = max / p;
}

static void
print_wide_uint (wide_uint n, int nesting, unsigned wide_uint_bits)
{
   
  int hex_digits_per_literal = 7;
  int bits_per_literal = hex_digits_per_literal * 4;

  unsigned remainder = n & ((1 << bits_per_literal) - 1);

  if (n != remainder)
    {
      int needs_parentheses = n >> bits_per_literal >> bits_per_literal != 0;
      if (needs_parentheses)
        printf ("(");
      print_wide_uint (n >> bits_per_literal, nesting + 1, wide_uint_bits);
      if (needs_parentheses)
        printf (")\n%*s", nesting + 3, "");
      printf (" << %d | ", bits_per_literal);
    }
  else if (nesting)
    {
      printf ("(uintmax_t) ");
      hex_digits_per_literal
        = ((wide_uint_bits - 1) % bits_per_literal) % 4 + 1;
    }

  printf ("0x%0*xU", hex_digits_per_literal, remainder);
}

 
  unsigned wide_uint_bits = 0;
  wide_uint mask = -1;
  for (wide_uint_bits = 0; mask; wide_uint_bits++)
    mask >>= 1;

  puts ("/* Generated file -- DO NOT EDIT */\n");
  printf ("#define WIDE_UINT_BITS %u\n", wide_uint_bits);

  for (i = 0, p = 2; i < nprimes; i++)
    {
      unsigned int d8 = i + 8 < nprimes ? primes[i + 8].p - primes[i].p : 0xff;
      if (255 < d8)  
        abort ();
      printf ("P (%u, %u,\n   (", primes[i].p - p, d8);
      print_wide_uint (primes[i].pinv, 0, wide_uint_bits);
      printf ("),\n   UINTMAX_MAX / %u)\n", primes[i].p);
      p = primes[i].p;
    }

  printf ("\n#undef FIRST_OMITTED_PRIME\n");

   
  do
    {
      p += 2;
      for (i = 0, is_prime = 1; is_prime; i++)
        {
          if (primes[i].p * primes[i].p > p)
            break;
          if (p * primes[i].pinv <= primes[i].lim)
            {
              is_prime = 0;
              break;
            }
        }
    }
  while (!is_prime);

  printf ("#define FIRST_OMITTED_PRIME %u\n", p);
}

ATTRIBUTE_MALLOC
static void *
xalloc (size_t s)
{
  void *p = malloc (s);
  if (p)
    return p;

  fprintf (stderr, "Virtual memory exhausted.\n");
  exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
  int limit;

  char *sieve;
  size_t size, i;

  struct prime *prime_list;
  unsigned nprimes;

  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s LIMIT\n"
               "Produces a list of odd primes <= LIMIT\n", argv[0]);
      return EXIT_FAILURE;
    }
  limit = atoi (argv[1]);
  if (limit < 3)
    return EXIT_SUCCESS;

   
  if ( !(limit & 1))
    limit--;

  size = (limit - 1) / 2;
   
  sieve = xalloc (size);
  memset (sieve, 1, size);

  prime_list = xalloc (size * sizeof (*prime_list));
  nprimes = 0;

  for (i = 0; i < size;)
    {
      unsigned p = 3 + 2 * i;
      unsigned j;

      process_prime (&prime_list[nprimes++], p);

      for (j = (p * p - 3) / 2; j < size; j += p)
        sieve[j] = 0;

      while (++i < size && sieve[i] == 0)
        ;
    }

  output_primes (prime_list, nprimes);

  free (sieve);
  free (prime_list);

  if (ferror (stdout) + fclose (stdout))
    {
      fprintf (stderr, "write error: %s\n", strerror (errno));
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
