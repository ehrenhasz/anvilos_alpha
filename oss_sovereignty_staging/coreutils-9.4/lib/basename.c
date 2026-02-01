 
      length += ISSLASH (base[length]);

       
      dotslash_len = FILE_SYSTEM_PREFIX_LEN (base) != 0 ? 2 : 0;
    }
  else
    {
       
      base = name;
      length = base_len (base);
      dotslash_len = 0;
    }

  char *p = ximalloc (dotslash_len + length + 1);
  if (dotslash_len)
    {
      p[0] = '.';
      p[1] = '/';
    }

   
  memcpy (p + dotslash_len, base, length);
  p[dotslash_len + length] = '\0';
  return p;
}
