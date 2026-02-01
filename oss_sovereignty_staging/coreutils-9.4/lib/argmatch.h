 

#ifndef ARGMATCH_H_
# define ARGMATCH_H_ 1

 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <limits.h>
# include <stddef.h>
# include <stdio.h>
# include <string.h>  

# include "gettext.h"
# include "quote.h"

# ifdef  __cplusplus
extern "C" {
# endif

# define ARRAY_CARDINALITY(Array) (sizeof (Array) / sizeof *(Array))

 

# define ARGMATCH_VERIFY(Arglist, Vallist) \
    static_assert (ARRAY_CARDINALITY (Arglist) \
                   == ARRAY_CARDINALITY (Vallist) + 1)

 

ptrdiff_t argmatch (char const *arg, char const *const *arglist,
                    void const *vallist, size_t valsize) _GL_ATTRIBUTE_PURE;

ptrdiff_t argmatch_exact (char const *arg, char const *const *arglist)
  _GL_ATTRIBUTE_PURE;

# define ARGMATCH(Arg, Arglist, Vallist) \
  argmatch (Arg, Arglist, (void const *) (Vallist), sizeof *(Vallist))

# define ARGMATCH_EXACT(Arg, Arglist) \
  argmatch_exact (Arg, Arglist)

 
typedef void (*argmatch_exit_fn) (void);
extern argmatch_exit_fn argmatch_die;

 

void argmatch_invalid (char const *context, char const *value,
                       ptrdiff_t problem);

 

# define invalid_arg(Context, Value, Problem) \
  argmatch_invalid (Context, Value, Problem)



 

void argmatch_valid (char const *const *arglist,
                     void const *vallist, size_t valsize);

# define ARGMATCH_VALID(Arglist, Vallist) \
  argmatch_valid (Arglist, (void const *) (Vallist), sizeof *(Vallist))



 

ptrdiff_t __xargmatch_internal (char const *context,
                                char const *arg, char const *const *arglist,
                                void const *vallist, size_t valsize,
                                argmatch_exit_fn exit_fn,
                                bool allow_abbreviation);

 

# define XARGMATCH(Context, Arg, Arglist, Vallist)              \
  ((Vallist) [__xargmatch_internal (Context, Arg, Arglist,      \
                                    (void const *) (Vallist),   \
                                    sizeof *(Vallist),          \
                                    argmatch_die,               \
                                    true)])

# define XARGMATCH_EXACT(Context, Arg, Arglist, Vallist)        \
  ((Vallist) [__xargmatch_internal (Context, Arg, Arglist,      \
                                    (void const *) (Vallist),   \
                                    sizeof *(Vallist),          \
                                    argmatch_die,               \
                                    false)])

 

char const *argmatch_to_argument (void const *value,
                                  char const *const *arglist,
                                  void const *vallist, size_t valsize)
  _GL_ATTRIBUTE_PURE;

# define ARGMATCH_TO_ARGUMENT(Value, Arglist, Vallist)                  \
  argmatch_to_argument (Value, Arglist,                                 \
                        (void const *) (Vallist), sizeof *(Vallist))

# define ARGMATCH_DEFINE_GROUP(Name, Type)                              \
                             \
  typedef Type argmatch_##Name##_type;                                  \
                                                                        \
                  \
  enum argmatch_##Name##_size_enum                                      \
  {                                                                     \
    argmatch_##Name##_size = sizeof (argmatch_##Name##_type)            \
  };                                                                    \
                                                                        \
                                   \
  typedef struct                                                        \
  {                                                                     \
                                        \
    const char *arg;                                                    \
                                     \
    const argmatch_##Name##_type val;                                   \
  } argmatch_##Name##_arg;                                              \
                                                                        \
                                      \
  typedef struct                                                        \
  {                                                                     \
                                        \
    const char *arg;                                                    \
           \
    const char *doc;                                                    \
  } argmatch_##Name##_doc;                                              \
                                                                        \
                            \
  typedef struct                                                        \
  {                                                                     \
    const argmatch_##Name##_arg* args;                                  \
    const argmatch_##Name##_doc* docs;                                  \
                                                                        \
                                 \
    const char *doc_pre;                                                \
                                  \
    const char *doc_post;                                               \
  } argmatch_##Name##_group_type;                                       \
                                                                        \
                                \
  extern const argmatch_##Name##_group_type argmatch_##Name##_group;    \
                                                                        \
                            \
  void argmatch_##Name##_usage (FILE *out);                             \
                                                                        \
         \
  ptrdiff_t argmatch_##Name##_choice (const char *arg);                 \
                                                                        \
                                                    \
  const argmatch_##Name##_type*                                         \
  argmatch_##Name##_value (const char *context, const char *arg);       \
                                                                        \
      \
  const char *                                                          \
  argmatch_##Name##_argument (const argmatch_##Name##_type *val);       \
                                                                        \
  ptrdiff_t                                                             \
  argmatch_##Name##_choice (const char *arg)                            \
  {                                                                     \
    const argmatch_##Name##_group_type *g = &argmatch_##Name##_group;   \
    size_t size = argmatch_##Name##_size;                               \
    ptrdiff_t res = -1;             \
    bool ambiguous = false;    \
    size_t arglen = strlen (arg);                                       \
                                                                        \
                                                          \
    for (size_t i = 0; g->args[i].arg; i++)                             \
      if (!strncmp (g->args[i].arg, arg, arglen))                       \
        {                                                               \
          if (strlen (g->args[i].arg) == arglen)                        \
                                                \
            return i;                                                   \
          else if (res == -1)                                           \
                                       \
            res = i;                                                    \
          else if (memcmp (&g->args[res].val, &g->args[i].val, size))   \
                                      \
                                                      \
            ambiguous = true;                                           \
        }                                                               \
    return ambiguous ? -2 : res;                                        \
  }                                                                     \
                                                                        \
  const char *                                                          \
  argmatch_##Name##_argument (const argmatch_##Name##_type *val)        \
  {                                                                     \
    const argmatch_##Name##_group_type *g = &argmatch_##Name##_group;   \
    size_t size = argmatch_##Name##_size;                               \
    for (size_t i = 0; g->args[i].arg; i++)                             \
      if (!memcmp (val, &g->args[i].val, size))                         \
        return g->args[i].arg;                                          \
    return NULL;                                                        \
  }                                                                     \
                                                                        \
                               \
  static void                                                           \
  argmatch_##Name##_valid (FILE *out)                                   \
  {                                                                     \
    const argmatch_##Name##_group_type *g = &argmatch_##Name##_group;   \
    size_t size = argmatch_##Name##_size;                               \
                                                                        \
                                              \
    fputs (gettext ("Valid arguments are:"), out);                      \
    for (int i = 0; g->args[i].arg; i++)                                \
      if (i == 0                                                        \
          || memcmp (&g->args[i-1].val, &g->args[i].val, size))         \
        fprintf (out, "\n  - %s", quote (g->args[i].arg));              \
      else                                                              \
        fprintf (out, ", %s", quote (g->args[i].arg));                  \
    putc ('\n', out);                                                   \
  }                                                                     \
                                                                        \
  const argmatch_##Name##_type*                                         \
  argmatch_##Name##_value (const char *context, const char *arg)        \
  {                                                                     \
    const argmatch_##Name##_group_type *g = &argmatch_##Name##_group;   \
    ptrdiff_t res = argmatch_##Name##_choice (arg);                     \
    if (res < 0)                                                        \
      {                                                                 \
        argmatch_invalid (context, arg, res);                           \
        argmatch_##Name##_valid (stderr);                               \
        argmatch_die ();                                                \
      }                                                                 \
    return &g->args[res].val;                                           \
  }                                                                     \
                                                                        \
                        \
  static int                                                            \
  argmatch_##Name##_doc_col (void)                                      \
  {                                                                     \
    const argmatch_##Name##_group_type *g = &argmatch_##Name##_group;   \
    size_t size = argmatch_##Name##_size;                               \
    int res = 0;                                                        \
    for (int i = 0; g->docs[i].arg; ++i)                                \
      {                                                                 \
        int col = 4;                                                    \
        int ival = argmatch_##Name##_choice (g->docs[i].arg);           \
        if (ival < 0)                                                   \
                                       \
          col += strlen (g->docs[i].arg);                               \
        else                                                            \
                    \
          for (int j = 0; g->args[j].arg; ++j)                          \
            if (! memcmp (&g->args[ival].val, &g->args[j].val, size))   \
              col += (col == 4 ? 0 : 2) + strlen (g->args[j].arg);      \
        if (res <= col)                                                 \
          res = col <= 20 ? col : 20;                                   \
      }                                                                 \
    return res ? res : 20;                                              \
  }                                                                     \
                                                                        \
  void                                                                  \
  argmatch_##Name##_usage (FILE *out)                                   \
  {                                                                     \
    const argmatch_##Name##_group_type *g = &argmatch_##Name##_group;   \
    size_t size = argmatch_##Name##_size;                               \
                                                       \
    const int screen_width = getenv ("HELP2MAN") ? INT_MAX : 80;        \
    if (g->doc_pre)                                                     \
      fprintf (out, "%s\n", gettext (g->doc_pre));                      \
    int doc_col = argmatch_##Name##_doc_col ();                         \
    for (int i = 0; g->docs[i].arg; ++i)                                \
      {                                                                 \
        int col = 0;                                                    \
        bool first = true;                                              \
        int ival = argmatch_##Name##_choice (g->docs[i].arg);           \
        if (ival < 0)                                                   \
                                       \
          col += fprintf (out,  "  %s", g->docs[i].arg);                \
        else                                                            \
                    \
          for (int j = 0; g->args[j].arg; ++j)                          \
            if (! memcmp (&g->args[ival].val, &g->args[j].val, size))   \
              {                                                         \
                if (!first                                              \
                    && screen_width < col + 2 + strlen (g->args[j].arg)) \
                  {                                                     \
                    fprintf (out, ",\n");                               \
                    col = 0;                                            \
                    first = true;                                       \
                  }                                                     \
                if (first)                                              \
                  {                                                     \
                    col += fprintf (out, " ");                          \
                    first = false;                                      \
                  }                                                     \
                else                                                    \
                  col += fprintf (out, ",");                            \
                col += fprintf (out,  " %s", g->args[j].arg);           \
              }                                                         \
                        \
        if (doc_col < col + 2)                                          \
          {                                                             \
            fprintf (out, "\n");                                        \
            col = 0;                                                    \
          }                                                             \
        fprintf (out, "%*s%s\n",                                        \
                 doc_col - col, "", gettext (g->docs[i].doc));          \
      }                                                                 \
    if (g->doc_post)                                                    \
      fprintf (out, "%s\n", gettext (g->doc_post));                     \
  }

# ifdef  __cplusplus
}
# endif

#endif  
