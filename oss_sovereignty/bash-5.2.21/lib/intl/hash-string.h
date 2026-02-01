 

 

 

#ifndef PARAMS
# if __STDC__ || defined __GNUC__ || defined __SUNPRO_C || defined __cplusplus || __PROTOTYPES
#  define PARAMS(Args) Args
# else
#  define PARAMS(Args) ()
# endif
#endif

 
#define HASHWORDBITS 32


 
static unsigned long int hash_string PARAMS ((const char *__str_param));

static inline unsigned long int
hash_string (str_param)
     const char *str_param;
{
  unsigned long int hval, g;
  const char *str = str_param;

   
  hval = 0;
  while (*str != '\0')
    {
      hval <<= 4;
      hval += (unsigned long int) *str++;
      g = hval & ((unsigned long int) 0xf << (HASHWORDBITS - 4));
      if (g != 0)
	{
	  hval ^= g >> (HASHWORDBITS - 8);
	  hval ^= g;
	}
    }
  return hval;
}
