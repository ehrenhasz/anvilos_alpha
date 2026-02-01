 

    static signed char ok;

    if (! ok)
      {
        struct utsname name;
        uname (&name);
        char *p = name.release;
        ok = ((p[1] != '.' || '5' < p[0]
               || (p[0] == '5' && (p[3] != '.' || '2' < p[2])))
              ? 1 : -1);
      }

    if (0 < ok)
      return copy_file_range (infd, pinoff, outfd, poutoff, length, flags);
#endif

   
  errno = ENOSYS;
  return -1;
}
