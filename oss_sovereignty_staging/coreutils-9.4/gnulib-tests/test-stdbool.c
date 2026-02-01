 

 
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) \
    || (__clang_major__ >= 4)
 
# pragma GCC diagnostic ignored "-Waddress"
# define ADDRESS_CHECK_OKAY
#elif defined __GNUC__ || defined __clang__
 
#else
 
# define ADDRESS_CHECK_OKAY
#endif

#include <config.h>

#ifdef TEST_STDBOOL_H
# include <stdbool.h>
#endif

#if false
 "error: false is not 0"
#endif
#if true != 1
 "error: true is not 1"
#endif

 
#if ((HAVE_C_BOOL || defined __cplusplus \
      || HAVE_STDBOOL_H || 3 <= __GNUC__ || 4 <= __clang_major__) \
     && !(defined _MSC_VER || defined __SUNPRO_C))
# define WORKING_BOOL 1
#else
# define WORKING_BOOL 0
#endif

#if WORKING_BOOL
struct s { bool s: 1; bool t; } s;
#endif

char a[true == 1 ? 1 : -1];
char b[false == 0 ? 1 : -1];
#if WORKING_BOOL
char d[(bool) 0.5 == true ? 1 : -1];
# ifdef ADDRESS_CHECK_OKAY  
 
#  if defined __GNUC__ || defined __clang__
bool e = &s;
#  endif
# endif
char f[(bool) 0.0 == false ? 1 : -1];
#endif
char g[true];
char h[sizeof (bool)];
#if WORKING_BOOL
char i[sizeof s.t];
#endif
enum { j = false, k = true, l = false * true, m = true * 256 };
bool n[m];
char o[sizeof n == m * sizeof n[0] ? 1 : -1];
char p[-1 - (bool) 0 < 0 && -1 - (bool) 0 < 0 ? 1 : -1];
 
bool q = true;
bool *pq = &q;

int
main ()
{
  int error = 0;

#if WORKING_BOOL
# ifdef ADDRESS_CHECK_OKAY  
   
  {
    bool e1 = &s;
    if (!e1)
      error = 1;
  }
# endif
#endif

   
  {
    char digs[] = "0123456789";
    if (&(digs + 5)[-2 + (bool) 1] != &digs[4])
      error = 1;
  }

  return error;
}
