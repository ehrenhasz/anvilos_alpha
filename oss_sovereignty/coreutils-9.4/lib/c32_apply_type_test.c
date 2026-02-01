 

#include <config.h>

#define IN_C32_APPLY_TYPE_TEST
 
#include <uchar.h>

#include <string.h>
#include <wctype.h>

#if _GL_WCHAR_T_IS_UCS4
_GL_EXTERN_INLINE
#endif
int
c32_apply_type_test (wint_t wc, c32_type_test_t property)
{
#if _GL_WCHAR_T_IS_UCS4
  return iswctype (wc, property);
#else
  return property (wc);
#endif
}
