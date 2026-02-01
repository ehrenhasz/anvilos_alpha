 

#include <limits.h>
#include <stdint.h>

 

 
#if CHAR_BIT < 10
# define LONG_NEEDLE_THRESHOLD 32U
#else
# define LONG_NEEDLE_THRESHOLD SIZE_MAX
#endif

#ifndef MAX
# define MAX(a, b) ((a < b) ? (b) : (a))
#endif

#ifndef CANON_ELEMENT
# define CANON_ELEMENT(c) c
#endif
#ifndef CMP_FUNC
# define CMP_FUNC memcmp
#endif

 
static size_t
critical_factorization (const unsigned char *needle, size_t needle_len,
                        size_t *period)
{
   
  size_t max_suffix, max_suffix_rev;
  size_t j;  
  size_t k;  
  size_t p;  
  unsigned char a, b;  

   
  if (needle_len < 3)
    {
      *period = 1;
      return needle_len - 1;
    }

   

   
  max_suffix = SIZE_MAX;
  j = 0;
  k = p = 1;
  while (j + k < needle_len)
    {
      a = CANON_ELEMENT (needle[j + k]);
      b = CANON_ELEMENT (needle[max_suffix + k]);
      if (a < b)
        {
           
          j += k;
          k = 1;
          p = j - max_suffix;
        }
      else if (a == b)
        {
           
          if (k != p)
            ++k;
          else
            {
              j += p;
              k = 1;
            }
        }
      else  
        {
           
          max_suffix = j++;
          k = p = 1;
        }
    }
  *period = p;

   
  max_suffix_rev = SIZE_MAX;
  j = 0;
  k = p = 1;
  while (j + k < needle_len)
    {
      a = CANON_ELEMENT (needle[j + k]);
      b = CANON_ELEMENT (needle[max_suffix_rev + k]);
      if (b < a)
        {
           
          j += k;
          k = 1;
          p = j - max_suffix_rev;
        }
      else if (a == b)
        {
           
          if (k != p)
            ++k;
          else
            {
              j += p;
              k = 1;
            }
        }
      else  
        {
           
          max_suffix_rev = j++;
          k = p = 1;
        }
    }

   
  if (max_suffix_rev + 1 < max_suffix + 1)
    return max_suffix + 1;
  *period = p;
  return max_suffix_rev + 1;
}

 
static RETURN_TYPE _GL_ATTRIBUTE_PURE
two_way_short_needle (const unsigned char *haystack, size_t haystack_len,
                      const unsigned char *needle, size_t needle_len)
{
  size_t i;  
  size_t j;  
  size_t period;  
  size_t suffix;  

   
  suffix = critical_factorization (needle, needle_len, &period);

   
  if (CMP_FUNC (needle, needle + period, suffix) == 0)
    {
       
      size_t memory = 0;
      j = 0;
      while (AVAILABLE (haystack, haystack_len, j, needle_len))
        {
           
          i = MAX (suffix, memory);
          while (i < needle_len && (CANON_ELEMENT (needle[i])
                                    == CANON_ELEMENT (haystack[i + j])))
            ++i;
          if (needle_len <= i)
            {
               
              i = suffix - 1;
              while (memory < i + 1 && (CANON_ELEMENT (needle[i])
                                        == CANON_ELEMENT (haystack[i + j])))
                --i;
              if (i + 1 < memory + 1)
                return (RETURN_TYPE) (haystack + j);
               
              j += period;
              memory = needle_len - period;
            }
          else
            {
              j += i - suffix + 1;
              memory = 0;
            }
        }
    }
  else
    {
       
      period = MAX (suffix, needle_len - suffix) + 1;
      j = 0;
      while (AVAILABLE (haystack, haystack_len, j, needle_len))
        {
           
          i = suffix;
          while (i < needle_len && (CANON_ELEMENT (needle[i])
                                    == CANON_ELEMENT (haystack[i + j])))
            ++i;
          if (needle_len <= i)
            {
               
              i = suffix - 1;
              while (i != SIZE_MAX && (CANON_ELEMENT (needle[i])
                                       == CANON_ELEMENT (haystack[i + j])))
                --i;
              if (i == SIZE_MAX)
                return (RETURN_TYPE) (haystack + j);
              j += period;
            }
          else
            j += i - suffix + 1;
        }
    }
  return NULL;
}

 
static RETURN_TYPE _GL_ATTRIBUTE_PURE
two_way_long_needle (const unsigned char *haystack, size_t haystack_len,
                     const unsigned char *needle, size_t needle_len)
{
  size_t i;  
  size_t j;  
  size_t period;  
  size_t suffix;  
  size_t shift_table[1U << CHAR_BIT];  

   
  suffix = critical_factorization (needle, needle_len, &period);

   
  for (i = 0; i < 1U << CHAR_BIT; i++)
    shift_table[i] = needle_len;
  for (i = 0; i < needle_len; i++)
    shift_table[CANON_ELEMENT (needle[i])] = needle_len - i - 1;

   
  if (CMP_FUNC (needle, needle + period, suffix) == 0)
    {
       
      size_t memory = 0;
      size_t shift;
      j = 0;
      while (AVAILABLE (haystack, haystack_len, j, needle_len))
        {
           
          shift = shift_table[CANON_ELEMENT (haystack[j + needle_len - 1])];
          if (0 < shift)
            {
              if (memory && shift < period)
                {
                   
                  shift = needle_len - period;
                }
              memory = 0;
              j += shift;
              continue;
            }
           
          i = MAX (suffix, memory);
          while (i < needle_len - 1 && (CANON_ELEMENT (needle[i])
                                        == CANON_ELEMENT (haystack[i + j])))
            ++i;
          if (needle_len - 1 <= i)
            {
               
              i = suffix - 1;
              while (memory < i + 1 && (CANON_ELEMENT (needle[i])
                                        == CANON_ELEMENT (haystack[i + j])))
                --i;
              if (i + 1 < memory + 1)
                return (RETURN_TYPE) (haystack + j);
               
              j += period;
              memory = needle_len - period;
            }
          else
            {
              j += i - suffix + 1;
              memory = 0;
            }
        }
    }
  else
    {
       
      size_t shift;
      period = MAX (suffix, needle_len - suffix) + 1;
      j = 0;
      while (AVAILABLE (haystack, haystack_len, j, needle_len))
        {
           
          shift = shift_table[CANON_ELEMENT (haystack[j + needle_len - 1])];
          if (0 < shift)
            {
              j += shift;
              continue;
            }
           
          i = suffix;
          while (i < needle_len - 1 && (CANON_ELEMENT (needle[i])
                                        == CANON_ELEMENT (haystack[i + j])))
            ++i;
          if (needle_len - 1 <= i)
            {
               
              i = suffix - 1;
              while (i != SIZE_MAX && (CANON_ELEMENT (needle[i])
                                       == CANON_ELEMENT (haystack[i + j])))
                --i;
              if (i == SIZE_MAX)
                return (RETURN_TYPE) (haystack + j);
              j += period;
            }
          else
            j += i - suffix + 1;
        }
    }
  return NULL;
}

#undef AVAILABLE
#undef CANON_ELEMENT
#undef CMP_FUNC
#undef MAX
#undef RETURN_TYPE
