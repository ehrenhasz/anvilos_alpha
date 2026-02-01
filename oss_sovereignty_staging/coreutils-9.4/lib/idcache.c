 

static struct userid *user_alist;

 
static struct userid *nouser_alist;

 
static struct userid *group_alist;

 
static struct userid *nogroup_alist;

 

char *
getuser (uid_t uid)
{
  struct userid *tail;
  struct userid *match = NULL;

  for (tail = user_alist; tail; tail = tail->next)
    {
      if (tail->id.u == uid)
        {
          match = tail;
          break;
        }
    }

  if (match == NULL)
    {
      struct passwd *pwent = getpwuid (uid);
      char const *name = pwent ? pwent->pw_name : "";
      match = xmalloc (FLEXSIZEOF (struct userid, name, strlen (name) + 1));
      match->id.u = uid;
      strcpy (match->name, name);

       
      match->next = user_alist;
      user_alist = match;
    }

  return match->name[0] ? match->name : NULL;
}

 

uid_t *
getuidbyname (const char *user)
{
  struct userid *tail;
  struct passwd *pwent;

  for (tail = user_alist; tail; tail = tail->next)
     
    if (*tail->name == *user && !strcmp (tail->name, user))
      return &tail->id.u;

  for (tail = nouser_alist; tail; tail = tail->next)
     
    if (*tail->name == *user && !strcmp (tail->name, user))
      return NULL;

  pwent = getpwnam (user);
#ifdef __DJGPP__
   
  if (!pwent && strspn (user, digits) < strlen (user))
    {
      setenv ("USER", user, 1);
      pwent = getpwnam (user);   
    }
#endif

  tail = xmalloc (FLEXSIZEOF (struct userid, name, strlen (user) + 1));
  strcpy (tail->name, user);

   
  if (pwent)
    {
      tail->id.u = pwent->pw_uid;
      tail->next = user_alist;
      user_alist = tail;
      return &tail->id.u;
    }

  tail->next = nouser_alist;
  nouser_alist = tail;
  return NULL;
}

 

char *
getgroup (gid_t gid)
{
  struct userid *tail;
  struct userid *match = NULL;

  for (tail = group_alist; tail; tail = tail->next)
    {
      if (tail->id.g == gid)
        {
          match = tail;
          break;
        }
    }

  if (match == NULL)
    {
      struct group *grent = getgrgid (gid);
      char const *name = grent ? grent->gr_name : "";
      match = xmalloc (FLEXSIZEOF (struct userid, name, strlen (name) + 1));
      match->id.g = gid;
      strcpy (match->name, name);

       
      match->next = group_alist;
      group_alist = match;
    }

  return match->name[0] ? match->name : NULL;
}

 

gid_t *
getgidbyname (const char *group)
{
  struct userid *tail;
  struct group *grent;

  for (tail = group_alist; tail; tail = tail->next)
     
    if (*tail->name == *group && !strcmp (tail->name, group))
      return &tail->id.g;

  for (tail = nogroup_alist; tail; tail = tail->next)
     
    if (*tail->name == *group && !strcmp (tail->name, group))
      return NULL;

  grent = getgrnam (group);
#ifdef __DJGPP__
   
  if (!grent && strspn (group, digits) < strlen (group))
    {
      setenv ("GROUP", group, 1);
      grent = getgrnam (group);  
    }
#endif

  tail = xmalloc (FLEXSIZEOF (struct userid, name, strlen (group) + 1));
  strcpy (tail->name, group);

   
  if (grent)
    {
      tail->id.g = grent->gr_gid;
      tail->next = group_alist;
      group_alist = tail;
      return &tail->id.g;
    }

  tail->next = nogroup_alist;
  nogroup_alist = tail;
  return NULL;
}
