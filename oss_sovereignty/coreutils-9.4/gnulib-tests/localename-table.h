 

#if HAVE_WORKING_USELOCALE && HAVE_NAMELESS_LOCALES

# include <stddef.h>
# include <locale.h>

# include "glthread/lock.h"

struct locale_categories_names
  {
     
    const char *category_name[6];
  };

 
# define locale_hash_function libintl_locale_hash_function
# define locale_hash_table libintl_locale_hash_table
# define locale_lock libintl_locale_lock

extern size_t _GL_ATTRIBUTE_CONST locale_hash_function (locale_t x);

 
struct locale_hash_node
  {
    struct locale_hash_node *next;
    locale_t locale;
    struct locale_categories_names names;
  };

# define LOCALE_HASH_TABLE_SIZE 101
extern struct locale_hash_node * locale_hash_table[LOCALE_HASH_TABLE_SIZE];

 

gl_rwlock_define(extern, locale_lock)

#endif
