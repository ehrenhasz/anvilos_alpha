 

static void
xstrtol_error (enum strtol_error err,
               int opt_idx, char c, struct option const *long_options,
               char const *arg,
               int exit_status)
{
  char const *hyphens = "--";
  char const *msgid;
  char const *option;
  char option_buffer[2];

  switch (err)
    {
    default:
      abort ();

    case LONGINT_INVALID:
      msgid = N_("invalid %s%s argument '%s'");
      break;

    case LONGINT_INVALID_SUFFIX_CHAR:
    case LONGINT_INVALID_SUFFIX_CHAR_WITH_OVERFLOW:
      msgid = N_("invalid suffix in %s%s argument '%s'");
      break;

    case LONGINT_OVERFLOW:
      msgid = N_("%s%s argument '%s' too large");
      break;
    }

  if (opt_idx < 0)
    {
      hyphens -= opt_idx;
      option_buffer[0] = c;
      option_buffer[1] = '\0';
      option = option_buffer;
    }
  else
    option = long_options[opt_idx].name;

  error (exit_status, 0, gettext (msgid), hyphens, option, arg);
}

 

void
xstrtol_fatal (enum strtol_error err,
               int opt_idx, char c, struct option const *long_options,
               char const *arg)
{
  xstrtol_error (err, opt_idx, c, long_options, arg, exit_failure);
  abort ();
}
