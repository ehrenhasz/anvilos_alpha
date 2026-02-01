 

void *
xmalloc (size_t s)
{
  return nonnull (malloc (s));
}

void *
ximalloc (idx_t s)
{
  return nonnull (imalloc (s));
}

char *
xcharalloc (size_t n)
{
  return XNMALLOC (n, char);
}

 

void *
xrealloc (void *p, size_t s)
{
  void *r = realloc (p, s);
  if (!r && (!p || s))
    xalloc_die ();
  return r;
}

void *
xirealloc (void *p, idx_t s)
{
  return nonnull (irealloc (p, s));
}

 

void *
xreallocarray (void *p, size_t n, size_t s)
{
  void *r = reallocarray (p, n, s);
  if (!r && (!p || (n && s)))
    xalloc_die ();
  return r;
}

void *
xireallocarray (void *p, idx_t n, idx_t s)
{
  return nonnull (ireallocarray (p, n, s));
}

 

void *
xnmalloc (size_t n, size_t s)
{
  return xreallocarray (NULL, n, s);
}

void *
xinmalloc (idx_t n, idx_t s)
{
  return xireallocarray (NULL, n, s);
}

 

void *
x2realloc (void *p, size_t *ps)
{
  return x2nrealloc (p, ps, 1);
}

 

void *
x2nrealloc (void *p, size_t *pn, size_t s)
{
  size_t n = *pn;

  if (! p)
    {
      if (! n)
        {
           
          enum { DEFAULT_MXFAST = 64 * sizeof (size_t) / 4 };

          n = DEFAULT_MXFAST / s;
          n += !n;
        }
    }
  else
    {
       
      if (ckd_add (&n, n, (n >> 1) + 1))
        xalloc_die ();
    }

  p = xreallocarray (p, n, s);
  *pn = n;
  return p;
}

 

void *
xpalloc (void *pa, idx_t *pn, idx_t n_incr_min, ptrdiff_t n_max, idx_t s)
{
  idx_t n0 = *pn;

   
  enum { DEFAULT_MXFAST = 64 * sizeof (size_t) / 4 };

   

  idx_t n;
  if (ckd_add (&n, n0, n0 >> 1))
    n = IDX_MAX;
  if (0 <= n_max && n_max < n)
    n = n_max;

   
#if IDX_MAX <= SIZE_MAX
  idx_t nbytes;
#else
  size_t nbytes;
#endif
  idx_t adjusted_nbytes
    = (ckd_mul (&nbytes, n, s)
       ? MIN (IDX_MAX, SIZE_MAX)
       : nbytes < DEFAULT_MXFAST ? DEFAULT_MXFAST : 0);
  if (adjusted_nbytes)
    {
      n = adjusted_nbytes / s;
      nbytes = adjusted_nbytes - adjusted_nbytes % s;
    }

  if (! pa)
    *pn = 0;
  if (n - n0 < n_incr_min
      && (ckd_add (&n, n0, n_incr_min)
          || (0 <= n_max && n_max < n)
          || ckd_mul (&nbytes, n, s)))
    xalloc_die ();
  pa = xrealloc (pa, nbytes);
  *pn = n;
  return pa;
}

 

void *
xzalloc (size_t s)
{
  return xcalloc (s, 1);
}

void *
xizalloc (idx_t s)
{
  return xicalloc (s, 1);
}

 

void *
xcalloc (size_t n, size_t s)
{
  return nonnull (calloc (n, s));
}

void *
xicalloc (idx_t n, idx_t s)
{
  return nonnull (icalloc (n, s));
}

 

void *
xmemdup (void const *p, size_t s)
{
  return memcpy (xmalloc (s), p, s);
}

void *
ximemdup (void const *p, idx_t s)
{
  return memcpy (ximalloc (s), p, s);
}

 

char *
ximemdup0 (void const *p, idx_t s)
{
  char *result = ximalloc (s + 1);
  result[s] = 0;
  return memcpy (result, p, s);
}

 

char *
xstrdup (char const *string)
{
  return xmemdup (string, strlen (string) + 1);
}
