 

 
static bool
dentry_exists (const char *filename)
{
  bool exists = false;
  DIR *dir = opendir (".");

  ASSERT (dir != NULL);
  for (;;)
    {
      struct dirent *d = readdir (dir);
      if (d == NULL)
        break;
      if (strcmp (d->d_name, filename) == 0)
        {
          exists = true;
          break;
        }
    }
  ASSERT (closedir (dir) == 0);
  return exists;
}

 
static void
assert_nonexistent (const char *filename)
{
  struct stat st;

   
  errno = 0;
  if (stat (filename, &st) == -1)
    ASSERT (errno == ENOENT);
  else
    {
       
      ASSERT (!dentry_exists (filename));
       
      (void) rmdir (filename);
    }
}

static int
test_rename (int (*func) (char const *, char const *), bool print)
{
   
  struct stat st;
  int fd = creat (BASE "file", 0600);
  ASSERT (0 <= fd);
  ASSERT (write (fd, "hi", 2) == 2);
  ASSERT (close (fd) == 0);
  ASSERT (mkdir (BASE "dir", 0700) == 0);

   

   

  {  
    {
      errno = 0;
      ASSERT (func (BASE "missing", BASE "missing") == -1);
      ASSERT (errno == ENOENT);
    }
    {
      errno = 0;
      ASSERT (func (BASE "missing/", BASE "missing") == -1);
      ASSERT (errno == ENOENT);
    }
    {
      errno = 0;
      ASSERT (func (BASE "missing", BASE "missing/") == -1);
      ASSERT (errno == ENOENT);
    }
  }
  {  
    {
      errno = 0;
      ASSERT (func ("", BASE "missing") == -1);
      ASSERT (errno == ENOENT);
    }
    {
      errno = 0;
      ASSERT (func (BASE "file", "") == -1);
      ASSERT (errno == ENOENT);
    }
    {
      errno = 0;
      ASSERT (func (BASE "", "") == -1);
      ASSERT (errno == ENOENT);
    }
  }

   

  {  
    {
      errno = 0;
      ASSERT (func (BASE "file", BASE "file2/") == -1);
      ASSERT (errno == ENOENT || errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "file/", BASE "file2") == -1);
      ASSERT (errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (stat (BASE "file2", &st) == -1);
      ASSERT (errno == ENOENT);
    }
  }
  {  
    ASSERT (func (BASE "file", BASE "file2") == 0);
    errno = 0;
    ASSERT (stat (BASE "file", &st) == -1);
    ASSERT (errno == ENOENT);
    memset (&st, 0, sizeof st);
    ASSERT (stat (BASE "file2", &st) == 0);
    ASSERT (st.st_size == 2);
  }
   
  {  
    ASSERT (close (creat (BASE "file", 0600)) == 0);
    errno = 0;
    ASSERT (func (BASE "file2", BASE "file/") == -1);
    ASSERT (errno == ENOTDIR);
    ASSERT (func (BASE "file2", BASE "file") == 0);
    memset (&st, 0, sizeof st);
    ASSERT (stat (BASE "file", &st) == 0);
    ASSERT (st.st_size == 2);
    errno = 0;
    ASSERT (stat (BASE "file2", &st) == -1);
    ASSERT (errno == ENOENT);
  }
   

   

  {  
    {
      ASSERT (func (BASE "dir", BASE "dir2/") == 0);
      errno = 0;
      ASSERT (stat (BASE "dir", &st) == -1);
      ASSERT (errno == ENOENT);
      ASSERT (stat (BASE "dir2", &st) == 0);
    }
     
    {
      ASSERT (func (BASE "dir2/", BASE "dir") == 0);
      ASSERT (stat (BASE "dir", &st) == 0);
      errno = 0;
      ASSERT (stat (BASE "dir2", &st) == -1);
      ASSERT (errno == ENOENT);
    }
     
    {
      ASSERT (func (BASE "dir", BASE "dir2") == 0);
      errno = 0;
      ASSERT (stat (BASE "dir", &st) == -1);
      ASSERT (errno == ENOENT);
      ASSERT (stat (BASE "dir2", &st) == 0);
    }
     
    {  
      ASSERT (mkdir (BASE "dir", 0700) == 0);
       
      ASSERT (func (BASE "dir2", BASE "dir") == 0);
       
      ASSERT (mkdir (BASE "dir2", 0700) == 0);
       
      ASSERT (func (BASE "dir2", BASE "dir/") == 0);
       
      ASSERT (mkdir (BASE "dir2", 0700) == 0);
       
      ASSERT (func (BASE "dir2/", BASE "dir") == 0);
       
      ASSERT (mkdir (BASE "dir2", 0700) == 0);
    }
     
    {  
      ASSERT (close (creat (BASE "dir/file", 0600)) == 0);
       
      {
        errno = 0;
        ASSERT (func (BASE "dir2", BASE "dir") == -1);
        ASSERT (errno == EEXIST || errno == ENOTEMPTY);
      }
      {
        errno = 0;
        ASSERT (func (BASE "dir2/", BASE "dir") == -1);
        ASSERT (errno == EEXIST || errno == ENOTEMPTY);
      }
      {
        errno = 0;
        ASSERT (func (BASE "dir2", BASE "dir/") == -1);
        ASSERT (errno == EEXIST || errno == ENOTEMPTY);
      }
    }
    {  
      ASSERT (func (BASE "dir", BASE "dir2") == 0);
      assert_nonexistent (BASE "dir");
      ASSERT (stat (BASE "dir2/file", &st) == 0);
       
      ASSERT (mkdir (BASE "dir", 0700) == 0);
       
      {
        ASSERT (func (BASE "dir2/", BASE "dir") == 0);
        ASSERT (stat (BASE "dir/file", &st) == 0);
        errno = 0;
        ASSERT (stat (BASE "dir2", &st) == -1);
        ASSERT (errno == ENOENT);
      }
       
      ASSERT (mkdir (BASE "dir2", 0700) == 0);
       
      {
        ASSERT (func (BASE "dir", BASE "dir2/") == 0);
        assert_nonexistent (BASE "dir");
        ASSERT (stat (BASE "dir2/file", &st) == 0);
      }
       
      ASSERT (unlink (BASE "dir2/file") == 0);
    }
     
    {  
      {
        errno = 0;
        ASSERT (func (BASE "dir2", BASE "dir/.") == -1);
        ASSERT (errno == EINVAL || errno == ENOENT);
      }
      ASSERT (mkdir (BASE "dir", 0700) == 0);
       
      {
        errno = 0;
        ASSERT (func (BASE "dir2", BASE "dir/.") == -1);
        ASSERT (errno == EINVAL || errno == EBUSY || errno == EISDIR
                || errno == ENOTEMPTY || errno == EEXIST
                || errno == ENOENT  );
      }
      {
        errno = 0;
        ASSERT (func (BASE "dir2/.", BASE "dir") == -1);
        ASSERT (errno == EINVAL || errno == EBUSY || errno == EEXIST
                || errno == ENOENT  );
      }
      ASSERT (rmdir (BASE "dir") == 0);
       
      {
        errno = 0;
        ASSERT (func (BASE "dir2", BASE "dir/.//") == -1);
        ASSERT (errno == EINVAL || errno == ENOENT);
      }
      ASSERT (mkdir (BASE "dir", 0700) == 0);
       
      {
        errno = 0;
        ASSERT (func (BASE "dir2", BASE "dir/.//") == -1);
        ASSERT (errno == EINVAL || errno == EBUSY || errno == EISDIR
                || errno == ENOTEMPTY || errno == EEXIST
                || errno == ENOENT  );
      }
      {
        errno = 0;
        ASSERT (func (BASE "dir2/.//", BASE "dir") == -1);
        ASSERT (errno == EINVAL || errno == EBUSY || errno == EEXIST
                || errno == ENOENT  );
      }
      ASSERT (rmdir (BASE "dir2") == 0);
       
    }
    {  
      {
        errno = 0;
        ASSERT (func (BASE "dir", BASE "dir/sub") == -1);
        ASSERT (errno == EINVAL || errno == EACCES);
      }
      {
        errno = 0;
        ASSERT (stat (BASE "dir/sub", &st) == -1);
        ASSERT (errno == ENOENT);
      }
      ASSERT (mkdir (BASE "dir/sub", 0700) == 0);
       
      {
        errno = 0;
        ASSERT (func (BASE "dir", BASE "dir/sub") == -1);
        ASSERT (errno == EINVAL);
        ASSERT (stat (BASE "dir/sub", &st) == 0);
      }
      ASSERT (rmdir (BASE "dir/sub") == 0);
    }
  }
   

   

  {
    {  
      {
        errno = 0;
        ASSERT (func (BASE "file", BASE "dir") == -1);
        ASSERT (errno == EISDIR || errno == ENOTDIR);
      }
      {
        errno = 0;
        ASSERT (func (BASE "file", BASE "dir/") == -1);
        ASSERT (errno == EISDIR || errno == ENOTDIR);
      }
    }
    {  
      {
        errno = 0;
        ASSERT (func (BASE "dir", BASE "file") == -1);
        ASSERT (errno == ENOTDIR);
      }
      {
        errno = 0;
        ASSERT (func (BASE "dir/", BASE "file") == -1);
        ASSERT (errno == ENOTDIR);
      }
    }
  }

   

  {  
    ASSERT (func (BASE "file", BASE "file") == 0);
    memset (&st, 0, sizeof st);
    ASSERT (stat (BASE "file", &st) == 0);
    ASSERT (st.st_size == 2);
  }
   
  {  
    ASSERT (func (BASE "dir", BASE "dir") == 0);
    ASSERT (stat (BASE "dir", &st) == 0);
  }
   
  ASSERT (close (creat (BASE "dir/file", 0600)) == 0);
   
  {  
    ASSERT (func (BASE "dir", BASE "dir") == 0);
  }
  ASSERT (unlink (BASE "dir/file") == 0);
   
  {
     
    int ret = link (BASE "file", BASE "file2");
    if (!ret)
      {
        memset (&st, 0, sizeof st);
        ASSERT (stat (BASE "file2", &st) == 0);
        if (st.st_ino && st.st_nlink != 2)
          {
            ASSERT (unlink (BASE "file2") == 0);
            errno = EPERM;
            ret = -1;
          }
      }
    if (ret == -1)
      {
         
        switch (errno)
          {
          case EPERM:
          case EOPNOTSUPP:
          #if defined __ANDROID__
          case EACCES:
          #endif
            if (print)
              fputs ("skipping test: "
                     "hard links not supported on this file system\n",
                     stderr);
            ASSERT (unlink (BASE "file") == 0);
            ASSERT (rmdir (BASE "dir") == 0);
            return 77;
          default:
            perror ("link");
            return 1;
          }
      }
    ASSERT (ret == 0);
  }
   
  {  
    ASSERT (func (BASE "file", BASE "file2") == 0);
    memset (&st, 0, sizeof st);
    if (stat (BASE "file", &st) != 0)
      {
         
        ASSERT (errno == ENOENT);
        ASSERT (link (BASE "file2", BASE "file") == 0);
        ASSERT (stat (BASE "file", &st) == 0);
      }
    ASSERT (st.st_size == 2);
    memset (&st, 0, sizeof st);
    ASSERT (stat (BASE "file2", &st) == 0);
    ASSERT (st.st_size == 2);
  }
   
  ASSERT (unlink (BASE "file2") == 0);
   

   

  if (symlink (BASE "file", BASE "link1"))
    {
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      ASSERT (unlink (BASE "file") == 0);
      ASSERT (rmdir (BASE "dir") == 0);
      return 77;
    }
   
  {  
    ASSERT (func (BASE "link1", BASE "link2") == 0);
    ASSERT (stat (BASE "file", &st) == 0);
    errno = 0;
    ASSERT (lstat (BASE "link1", &st) == -1);
    ASSERT (errno == ENOENT);
    memset (&st, 0, sizeof st);
    ASSERT (lstat (BASE "link2", &st) == 0);
    ASSERT (S_ISLNK (st.st_mode));
  }
   
  {  
    ASSERT (symlink (BASE "nowhere", BASE "link1") == 0);
     
    {
      ASSERT (func (BASE "link2", BASE "link1") == 0);
      memset (&st, 0, sizeof st);
      ASSERT (stat (BASE "link1", &st) == 0);
      ASSERT (st.st_size == 2);
      errno = 0;
      ASSERT (lstat (BASE "link2", &st) == -1);
      ASSERT (errno == ENOENT);
    }
  }
   
  {  
    ASSERT (symlink (BASE "link2", BASE "link2") == 0);
     
    {
      ASSERT (func (BASE "link2", BASE "link2") == 0);
    }
    {
      errno = 0;
      ASSERT (func (BASE "link2/", BASE "link2") == -1);
      ASSERT (errno == ELOOP || errno == ENOTDIR);
    }
    ASSERT (func (BASE "link2", BASE "link3") == 0);
     
    ASSERT (unlink (BASE "link3") == 0);
  }
   
  {  
    ASSERT (symlink (BASE "nowhere", BASE "link2") == 0);
     
    {
      ASSERT (func (BASE "link2", BASE "link3") == 0);
      errno = 0;
      ASSERT (lstat (BASE "link2", &st) == -1);
      ASSERT (errno == ENOENT);
      memset (&st, 0, sizeof st);
      ASSERT (lstat (BASE "link3", &st) == 0);
    }
  }
   
  {  
    {
      errno = 0;
      ASSERT (func (BASE "link3/", BASE "link2") == -1);
      ASSERT (errno == ENOENT || errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "link3", BASE "link2/") == -1);
      ASSERT (errno == ENOENT || errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (lstat (BASE "link2", &st) == -1);
      ASSERT (errno == ENOENT);
    }
    memset (&st, 0, sizeof st);
    ASSERT (lstat (BASE "link3", &st) == 0);
  }
   
  {  
    {
      errno = 0;
      ASSERT (func (BASE "link1/", BASE "link2") == -1);
      ASSERT (errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "link1", BASE "link3/") == -1);
      ASSERT (errno == ENOENT || errno == ENOTDIR);
    }
  }
   

   

  {  
    ASSERT (close (creat (BASE "file2", 0600)) == 0);
     
    {
      ASSERT (func (BASE "file2", BASE "link3") == 0);
      errno = 0;
      ASSERT (stat (BASE "file2", &st) == -1);
      ASSERT (errno == ENOENT);
      memset (&st, 0, sizeof st);
      ASSERT (lstat (BASE "link3", &st) == 0);
      ASSERT (S_ISREG (st.st_mode));
    }
     
    ASSERT (unlink (BASE "link3") == 0);
  }
   
  {  
    ASSERT (symlink (BASE "nowhere", BASE "link2") == 0);
     
    ASSERT (close (creat (BASE "file2", 0600)) == 0);
     
    {
      ASSERT (func (BASE "link2", BASE "file2") == 0);
      errno = 0;
      ASSERT (lstat (BASE "link2", &st) == -1);
      ASSERT (errno == ENOENT);
      memset (&st, 0, sizeof st);
      ASSERT (lstat (BASE "file2", &st) == 0);
      ASSERT (S_ISLNK (st.st_mode));
    }
     
    ASSERT (unlink (BASE "file2") == 0);
  }
   
  {  
    {
      errno = 0;
      ASSERT (func (BASE "file/", BASE "link1") == -1);
      ASSERT (errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "file", BASE "link1/") == -1);
      ASSERT (errno == ENOTDIR || errno == ENOENT);
    }
    {
      errno = 0;
      ASSERT (func (BASE "link1/", BASE "file") == -1);
      ASSERT (errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "link1", BASE "file/") == -1);
      ASSERT (errno == ENOTDIR || errno == ENOENT);
      memset (&st, 0, sizeof st);
      ASSERT (lstat (BASE "file", &st) == 0);
      ASSERT (S_ISREG (st.st_mode));
      memset (&st, 0, sizeof st);
      ASSERT (lstat (BASE "link1", &st) == 0);
      ASSERT (S_ISLNK (st.st_mode));
    }
  }
   

   

  {  
    {
      errno = 0;
      ASSERT (func (BASE "dir", BASE "link1") == -1);
      ASSERT (errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "dir/", BASE "link1") == -1);
      ASSERT (errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "dir", BASE "link1/") == -1);
      ASSERT (errno == ENOTDIR);
    }
  }
  {  
    {
      errno = 0;
      ASSERT (func (BASE "link1", BASE "dir") == -1);
      ASSERT (errno == EISDIR || errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "link1", BASE "dir/") == -1);
      ASSERT (errno == EISDIR || errno == ENOTDIR);
    }
    {
      errno = 0;
      ASSERT (func (BASE "link1/", BASE "dir") == -1);
      ASSERT (errno == ENOTDIR);
      memset (&st, 0, sizeof st);
      ASSERT (lstat (BASE "link1", &st) == 0);
      ASSERT (S_ISLNK (st.st_mode));
      memset (&st, 0, sizeof st);
      ASSERT (lstat (BASE "dir", &st) == 0);
      ASSERT (S_ISDIR (st.st_mode));
    }
  }
   

   
  {
    int result;
    ASSERT (symlink (BASE "dir2", BASE "link2") == 0);
     
    errno = 0;
    result = func (BASE "dir", BASE "link2/");
    if (result == 0)
      {
         
        errno = 0;
        ASSERT (lstat (BASE "dir", &st) == -1);
        ASSERT (errno == ENOENT);
        memset (&st, 0, sizeof st);
        ASSERT (lstat (BASE "dir2", &st) == 0);
        ASSERT (S_ISDIR (st.st_mode));
        memset (&st, 0, sizeof st);
        ASSERT (lstat (BASE "link2", &st) == 0);
        ASSERT (S_ISLNK (st.st_mode));
         
        {
          ASSERT (func (BASE "link2/", BASE "dir") == 0);
          memset (&st, 0, sizeof st);
          ASSERT (lstat (BASE "dir", &st) == 0);
          ASSERT (S_ISDIR (st.st_mode));
          errno = 0;
          ASSERT (lstat (BASE "dir2", &st) == -1);
          ASSERT (errno == ENOENT);
          memset (&st, 0, sizeof st);
          ASSERT (lstat (BASE "link2", &st) == 0);
          ASSERT (S_ISLNK (st.st_mode));
        }
      }
    else
      {
         
        ASSERT (result == -1);
        ASSERT (errno == ENOTDIR);
        memset (&st, 0, sizeof st);
        ASSERT (lstat (BASE "dir", &st) == 0);
        ASSERT (S_ISDIR (st.st_mode));
        errno = 0;
        ASSERT (lstat (BASE "dir2", &st) == -1);
        ASSERT (errno == ENOENT);
        memset (&st, 0, sizeof st);
        ASSERT (lstat (BASE "link2", &st) == 0);
        ASSERT (S_ISLNK (st.st_mode));
        ASSERT (unlink (BASE "link2") == 0);
        ASSERT (symlink (BASE "dir", BASE "link2") == 0);
         
        errno = 0;  
        result = func (BASE "link2/", BASE "dir");
        if (result)  
          {
            ASSERT (result == -1);
            ASSERT (errno == ENOTDIR || errno == EISDIR);
          }
        memset (&st, 0, sizeof st);
        ASSERT (lstat (BASE "dir", &st) == 0);
        ASSERT (S_ISDIR (st.st_mode));
        errno = 0;
        ASSERT (lstat (BASE "dir2", &st) == -1);
        ASSERT (errno == ENOENT);
        memset (&st, 0, sizeof st);
        ASSERT (lstat (BASE "link2", &st) == 0);
        ASSERT (S_ISLNK (st.st_mode));
      }
  }
   

   
  ASSERT (unlink (BASE "file") == 0);
  ASSERT (rmdir (BASE "dir") == 0);
  ASSERT (unlink (BASE "link1") == 0);
  ASSERT (unlink (BASE "link2") == 0);

  return 0;
}
