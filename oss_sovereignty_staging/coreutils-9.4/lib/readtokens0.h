 

#ifndef READTOKENS0_H
# define READTOKENS0_H 1

# include <stdio.h>
# include <sys/types.h>
# include "obstack.h"

struct Tokens
{
  size_t n_tok;
  char **tok;
  size_t *tok_len;
  struct obstack o_data;  
  struct obstack o_tok;  
  struct obstack o_tok_len;  
};

void readtokens0_init (struct Tokens *t);
void readtokens0_free (struct Tokens *t);
bool readtokens0 (FILE *in, struct Tokens *t);

#endif
