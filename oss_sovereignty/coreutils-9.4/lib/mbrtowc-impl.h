 

 

  char *pstate = (char *)ps;

  if (s == NULL)
    {
      pwc = NULL;
      s = "";
      n = 1;
    }

  if (n == 0)
    return (size_t)(-2);

   

  if (pstate == NULL)
    pstate = internal_state;

  {
    size_t nstate = pstate[0];
    char buf[4];
    const char *p;
    size_t m;
    enc_t enc;
    int res;

    switch (nstate)
      {
      case 0:
        p = s;
        m = n;
        break;
      case 3:
        buf[2] = pstate[3];
        FALLTHROUGH;
      case 2:
        buf[1] = pstate[2];
        FALLTHROUGH;
      case 1:
        buf[0] = pstate[1];
        p = buf;
        m = nstate;
        buf[m++] = s[0];
        if (n >= 2 && m < 4)
          {
            buf[m++] = s[1];
            if (n >= 3 && m < 4)
              buf[m++] = s[2];
          }
        break;
      default:
        errno = EINVAL;
        return (size_t)(-1);
      }

     

    enc = locale_encoding_classification ();

    if (enc == enc_utf8)  
      {
         
#include "mbrtowc-impl-utf8.h"
      }
    else
      {
         
        wchar_t wc;
        res = mbtowc_with_lock (&wc, p, m);

        if (res >= 0)
          {
            if ((wc == 0) != (res == 0))
              abort ();
            if (pwc != NULL)
              *pwc = wc;
            goto success;
          }

         
        if (m >= 4 || m >= MB_CUR_MAX)
          goto invalid;
         
        switch (enc)
          {
           

          case enc_eucjp:  
            {
              if (m == 1)
                {
                  unsigned char c = (unsigned char) p[0];

                  if ((c >= 0xa1 && c < 0xff) || c == 0x8e || c == 0x8f)
                    goto incomplete;
                }
              if (m == 2)
                {
                  unsigned char c = (unsigned char) p[0];

                  if (c == 0x8f)
                    {
                      unsigned char c2 = (unsigned char) p[1];

                      if (c2 >= 0xa1 && c2 < 0xff)
                        goto incomplete;
                    }
                }
              goto invalid;
            }

          case enc_94:  
            {
              if (m == 1)
                {
                  unsigned char c = (unsigned char) p[0];

                  if (c >= 0xa1 && c < 0xff)
                    goto incomplete;
                }
              goto invalid;
            }

          case enc_euctw:  
            {
              if (m == 1)
                {
                  unsigned char c = (unsigned char) p[0];

                  if ((c >= 0xa1 && c < 0xff) || c == 0x8e)
                    goto incomplete;
                }
              else  
                {
                  unsigned char c = (unsigned char) p[0];

                  if (c == 0x8e)
                    goto incomplete;
                }
              goto invalid;
            }

          case enc_gb18030:  
            {
              if (m == 1)
                {
                  unsigned char c = (unsigned char) p[0];

                  if ((c >= 0x90 && c <= 0xe3) || (c >= 0xf8 && c <= 0xfe))
                    goto incomplete;
                }
              else  
                {
                  unsigned char c = (unsigned char) p[0];

                  if (c >= 0x90 && c <= 0xe3)
                    {
                      unsigned char c2 = (unsigned char) p[1];

                      if (c2 >= 0x30 && c2 <= 0x39)
                        {
                          if (m == 2)
                            goto incomplete;
                          else  
                            {
                              unsigned char c3 = (unsigned char) p[2];

                              if (c3 >= 0x81 && c3 <= 0xfe)
                                goto incomplete;
                            }
                        }
                    }
                }
              goto invalid;
            }

          case enc_sjis:  
            {
              if (m == 1)
                {
                  unsigned char c = (unsigned char) p[0];

                  if ((c >= 0x81 && c <= 0x9f) || (c >= 0xe0 && c <= 0xea)
                      || (c >= 0xf0 && c <= 0xf9))
                    goto incomplete;
                }
              goto invalid;
            }

          default:
             
            goto incomplete;
          }
      }

   success:
     
    if (nstate >= (res > 0 ? res : 1))
      abort ();
    res -= nstate;
    pstate[0] = 0;
    return res;

   incomplete:
    {
      size_t k = nstate;
       
      pstate[++k] = s[0];
      if (k < m)
        {
          pstate[++k] = s[1];
          if (k < m)
            pstate[++k] = s[2];
        }
      if (k != m)
        abort ();
    }
    pstate[0] = m;
    return (size_t)(-2);

   invalid:
    errno = EILSEQ;
     
    return (size_t)(-1);
  }
