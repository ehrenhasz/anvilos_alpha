 
            if (src[0] == '\0')
              src_avail = 1;
            else if (src[1] == '\0')
              src_avail = 2;
            else if (src[2] == '\0')
              src_avail = 3;
            else if (MB_LEN_MAX <= 4 || src[3] == '\0')
              src_avail = 4;
            else
              src_avail = 4 + strnlen1 (src + 4, MB_LEN_MAX - 4);

             
            ret = MBRTOWC (destptr, src, src_avail, ps);

            if (ret == (size_t)(-2))
               
              abort ();

            if (ret == (size_t)(-1))
              goto bad_input;
            if (ret == 0)
              {
                src = NULL;
                 
                break;
              }
            if (!(USES_C32 && ret == (size_t)(-3)))
              src += ret;
          }

        *srcp = src;
        return destptr - dest;
      }
    else
      {
         
        mbstate_t state = *ps;
        size_t totalcount = 0;

        for (;; totalcount++)
          {
            size_t src_avail;
            size_t ret;

             
            if (src[0] == '\0')
              src_avail = 1;
            else if (src[1] == '\0')
              src_avail = 2;
            else if (src[2] == '\0')
              src_avail = 3;
            else if (MB_LEN_MAX <= 4 || src[3] == '\0')
              src_avail = 4;
            else
              src_avail = 4 + strnlen1 (src + 4, MB_LEN_MAX - 4);

             
            ret = MBRTOWC (NULL, src, src_avail, &state);

            if (ret == (size_t)(-2))
               
              abort ();

            if (ret == (size_t)(-1))
              goto bad_input2;
            if (ret == 0)
              {
                 
                break;
              }
            src += ret;
          }

        return totalcount;
      }

   bad_input:
    *srcp = src;
   bad_input2:
    errno = EILSEQ;
    return (size_t)(-1);
  }
}
