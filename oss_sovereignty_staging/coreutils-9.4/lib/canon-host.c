 
static int last_cherror;

 
char *
canon_host (const char *host)
{
  return canon_host_r (host, &last_cherror);
}

 
char *
canon_host_r (char const *host, int *cherror)
{
  char *retval = NULL;
  static struct addrinfo hints;
  struct addrinfo *res = NULL;
  int status;

  hints.ai_flags = AI_CANONNAME;
  status = getaddrinfo (host, NULL, &hints, &res);
  if (!status)
    {
       
      retval = strdup (res->ai_canonname ? res->ai_canonname : host);
      if (!retval && cherror)
        *cherror = EAI_MEMORY;
      freeaddrinfo (res);
    }
  else if (cherror)
    *cherror = status;

  return retval;
}

 
const char *
ch_strerror (void)
{
  return gai_strerror (last_cherror);
}
