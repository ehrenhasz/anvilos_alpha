#ifndef DEBUG_H_12987983217
#define DEBUG_H_12987983217
#if defined (__cplusplus)
extern "C" {
#endif
#define DEBUG_STATIC_ASSERT(c) (void)sizeof(char[(c) ? 1 : -1])
#ifndef DEBUGLEVEL
#  define DEBUGLEVEL 0
#endif
#ifndef DEBUGFILE
#  define DEBUGFILE stderr
#endif
#if (DEBUGLEVEL>=1)
#  include <assert.h>
#else
#  ifndef assert    
#    define assert(condition) ((void)0)    
#  endif
#endif
#if (DEBUGLEVEL>=2)
#  include <stdio.h>
extern int g_debuglevel;  
#  define RAWLOG(l, ...) {                                      \
                if (l<=g_debuglevel) {                          \
                    fprintf(stderr, __VA_ARGS__);               \
            }   }
#  define DEBUGLOG(l, ...) {                                    \
                if (l<=g_debuglevel) {                          \
                    fprintf(stderr, __FILE__ ": " __VA_ARGS__); \
                    fprintf(stderr, " \n");                     \
            }   }
#else
#  define RAWLOG(l, ...)      {}     
#  define DEBUGLOG(l, ...)    {}     
#endif
#if defined (__cplusplus)
}
#endif
#endif  
