 
#if __GNUC__ + (__GNUC_MINOR__ >= 7) > 4
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

 
static bool
file_accessible (char const *file)
{
# if HAVE_FACCESSAT
  return faccessat (AT_FDCWD, file, F_OK, AT_EACCESS) == 0;
# else
  struct stat st;
  return stat (file, &st) == 0 || errno == EOVERFLOW;
# endif
}

 

static bool _GL_ATTRIBUTE_PURE
suffix_requires_dir_check (char const *end)
{
   
  while (ISSLASH (*end))
    {
       
      do
        end++;
      while (ISSLASH (*end));

      switch (*end++)
        {
        default: return false;   
        case '\0': return true;  
        case '.': break;         
        }
       
      if (!*end || (*end == '.' && (!end[1] || ISSLASH (end[1]))))
        return true;
    }

  return false;
}

 

#ifdef LSTAT_FOLLOWS_SLASHED_SYMLINK
static char const dir_suffix[] = "/";
#else
static char const dir_suffix[] = "/./";
#endif

 

static bool
dir_check (char *dir, char *dirend)
{
  strcpy (dirend, dir_suffix);
  return file_accessible (dir);
}

#if !((HAVE_CANONICALIZE_FILE_NAME && FUNC_REALPATH_WORKS)      \
      || GNULIB_CANONICALIZE_LGPL)
 

char *
canonicalize_file_name (const char *name)
{
  return canonicalize_filename_mode (name, CAN_EXISTING);
}
#endif  

static bool
multiple_bits_set (canonicalize_mode_t i)
{
  return (i & (i - 1)) != 0;
}

 
static bool
seen_triple (Hash_table **ht, char const *filename, struct stat const *st)
{
  if (*ht == NULL)
    {
      idx_t initial_capacity = 7;
      *ht = hash_initialize (initial_capacity,
                            NULL,
                            triple_hash,
                            triple_compare_ino_str,
                            triple_free);
      if (*ht == NULL)
        xalloc_die ();
    }

  if (seen_file (*ht, filename, st))
    return true;

  record_file (*ht, filename, st);
  return false;
}

 
struct realpath_bufs
{
  struct scratch_buffer rname;
  struct scratch_buffer extra;
  struct scratch_buffer link;
};

static char *
canonicalize_filename_mode_stk (const char *name, canonicalize_mode_t can_mode,
                                struct realpath_bufs *bufs)
{
  char *dest;
  char const *start;
  char const *end;
  Hash_table *ht = NULL;
  bool logical = (can_mode & CAN_NOLINKS) != 0;
  int num_links = 0;

  canonicalize_mode_t can_exist = can_mode & CAN_MODE_MASK;
  if (multiple_bits_set (can_exist))
    {
      errno = EINVAL;
      return NULL;
    }

  if (name == NULL)
    {
      errno = EINVAL;
      return NULL;
    }

  if (name[0] == '\0')
    {
      errno = ENOENT;
      return NULL;
    }

  char *rname = bufs->rname.data;
  bool end_in_extra_buffer = false;
  bool failed = true;

   
  idx_t prefix_len = FILE_SYSTEM_PREFIX_LEN (name);

  if (!IS_ABSOLUTE_FILE_NAME (name))
    {
      while (!getcwd (bufs->rname.data, bufs->rname.length))
        {
          switch (errno)
            {
            case ERANGE:
              if (scratch_buffer_grow (&bufs->rname))
                break;
              FALLTHROUGH;
            case ENOMEM:
              xalloc_die ();

            default:
              dest = rname;
              goto error;
            }
          rname = bufs->rname.data;
        }
      dest = rawmemchr (rname, '\0');
      start = name;
      prefix_len = FILE_SYSTEM_PREFIX_LEN (rname);
    }
  else
    {
      dest = mempcpy (rname, name, prefix_len);
      *dest++ = '/';
      if (DOUBLE_SLASH_IS_DISTINCT_ROOT)
        {
          if (prefix_len == 0  
              && ISSLASH (name[1]) && !ISSLASH (name[2]))
            {
              *dest++ = '/';
#if defined _WIN32 && !defined __CYGWIN__
               
              {
                idx_t i;
                for (i = 2; name[i] != '\0' && !ISSLASH (name[i]); )
                  i++;
                if (name[i] != '\0'  
                    && i + 1 < bufs->rname.length)
                  {
                    prefix_len = i;
                    memcpy (dest, name + 2, i - 2 + 1);
                    dest += i - 2 + 1;
                  }
                else
                  {
                     
                  }
              }
#endif
            }
          *dest = '\0';
        }
      start = name + prefix_len;
    }

