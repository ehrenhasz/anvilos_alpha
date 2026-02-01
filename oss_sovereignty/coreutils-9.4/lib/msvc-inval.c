 
#include "msvc-inval.h"

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER \
    && !(MSVC_INVALID_PARAMETER_HANDLING == SANE_LIBRARY_HANDLING)

 
# include <stdlib.h>

# if MSVC_INVALID_PARAMETER_HANDLING == DEFAULT_HANDLING

static void __cdecl
gl_msvc_invalid_parameter_handler (const wchar_t *expression,
                                   const wchar_t *function,
                                   const wchar_t *file,
                                   unsigned int line,
                                   uintptr_t dummy)
{
}

# else

 
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

#  if defined _MSC_VER

static void __cdecl
gl_msvc_invalid_parameter_handler (const wchar_t *expression,
                                   const wchar_t *function,
                                   const wchar_t *file,
                                   unsigned int line,
                                   uintptr_t dummy)
{
  RaiseException (STATUS_GNULIB_INVALID_PARAMETER, 0, 0, NULL);
}

#  else

 
static DWORD tls_index;
static int tls_initialized  ;

 
static struct gl_msvc_inval_per_thread not_per_thread;

struct gl_msvc_inval_per_thread *
gl_msvc_inval_current (void)
{
  if (!tls_initialized)
    {
      tls_index = TlsAlloc ();
      tls_initialized = 1;
    }
  if (tls_index == TLS_OUT_OF_INDEXES)
     
    return &not_per_thread;
  else
    {
      struct gl_msvc_inval_per_thread *pointer =
        (struct gl_msvc_inval_per_thread *) TlsGetValue (tls_index);
      if (pointer == NULL)
        {
           
          pointer =
            (struct gl_msvc_inval_per_thread *)
            malloc (sizeof (struct gl_msvc_inval_per_thread));
          if (pointer == NULL)
             
            pointer = &not_per_thread;
          TlsSetValue (tls_index, pointer);
        }
      return pointer;
    }
}

static void __cdecl
gl_msvc_invalid_parameter_handler (const wchar_t *expression,
                                   const wchar_t *function,
                                   const wchar_t *file,
                                   unsigned int line,
                                   uintptr_t dummy)
{
  struct gl_msvc_inval_per_thread *current = gl_msvc_inval_current ();
  if (current->restart_valid)
    longjmp (current->restart, 1);
  else
     
    RaiseException (STATUS_GNULIB_INVALID_PARAMETER, 0, 0, NULL);
}

#  endif

# endif

static int gl_msvc_inval_initialized  ;

void
gl_msvc_inval_ensure_handler (void)
{
  if (gl_msvc_inval_initialized == 0)
    {
      _set_invalid_parameter_handler (gl_msvc_invalid_parameter_handler);
      gl_msvc_inval_initialized = 1;
    }
}

#endif
