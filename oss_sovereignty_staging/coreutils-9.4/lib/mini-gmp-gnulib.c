 
#if 4 < __GNUC__ + (6 <= __GNUC_MINOR__)
# pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
# pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif
#if 8 <= __GNUC__
# pragma GCC diagnostic ignored "-Wsuggest-attribute=malloc"
#endif

 
#if (defined NDEBUG \
     && (4 < __GNUC__ + (6 <= __GNUC_MINOR__) || defined __clang__))
# pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "mini-gmp.c"
