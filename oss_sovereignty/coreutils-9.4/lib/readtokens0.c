 

#include <config.h>

#include <stdlib.h>

#include "readtokens0.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

void
readtokens0_init (struct Tokens *t)
{
  t->n_tok = 0;
  t->tok = NULL;
  t->tok_len = NULL;
  obstack_init (&t->o_data);
  obstack_init (&t->o_tok);
  obstack_init (&t->o_tok_len);
}

void
readtokens0_free (struct Tokens *t)
{
  obstack_free (&t->o_data, NULL);
  obstack_free (&t->o_tok, NULL);
  obstack_free (&t->o_tok_len, NULL);
}

 
static void
save_token (struct Tokens *t)
{
   
  size_t len = obstack_object_size (&t->o_data) - 1;
  char const *s = obstack_finish (&t->o_data);
  obstack_ptr_grow (&t->o_tok, s);
  obstack_grow (&t->o_tok_len, &len, sizeof len);
  t->n_tok++;
}

 
bool
readtokens0 (FILE *in, struct Tokens *t)
{

  while (1)
    {
      int c = fgetc (in);
      if (c == EOF)
        {
          size_t len = obstack_object_size (&t->o_data);
           
          if (len)
            {
              obstack_1grow (&t->o_data, '\0');
              save_token (t);
            }

          break;
        }

      obstack_1grow (&t->o_data, c);
      if (c == '\0')
        save_token (t);
    }

   
  obstack_ptr_grow (&t->o_tok, NULL);

  t->tok = obstack_finish (&t->o_tok);
  t->tok_len = obstack_finish (&t->o_tok_len);
  return ! ferror (in);
}
