 

 

 

#ifndef _COLORS_H_
#define _COLORS_H_

#include <stdio.h>  

#if defined(__TANDEM) && defined(HAVE_STDBOOL_H) && (__STDC_VERSION__ < 199901L)
typedef int _Bool;
#endif

#if defined (HAVE_STDBOOL_H)
#  include <stdbool.h>  
#else
typedef int _rl_bool_t;

#ifdef bool
#  undef bool
#endif
#define bool _rl_bool_t

#ifndef true
#  define true 1
#  define false 0
#endif

#endif  

 
struct bin_str
  {
    size_t len;
    const char *string;
  };

 
extern struct bin_str _rl_color_indicator[];

 
typedef struct _color_ext_type
  {
    struct bin_str ext;         	 
    struct bin_str seq;         	 
    struct _color_ext_type *next;	 
  } COLOR_EXT_TYPE;

 
extern COLOR_EXT_TYPE *_rl_color_ext_list;

#define FILETYPE_INDICATORS				\
  {							\
    C_ORPHAN, C_FIFO, C_CHR, C_DIR, C_BLK, C_FILE,	\
    C_LINK, C_SOCK, C_FILE, C_DIR			\
  }

 

enum indicator_no
  {
    C_LEFT, C_RIGHT, C_END, C_RESET, C_NORM, C_FILE, C_DIR, C_LINK,
    C_FIFO, C_SOCK,
    C_BLK, C_CHR, C_MISSING, C_ORPHAN, C_EXEC, C_DOOR, C_SETUID, C_SETGID,
    C_STICKY, C_OTHER_WRITABLE, C_STICKY_OTHER_WRITABLE, C_CAP, C_MULTIHARDLINK,
    C_CLR_TO_EOL
  };


#if !S_IXUGO
# define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

enum filetype
  {
    unknown,
    fifo,
    chardev,
    directory,
    blockdev,
    normal,
    symbolic_link,
    sock,
    whiteout,
    arg_directory
  };

 
#define C_PREFIX	C_SOCK

extern void _rl_put_indicator (const struct bin_str *ind);
extern void _rl_set_normal_color (void);
extern bool _rl_print_prefix_color (void);
extern bool _rl_print_color_indicator (const char *f);
extern void _rl_prep_non_filename_text (void);

#endif  
