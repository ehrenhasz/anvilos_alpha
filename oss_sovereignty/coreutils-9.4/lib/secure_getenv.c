 
  return __secure_getenv (name);
#elif HAVE_ISSETUGID  
  if (issetugid ())
    return NULL;
  return getenv (name);
#elif HAVE_GETUID && HAVE_GETEUID && HAVE_GETGID && HAVE_GETEGID  
  if (geteuid () != getuid () || getegid () != getgid ())
    return NULL;
  return getenv (name);
#elif defined _WIN32 && ! defined __CYGWIN__  
   
  return getenv (name);
#else
  return NULL;
#endif
}
