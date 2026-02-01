 

#ifndef _GL_RAND_ISAAC_H
#define _GL_RAND_ISAAC_H

#include <stddef.h>
#include <stdint.h>

 
#ifndef ISAAC_BITS_LOG
 #if SIZE_MAX >> 31 >> 31 < 3  
  #define ISAAC_BITS_LOG 5
 #else
  #define ISAAC_BITS_LOG 6
 #endif
#endif

 
#define ISAAC_BITS (1 << ISAAC_BITS_LOG)

#if ISAAC_BITS == 32
  typedef uint_least32_t isaac_word;
#else
  typedef uint_least64_t isaac_word;
#endif

 
#define ISAAC_WORDS_LOG 8
#define ISAAC_WORDS (1 << ISAAC_WORDS_LOG)
#define ISAAC_BYTES (ISAAC_WORDS * sizeof (isaac_word))

 
struct isaac_state
  {
    isaac_word m[ISAAC_WORDS];	 
    isaac_word a, b, c;		 
  };

void isaac_seed (struct isaac_state *) _GL_ATTRIBUTE_NONNULL ();
void isaac_refill (struct isaac_state *, isaac_word[ISAAC_WORDS])
  _GL_ATTRIBUTE_NONNULL ();

#endif
