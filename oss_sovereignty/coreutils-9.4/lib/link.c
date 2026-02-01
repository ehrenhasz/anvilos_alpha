 
#  undef GetModuleHandle
#  define GetModuleHandle GetModuleHandleA
#  undef CreateHardLink
#  define CreateHardLink CreateHardLinkA

#  if !(_WIN32_WINNT >= _WIN32_WINNT_WINXP)

 
#   define GetProcAddress \
     (void *) GetProcAddress

 
typedef BOOL (WINAPI * CreateHardLinkFuncType) (LPCSTR lpFileName,
                                                LPCSTR lpExistingFileName,
                                                LPSECURITY_ATTRIBUTES lpSecurityAttributes);
static CreateHardLinkFuncType CreateHardLinkFunc = NULL;
static BOOL initialized = FALSE;

static void
initialize (void)
{
  HMODULE kernel32 = GetModuleHandle ("kernel32.dll");
  if (kernel32 != NULL)
    {
      CreateHardLinkFunc =
        (CreateHardLinkFuncType) GetProcAddress (kernel32, "CreateHardLinkA");
    }
  initialized = TRUE;
}

#  else

#   define CreateHardLinkFunc CreateHardLink

#  endif

int
link (const char *file1, const char *file2)
{
  char *dir;
  size_t len1 = strlen (file1);
  size_t len2 = strlen (file2);

#  if !(_WIN32_WINNT >= _WIN32_WINNT_WINXP)
  if (!initialized)
    initialize ();
#  endif

  if (CreateHardLinkFunc == NULL)
    {
       
      errno = EPERM;
      return -1;
    }
   
  if ((len1 && (file1[len1 - 1] == '/' || file1[len1 - 1] == '\\'))
      || (len2 && (file2[len2 - 1] == '/' || file2[len2 - 1] == '\\')))
    {
       
      struct stat st;
      if (stat (file1, &st))
        {
          if (errno == EOVERFLOW)
             
            errno = ENOTDIR;
          return -1;
        }
      if (!S_ISDIR (st.st_mode))
        errno = ENOTDIR;
      else
        errno = EPERM;
      return -1;
    }
   
  dir = strdup (file2);
  if (!dir)
    return -1;
  {
    struct stat st;
    char *p = strchr (dir, '\0');
    while (dir < p && (*--p != '/' && *p != '\\'));
    *p = '\0';
    if (p != dir && stat (dir, &st) != 0 && errno != EOVERFLOW)
      {
        free (dir);
        return -1;
      }
    free (dir);
  }
   
  if (CreateHardLinkFunc (file2, file1, NULL) == 0)
    {
       
      DWORD err = GetLastError ();
      switch (err)
        {
        case ERROR_ACCESS_DENIED:
          errno = EACCES;
          break;

        case ERROR_INVALID_FUNCTION:     
          errno = EPERM;
          break;

        case ERROR_NOT_SAME_DEVICE:
          errno = EXDEV;
          break;

        case ERROR_PATH_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
          errno = ENOENT;
          break;

        case ERROR_INVALID_PARAMETER:
          errno = ENAMETOOLONG;
          break;

        case ERROR_TOO_MANY_LINKS:
          errno = EMLINK;
          break;

        case ERROR_ALREADY_EXISTS:
          errno = EEXIST;
          break;

        default:
          errno = EIO;
        }
      return -1;
    }

  return 0;
}

# else  

#  error "This platform lacks a link function, and Gnulib doesn't provide a replacement. This is a bug in Gnulib."

# endif  
#else  

# undef link

 
int
rpl_link (char const *file1, char const *file2)
{
  size_t len1;
  size_t len2;
  struct stat st;

   
  if (lstat (file2, &st) == 0 || errno == EOVERFLOW)
    {
      errno = EEXIST;
      return -1;
    }

   
  len1 = strlen (file1);
  len2 = strlen (file2);
  if ((len1 && file1[len1 - 1] == '/')
      || (len2 && file2[len2 - 1] == '/'))
    {
       
      if (stat (file1, &st))
        return -1;
      if (!S_ISDIR (st.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
    }
  else
    {
       
      char *dir = strdup (file2);
      char *p;
      if (!dir)
        return -1;
       
      p = strrchr (dir, '/');
      if (p)
        {
          *p = '\0';
          if (stat (dir, &st) != 0 && errno != EOVERFLOW)
            {
              free (dir);
              return -1;
            }
        }
      free (dir);
    }
  return link (file1, file2);
}
#endif  
