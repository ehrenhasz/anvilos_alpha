 

#include <config.h>

#if HAVE_WORKING_USELOCALE && HAVE_NAMELESS_LOCALES

 
#include "localename-table.h"

#include <stdint.h>

 
size_t _GL_ATTRIBUTE_CONST
locale_hash_function (locale_t x)
{
  uintptr_t p = (uintptr_t) x;
  size_t h = ((p % 4177) << 12) + ((p % 79) << 6) + (p % 61);
  return h;
}

struct locale_hash_node * locale_hash_table[LOCALE_HASH_TABLE_SIZE]
   ;

gl_rwlock_define_initialized(, locale_lock)

#else

 
typedef int dummy;

#endif