  for ( ; *start; start = end)
    {
       
      while (ISSLASH (*start))
        ++start;

       
      for (end = start; *end && !ISSLASH (*end); ++end)
         ;

       
      idx_t startlen = end - start;

      if (startlen == 0)
        break;
      else if (startlen == 1 && start[0] == '.')
         ;
      else if (startlen == 2 && start[0] == '.' && start[1] == '.')
        {
           
          if (dest > rname + prefix_len + 1)
            for (--dest; dest > rname && !ISSLASH (dest[-1]); --dest)
              continue;
          if (DOUBLE_SLASH_IS_DISTINCT_ROOT
              && dest == rname + 1 && !prefix_len
              && ISSLASH (*dest) && !ISSLASH (dest[1]))
            dest++;
        }
      else
        {
          if (!ISSLASH (dest[-1]))
            *dest++ = '/';

          while (rname + bufs->rname.length - dest
                 < startlen + sizeof dir_suffix)
            {
              idx_t dest_offset = dest - rname;
              if (!scratch_buffer_grow_preserve (&bufs->rname))
                xalloc_die ();
              rname = bufs->rname.data;
              dest = rname + dest_offset;
            }

          dest = mempcpy (dest, start, startlen);
          *dest = '\0';

          char *buf;
          ssize_t n = -1;
          if (!logical)
            {
              while (true)
                {
                  buf = bufs->link.data;
                  idx_t bufsize = bufs->link.length;
                  n = readlink (rname, buf, bufsize - 1);
                  if (n < bufsize - 1)
                    break;
                  if (!scratch_buffer_grow (&bufs->link))
                    xalloc_die ();
                }
            }
          if (0 <= n)
            {
               

              if (num_links < 20)
                num_links++;
              else if (*start)
                {
                   
                  struct stat st;
                  dest[- startlen] = '\0';
                  if (stat (*rname ? rname : ".", &st) != 0)
                    goto error;
                  dest[- startlen] = *start;

                   
                  if (seen_triple (&ht, start, &st))
                    {
                      if (can_exist == CAN_MISSING)
                        continue;
                      errno = ELOOP;
                      goto error;
                    }
                }

              buf[n] = '\0';

              char *extra_buf = bufs->extra.data;
              idx_t end_idx;
              if (end_in_extra_buffer)
                end_idx = end - extra_buf;
              size_t len = strlen (end);
              if (INT_ADD_OVERFLOW (len, n))
                xalloc_die ();
              while (bufs->extra.length <= len + n)
                {
                  if (!scratch_buffer_grow_preserve (&bufs->extra))
                    xalloc_die ();
                  extra_buf = bufs->extra.data;
                }
              if (end_in_extra_buffer)
                end = extra_buf + end_idx;

               
              memmove (&extra_buf[n], end, len + 1);
              name = end = memcpy (extra_buf, buf, n);
              end_in_extra_buffer = true;

              if (IS_ABSOLUTE_FILE_NAME (buf))
                {
                  idx_t pfxlen = FILE_SYSTEM_PREFIX_LEN (buf);

                  dest = mempcpy (rname, buf, pfxlen);
                  *dest++ = '/';  
                  if (DOUBLE_SLASH_IS_DISTINCT_ROOT)
                    {
                      if (ISSLASH (buf[1]) && !ISSLASH (buf[2]) && !pfxlen)
                        *dest++ = '/';
                      *dest = '\0';
                    }
                   
                  prefix_len = pfxlen;
                }
              else
                {
                   
                  if (dest > rname + prefix_len + 1)
                    for (--dest; dest > rname && !ISSLASH (dest[-1]); --dest)
                      continue;
                  if (DOUBLE_SLASH_IS_DISTINCT_ROOT && dest == rname + 1
                      && ISSLASH (*dest) && !ISSLASH (dest[1]) && !prefix_len)
                    dest++;
                }
            }
          else if (! (can_exist == CAN_MISSING
                      || (suffix_requires_dir_check (end)
                          ? dir_check (rname, dest)
                          : !logical
                          ? errno == EINVAL
                          : *end || file_accessible (rname))
                      || (can_exist == CAN_ALL_BUT_LAST
                          && errno == ENOENT
                          && !end[strspn (end, SLASHES)])))
            goto error;
        }
    }
  if (dest > rname + prefix_len + 1 && ISSLASH (dest[-1]))
    --dest;
  if (DOUBLE_SLASH_IS_DISTINCT_ROOT && dest == rname + 1 && !prefix_len
      && ISSLASH (*dest) && !ISSLASH (dest[1]))
    dest++;
  failed = false;

error:
  if (ht)
    hash_free (ht);

  if (failed)
    return NULL;

  *dest++ = '\0';
  char *result = malloc (dest - rname);
  if (!result)
    xalloc_die ();
  return memcpy (result, rname, dest - rname);
}

 

char *
canonicalize_filename_mode (const char *name, canonicalize_mode_t can_mode)
{
  struct realpath_bufs bufs;
  scratch_buffer_init (&bufs.rname);
  scratch_buffer_init (&bufs.extra);
  scratch_buffer_init (&bufs.link);
  char *result = canonicalize_filename_mode_stk (name, can_mode, &bufs);
  scratch_buffer_free (&bufs.link);
  scratch_buffer_free (&bufs.extra);
  scratch_buffer_free (&bufs.rname);
  return result;
}
