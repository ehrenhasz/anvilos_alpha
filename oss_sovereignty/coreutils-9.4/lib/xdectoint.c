 

__xdectoint_t
__xnumtoint (char const *n_str, int base, __xdectoint_t min, __xdectoint_t max,
             char const *suffixes, char const *err, int err_exit)
{
  strtol_error s_err;

  __xdectoint_t tnum;
  s_err = __xstrtol (n_str, nullptr, base, &tnum, suffixes);

  if (s_err == LONGINT_OK)
    {
      if (tnum < min || max < tnum)
        {
          s_err = LONGINT_OVERFLOW;
           
          if (tnum > INT_MAX / 2)
            errno = EOVERFLOW;
#if __xdectoint_signed
          else if (tnum < INT_MIN / 2)
            errno = EOVERFLOW;
#endif
          else
            errno = ERANGE;
        }
    }
  else if (s_err == LONGINT_OVERFLOW)
    errno = EOVERFLOW;
  else if (s_err == LONGINT_INVALID_SUFFIX_CHAR_WITH_OVERFLOW)
    errno = 0;  

  if (s_err != LONGINT_OK)
    {
       
      error (err_exit ? err_exit : EXIT_FAILURE, errno == EINVAL ? 0 : errno,
             "%s: %s", err, quote (n_str));
      unreachable ();
    }

  return tnum;
}

 

__xdectoint_t
__xdectoint (char const *n_str, __xdectoint_t min, __xdectoint_t max,
             char const *suffixes, char const *err, int err_exit)
{
  return __xnumtoint (n_str, 10, min, max, suffixes, err, err_exit);
}
