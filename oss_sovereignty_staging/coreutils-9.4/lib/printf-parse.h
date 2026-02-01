 

#if HAVE_FEATURES_H
# include <features.h>  
#endif

#include "printf-args.h"


 
#define FLAG_GROUP       1       
#define FLAG_LEFT        2       
#define FLAG_SHOWSIGN    4       
#define FLAG_SPACE       8       
#define FLAG_ALT        16       
#define FLAG_ZERO       32
#if __GLIBC__ >= 2 && !defined __UCLIBC__
# define FLAG_LOCALIZED 64       
#endif

 
#define ARG_NONE        (~(size_t)0)

 

 
#define N_DIRECT_ALLOC_DIRECTIVES 7

 
typedef struct
{
  const char* dir_start;
  const char* dir_end;
  int flags;
  const char* width_start;
  const char* width_end;
  size_t width_arg_index;
  const char* precision_start;
  const char* precision_end;
  size_t precision_arg_index;
  char conversion;  
  size_t arg_index;
}
char_directive;

 
typedef struct
{
  size_t count;
  char_directive *dir;
  size_t max_width_length;
  size_t max_precision_length;
  char_directive direct_alloc_dir[N_DIRECT_ALLOC_DIRECTIVES];
}
char_directives;

#if ENABLE_UNISTDIO

 
typedef struct
{
  const uint8_t* dir_start;
  const uint8_t* dir_end;
  int flags;
  const uint8_t* width_start;
  const uint8_t* width_end;
  size_t width_arg_index;
  const uint8_t* precision_start;
  const uint8_t* precision_end;
  size_t precision_arg_index;
  uint8_t conversion;  
  size_t arg_index;
}
u8_directive;

 
typedef struct
{
  size_t count;
  u8_directive *dir;
  size_t max_width_length;
  size_t max_precision_length;
  u8_directive direct_alloc_dir[N_DIRECT_ALLOC_DIRECTIVES];
}
u8_directives;

 
typedef struct
{
  const uint16_t* dir_start;
  const uint16_t* dir_end;
  int flags;
  const uint16_t* width_start;
  const uint16_t* width_end;
  size_t width_arg_index;
  const uint16_t* precision_start;
  const uint16_t* precision_end;
  size_t precision_arg_index;
  uint16_t conversion;  
  size_t arg_index;
}
u16_directive;

 
typedef struct
{
  size_t count;
  u16_directive *dir;
  size_t max_width_length;
  size_t max_precision_length;
  u16_directive direct_alloc_dir[N_DIRECT_ALLOC_DIRECTIVES];
}
u16_directives;

 
typedef struct
{
  const uint32_t* dir_start;
  const uint32_t* dir_end;
  int flags;
  const uint32_t* width_start;
  const uint32_t* width_end;
  size_t width_arg_index;
  const uint32_t* precision_start;
  const uint32_t* precision_end;
  size_t precision_arg_index;
  uint32_t conversion;  
  size_t arg_index;
}
u32_directive;

 
typedef struct
{
  size_t count;
  u32_directive *dir;
  size_t max_width_length;
  size_t max_precision_length;
  u32_directive direct_alloc_dir[N_DIRECT_ALLOC_DIRECTIVES];
}
u32_directives;

#endif


 
#if ENABLE_UNISTDIO
extern int
       ulc_printf_parse (const char *format, char_directives *d, arguments *a);
extern int
       u8_printf_parse (const uint8_t *format, u8_directives *d, arguments *a);
extern int
       u16_printf_parse (const uint16_t *format, u16_directives *d,
                         arguments *a);
extern int
       u32_printf_parse (const uint32_t *format, u32_directives *d,
                         arguments *a);
#else
# ifdef STATIC
STATIC
# else
extern
# endif
int printf_parse (const char *format, char_directives *d, arguments *a);
#endif

#endif  
