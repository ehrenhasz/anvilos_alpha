 

#include <config.h>
#include "argv-iter.h"

#include <stdlib.h>
#include <string.h>

struct argv_iterator
{
   
   
  FILE *fp;
  size_t item_idx;
  char *tok;
  size_t buf_len;

   
  char **arg_list;
  char **p;
};

struct argv_iterator *
argv_iter_init_argv (char **argv)
{
  struct argv_iterator *ai = malloc (sizeof *ai);
  if (!ai)
    return NULL;
  ai->fp = NULL;
  ai->arg_list = argv;
  ai->p = argv;
  return ai;
}

 
struct argv_iterator *
argv_iter_init_stream (FILE *fp)
{
  struct argv_iterator *ai = malloc (sizeof *ai);
  if (!ai)
    return NULL;
  ai->fp = fp;
  ai->tok = NULL;
  ai->buf_len = 0;

  ai->item_idx = 0;
  ai->arg_list = NULL;
  return ai;
}

char *
argv_iter (struct argv_iterator *ai, enum argv_iter_err *err)
{
  if (ai->fp)
    {
      ssize_t len = getdelim (&ai->tok, &ai->buf_len, '\0', ai->fp);
      if (len < 0)
        {
          *err = feof (ai->fp) ? AI_ERR_EOF : AI_ERR_READ;
          return NULL;
        }

      *err = AI_ERR_OK;
      ai->item_idx++;
      return ai->tok;
    }
  else
    {
      if (*(ai->p) == NULL)
        {
          *err = AI_ERR_EOF;
          return NULL;
        }
      else
        {
          *err = AI_ERR_OK;
          return *(ai->p++);
        }
    }
}

size_t
argv_iter_n_args (struct argv_iterator const *ai)
{
  return ai->fp ? ai->item_idx : ai->p - ai->arg_list;
}

void
argv_iter_free (struct argv_iterator *ai)
{
  if (ai->fp)
    free (ai->tok);
  free (ai);
}
