 
typedef uint_fast64_t random_value;
#define RANDOM_VALUE_MAX UINT_FAST64_MAX
#define BASE_62_DIGITS 10  
#define BASE_62_POWER (62LL * 62 * 62 * 62 * 62 * 62 * 62 * 62 * 62 * 62)

 

static random_value
mix_random_values (random_value r, random_value s)
{
   
  return (2862933555777941757 * r + 3037000493) ^ s;
}

 

static bool
random_bits (random_value *r, random_value s)
{
   
  if (__getrandom (r, sizeof *r, GRND_NONBLOCK) == sizeof *r)
    return true;

   

  random_value v = s;

#if _LIBC || (defined CLOCK_REALTIME && HAVE_CLOCK_GETTIME)
  struct __timespec64 tv;
  __clock_gettime64 (CLOCK_REALTIME, &tv);
  v = mix_random_values (v, tv.tv_sec);
  v = mix_random_values (v, tv.tv_nsec);
#endif

  *r = mix_random_values (v, clock ());
  return false;
}

#if _LIBC
static int try_tempname_len (char *, int, void *, int (*) (char *, void *),
                             size_t);
#endif

static int
try_file (char *tmpl, void *flags)
{
  int *openflags = flags;
  return __open (tmpl,
                 (*openflags & ~O_ACCMODE)
                 | O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
}

static int
try_dir (char *tmpl, _GL_UNUSED void *flags)
{
  return __mkdir (tmpl, S_IRUSR | S_IWUSR | S_IXUSR);
}

static int
try_nocreate (char *tmpl, _GL_UNUSED void *flags)
{
  struct_stat64 st;

  if (__lstat64_time64 (tmpl, &st) == 0 || errno == EOVERFLOW)
    __set_errno (EEXIST);
  return errno == ENOENT ? 0 : -1;
}

 
static const char letters[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

 
#ifdef _LIBC
static
#endif
int
gen_tempname_len (char *tmpl, int suffixlen, int flags, int kind,
                  size_t x_suffix_len)
{
  static int (*const tryfunc[]) (char *, void *) =
    {
      [__GT_FILE] = try_file,
      [__GT_DIR] = try_dir,
      [__GT_NOCREATE] = try_nocreate
    };
  return try_tempname_len (tmpl, suffixlen, &flags, tryfunc[kind],
                           x_suffix_len);
}

#ifdef _LIBC
static
#endif
int
try_tempname_len (char *tmpl, int suffixlen, void *args,
                  int (*tryfunc) (char *, void *), size_t x_suffix_len)
{
  size_t len;
  char *XXXXXX;
  unsigned int count;
  int fd = -1;
  int save_errno = errno;

   
#define ATTEMPTS_MIN (62 * 62 * 62)

   
#if ATTEMPTS_MIN < TMP_MAX
  unsigned int attempts = TMP_MAX;
#else
  unsigned int attempts = ATTEMPTS_MIN;
#endif

   
  random_value v = 0;

   
  random_value vdigbuf;
  int vdigits = 0;

   
  random_value const biased_min
    = RANDOM_VALUE_MAX - RANDOM_VALUE_MAX % BASE_62_POWER;

  len = strlen (tmpl);
  if (len < x_suffix_len + suffixlen
      || strspn (&tmpl[len - x_suffix_len - suffixlen], "X") < x_suffix_len)
    {
      __set_errno (EINVAL);
      return -1;
    }

   
  XXXXXX = &tmpl[len - x_suffix_len - suffixlen];

  for (count = 0; count < attempts; ++count)
    {
      for (size_t i = 0; i < x_suffix_len; i++)
        {
          if (vdigits == 0)
            {
               
              while (random_bits (&v, v) && biased_min <= v)
                continue;

              vdigbuf = v;
              vdigits = BASE_62_DIGITS;
            }

          XXXXXX[i] = letters[vdigbuf % 62];
          vdigbuf /= 62;
          vdigits--;
        }

      fd = tryfunc (tmpl, args);
      if (fd >= 0)
        {
          __set_errno (save_errno);
          return fd;
        }
      else if (errno != EEXIST)
        return -1;
    }

   
  __set_errno (EEXIST);
  return -1;
}

int
__gen_tempname (char *tmpl, int suffixlen, int flags, int kind)
{
  return gen_tempname_len (tmpl, suffixlen, flags, kind, 6);
}

#if !_LIBC
int
try_tempname (char *tmpl, int suffixlen, void *args,
              int (*tryfunc) (char *, void *))
{
  return try_tempname_len (tmpl, suffixlen, args, tryfunc, 6);
}
#endif
