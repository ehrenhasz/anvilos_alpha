 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdio.h>

 

 

 
#define EXCLUDE_ANCHORED (1 << 30)

 
#define EXCLUDE_INCLUDE (1 << 29)

 
#define EXCLUDE_WILDCARDS (1 << 28)

 
#define EXCLUDE_REGEX     (1 << 27)

 
#define EXCLUDE_ALLOC     (1 << 26)

struct exclude;

bool fnmatch_pattern_has_wildcards (const char *, int) _GL_ATTRIBUTE_PURE;

void free_exclude (struct exclude *)
  _GL_ATTRIBUTE_NONNULL ((1));
struct exclude *new_exclude (void)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_RETURNS_NONNULL
  _GL_ATTRIBUTE_DEALLOC (free_exclude, 1);
void add_exclude (struct exclude *, char const *, int);
int add_exclude_file (void (*) (struct exclude *, char const *, int),
                      struct exclude *, char const *, int, char);
int add_exclude_fp (void (*) (struct exclude *, char const *, int, void *),
                    struct exclude *, FILE *, int, char, void *);
bool excluded_file_name (struct exclude const *, char const *);
void exclude_add_pattern_buffer (struct exclude *ex, char *buf);
bool exclude_fnmatch (char const *, char const *, int);

#endif  
