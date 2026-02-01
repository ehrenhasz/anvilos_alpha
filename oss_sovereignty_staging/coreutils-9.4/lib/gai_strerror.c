 

static struct
  {
    int code;
    const char *msg;
  }
values[] =
  {
    { EAI_ADDRFAMILY, N_("Address family for hostname not supported") },
    { EAI_AGAIN, N_("Temporary failure in name resolution") },
    { EAI_BADFLAGS, N_("Bad value for ai_flags") },
    { EAI_FAIL, N_("Non-recoverable failure in name resolution") },
    { EAI_FAMILY, N_("ai_family not supported") },
    { EAI_MEMORY, N_("Memory allocation failure") },
    { EAI_NODATA, N_("No address associated with hostname") },
    { EAI_NONAME, N_("Name or service not known") },
    { EAI_SERVICE, N_("Servname not supported for ai_socktype") },
    { EAI_SOCKTYPE, N_("ai_socktype not supported") },
    { EAI_SYSTEM, N_("System error") },
    { EAI_OVERFLOW, N_("Argument buffer too small") },
#ifdef EAI_INPROGRESS
    { EAI_INPROGRESS, N_("Processing request in progress") },
    { EAI_CANCELED, N_("Request canceled") },
    { EAI_NOTCANCELED, N_("Request not canceled") },
    { EAI_ALLDONE, N_("All requests done") },
    { EAI_INTR, N_("Interrupted by a signal") },
    { EAI_IDN_ENCODE, N_("Parameter string not correctly encoded") }
#endif
  };

const char *
gai_strerror (int code)
{
  size_t i;
  for (i = 0; i < sizeof (values) / sizeof (values[0]); ++i)
    if (values[i].code == code)
      return _(values[i].msg);

  return _("Unknown error");
}
# ifdef _LIBC
libc_hidden_def (gai_strerror)
# endif
#endif  
