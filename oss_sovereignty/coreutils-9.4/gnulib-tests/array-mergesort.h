 

 
static void
merge (const ELEMENT *src1, size_t n1,
       const ELEMENT *src2, size_t n2,
       ELEMENT *dst)
{
  for (;;)  
    {
      if (COMPARE (src1, src2) <= 0)
        {
          *dst++ = *src1++;
          n1--;
          if (n1 == 0)
            break;
        }
      else
        {
          *dst++ = *src2++;
          n2--;
          if (n2 == 0)
            break;
        }
    }
   
  if (n1 > 0)
    {
      if (dst != src1)
        do
          {
            *dst++ = *src1++;
            n1--;
          }
        while (n1 > 0);
    }
  else  
    {
      if (dst != src2)
        do
          {
            *dst++ = *src2++;
            n2--;
          }
        while (n2 > 0);
    }
}

 
#ifdef STATIC_FROMTO
STATIC_FROMTO
#else
STATIC
#endif
void
merge_sort_fromto (const ELEMENT *src, ELEMENT *dst, size_t n, ELEMENT *tmp)
{
  switch (n)
    {
    case 0:
      return;
    case 1:
       
      dst[0] = src[0];
      return;
    case 2:
       
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
           
          dst[0] = src[0];
          dst[1] = src[1];
        }
      else
        {
          dst[0] = src[1];
          dst[1] = src[0];
        }
      break;
    case 3:
       
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
          if (COMPARE (&src[1], &src[2]) <= 0)
            {
               
              dst[0] = src[0];
              dst[1] = src[1];
              dst[2] = src[2];
            }
          else if (COMPARE (&src[0], &src[2]) <= 0)
            {
               
              dst[0] = src[0];
              dst[1] = src[2];
              dst[2] = src[1];
            }
          else
            {
               
              dst[0] = src[2];
              dst[1] = src[0];
              dst[2] = src[1];
            }
        }
      else
        {
          if (COMPARE (&src[0], &src[2]) <= 0)
            {
               
              dst[0] = src[1];
              dst[1] = src[0];
              dst[2] = src[2];
            }
          else if (COMPARE (&src[1], &src[2]) <= 0)
            {
               
              dst[0] = src[1];
              dst[1] = src[2];
              dst[2] = src[0];
            }
          else
            {
               
              dst[0] = src[2];
              dst[1] = src[1];
              dst[2] = src[0];
            }
        }
      break;
    default:
      {
        size_t n1 = n / 2;
        size_t n2 = (n + 1) / 2;
         
         
        merge_sort_fromto (src + n1, dst + n1, n2, tmp);
         
        merge_sort_fromto (src, tmp, n1, dst);
         
        merge (tmp, n1, dst + n1, n2, dst);
      }
      break;
    }
}

 
STATIC void
merge_sort_inplace (ELEMENT *src, size_t n, ELEMENT *tmp)
{
  switch (n)
    {
    case 0:
    case 1:
       
      return;
    case 2:
       
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
           
        }
      else
        {
          ELEMENT t = src[0];
          src[0] = src[1];
          src[1] = t;
        }
      break;
    case 3:
       
      if (COMPARE (&src[0], &src[1]) <= 0)
        {
          if (COMPARE (&src[1], &src[2]) <= 0)
            {
               
            }
          else if (COMPARE (&src[0], &src[2]) <= 0)
            {
               
              ELEMENT t = src[1];
              src[1] = src[2];
              src[2] = t;
            }
          else
            {
               
              ELEMENT t = src[0];
              src[0] = src[2];
              src[2] = src[1];
              src[1] = t;
            }
        }
      else
        {
          if (COMPARE (&src[0], &src[2]) <= 0)
            {
               
              ELEMENT t = src[0];
              src[0] = src[1];
              src[1] = t;
            }
          else if (COMPARE (&src[1], &src[2]) <= 0)
            {
               
              ELEMENT t = src[0];
              src[0] = src[1];
              src[1] = src[2];
              src[2] = t;
            }
          else
            {
               
              ELEMENT t = src[0];
              src[0] = src[2];
              src[2] = t;
            }
        }
      break;
    default:
      {
        size_t n1 = n / 2;
        size_t n2 = (n + 1) / 2;
         
         
        merge_sort_inplace (src + n1, n2, tmp);
         
        merge_sort_fromto (src, tmp, n1, tmp + n1);
         
        merge (tmp, n1, src + n1, n2, src);
      }
      break;
    }
}

#undef ELEMENT
#undef COMPARE
#undef STATIC
