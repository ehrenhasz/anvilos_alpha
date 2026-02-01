 

#ifndef READTOKENS_H
# define READTOKENS_H

# include <stdio.h>

 

struct tokenbuffer
{
  size_t size;
  char *buffer;
};
typedef struct tokenbuffer token_buffer;

void init_tokenbuffer (token_buffer *tokenbuffer);

size_t
  readtoken (FILE *stream, const char *delim, size_t n_delim,
             token_buffer *tokenbuffer);
size_t
  readtokens (FILE *stream, size_t projected_n_tokens,
              const char *delim, size_t n_delim,
              char ***tokens_out, size_t **token_lengths);

#endif  
