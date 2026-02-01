 

#ifndef QUOTEARG_H_
# define QUOTEARG_H_ 1

 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdlib.h>

 
enum quoting_style
  {
     
    literal_quoting_style,

     
    shell_quoting_style,

     
    shell_always_quoting_style,

     
    shell_escape_quoting_style,

     
    shell_escape_always_quoting_style,

     
    c_quoting_style,

     
    c_maybe_quoting_style,

     
    escape_quoting_style,

     
    locale_quoting_style,

     
    clocale_quoting_style,

     
    custom_quoting_style
  };

 
enum quoting_flags
  {
     
    QA_ELIDE_NULL_BYTES = 0x01,

     
    QA_ELIDE_OUTER_QUOTES = 0x02,

     
    QA_SPLIT_TRIGRAPHS = 0x04
  };

 
# ifndef DEFAULT_QUOTING_STYLE
#  define DEFAULT_QUOTING_STYLE literal_quoting_style
# endif

 
extern char const *const quoting_style_args[];
extern enum quoting_style const quoting_style_vals[];

struct quoting_options;

 

 
struct quoting_options *clone_quoting_options (struct quoting_options *o)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

 
enum quoting_style get_quoting_style (struct quoting_options const *o);

 
void set_quoting_style (struct quoting_options *o, enum quoting_style s);

 
int set_char_quoting (struct quoting_options *o, char c, int i);

 
int set_quoting_flags (struct quoting_options *o, int i);

 
void set_custom_quoting (struct quoting_options *o,
                         char const *left_quote,
                         char const *right_quote);

 
size_t quotearg_buffer (char *restrict buffer, size_t buffersize,
                        char const *arg, size_t argsize,
                        struct quoting_options const *o);

 
char *quotearg_alloc (char const *arg, size_t argsize,
                      struct quoting_options const *o)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

 
char *quotearg_alloc_mem (char const *arg, size_t argsize,
                          size_t *size, struct quoting_options const *o)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

 
char *quotearg_n (int n, char const *arg);

 
char *quotearg (char const *arg);

 
char *quotearg_n_mem (int n, char const *arg, size_t argsize);

 
char *quotearg_mem (char const *arg, size_t argsize);

 
char *quotearg_n_style (int n, enum quoting_style s, char const *arg);

 
char *quotearg_n_style_mem (int n, enum quoting_style s,
                            char const *arg, size_t argsize);

 
char *quotearg_style (enum quoting_style s, char const *arg);

 
char *quotearg_style_mem (enum quoting_style s,
                          char const *arg, size_t argsize);

 
char *quotearg_char (char const *arg, char ch);

 
char *quotearg_char_mem (char const *arg, size_t argsize, char ch);

 
char *quotearg_colon (char const *arg);

 
char *quotearg_colon_mem (char const *arg, size_t argsize);

 
char *quotearg_n_style_colon (int n, enum quoting_style s, char const *arg);

 
char *quotearg_n_custom (int n, char const *left_quote,
                         char const *right_quote, char const *arg);

 
char *quotearg_n_custom_mem (int n, char const *left_quote,
                             char const *right_quote,
                             char const *arg, size_t argsize);

 
char *quotearg_custom (char const *left_quote, char const *right_quote,
                       char const *arg);

 
char *quotearg_custom_mem (char const *left_quote,
                           char const *right_quote,
                           char const *arg, size_t argsize);

 
void quotearg_free (void);

#endif  
