 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#define DEFAULT_HANDLING       0
#define HAIRY_LIBRARY_HANDLING 1
#define SANE_LIBRARY_HANDLING  2

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER \
    && !(MSVC_INVALID_PARAMETER_HANDLING == SANE_LIBRARY_HANDLING)
 

# if MSVC_INVALID_PARAMETER_HANDLING == DEFAULT_HANDLING
 

#  ifdef __cplusplus
extern "C" {
#  endif

 
extern void gl_msvc_inval_ensure_handler (void);

#  ifdef __cplusplus
}
#  endif

#  define TRY_MSVC_INVAL \
     do                                                                        \
       {                                                                       \
         gl_msvc_inval_ensure_handler ();                                      \
         if (1)
#  define CATCH_MSVC_INVAL \
         else
#  define DONE_MSVC_INVAL \
       }                                                                       \
     while (0)

# else
 

#  include <excpt.h>

 
#  define STATUS_GNULIB_INVALID_PARAMETER (0xE0000000 + 0x474E550 + 0)

#  if defined _MSC_VER
 

#   ifdef __cplusplus
extern "C" {
#   endif

 
extern void gl_msvc_inval_ensure_handler (void);

#   ifdef __cplusplus
}
#   endif

#   define TRY_MSVC_INVAL \
      do                                                                       \
        {                                                                      \
          gl_msvc_inval_ensure_handler ();                                     \
          __try
#   define CATCH_MSVC_INVAL \
          __except (GetExceptionCode () == STATUS_GNULIB_INVALID_PARAMETER     \
                    ? EXCEPTION_EXECUTE_HANDLER                                \
                    : EXCEPTION_CONTINUE_SEARCH)
#   define DONE_MSVC_INVAL \
        }                                                                      \
      while (0)

#  else
 

#   include <setjmp.h>

#   ifdef __cplusplus
extern "C" {
#   endif

struct gl_msvc_inval_per_thread
{
   
  jmp_buf restart;

   
  int restart_valid;
};

 
extern void gl_msvc_inval_ensure_handler (void);

 
extern struct gl_msvc_inval_per_thread *gl_msvc_inval_current (void);

#   ifdef __cplusplus
}
#   endif

#   define TRY_MSVC_INVAL \
      do                                                                       \
        {                                                                      \
          struct gl_msvc_inval_per_thread *msvc_inval_current;                 \
          gl_msvc_inval_ensure_handler ();                                     \
          msvc_inval_current = gl_msvc_inval_current ();                       \
                                 \
          if (setjmp (msvc_inval_current->restart) == 0)                       \
            {                                                                  \
                                                  \
              msvc_inval_current->restart_valid = 1;
#   define CATCH_MSVC_INVAL \
                                   \
              msvc_inval_current->restart_valid = 0;                           \
            }                                                                  \
          else                                                                 \
            {                                                                  \
                                   \
              msvc_inval_current->restart_valid = 0;
#   define DONE_MSVC_INVAL \
            }                                                                  \
        }                                                                      \
      while (0)

#  endif

# endif

#else
 

 
# define TRY_MSVC_INVAL \
    do                                                                         \
      {                                                                        \
        if (1)
# define CATCH_MSVC_INVAL \
        else
# define DONE_MSVC_INVAL \
      }                                                                        \
    while (0)

#endif

#endif  
