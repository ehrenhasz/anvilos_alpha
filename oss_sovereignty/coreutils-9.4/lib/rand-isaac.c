 

 
#include <config.h>

#include "rand-isaac.h"

#include <limits.h>
#include <string.h>

 
#undef ATTRIBUTE_NO_WARN_SANITIZE_UNDEFINED
#if !(_STRING_ARCH_unaligned || _STRING_INLINE_unaligned) \
    || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
# define ATTRIBUTE_NO_WARN_SANITIZE_UNDEFINED  
#else
# define ATTRIBUTE_NO_WARN_SANITIZE_UNDEFINED \
  __attribute__ ((__no_sanitize_undefined__))
#endif

 
#define IF32(a, b) (ISAAC_BITS == 32 ? (a) : (b))

 
static inline isaac_word
just (isaac_word a)
{
  isaac_word desired_bits = ((isaac_word) 1 << 1 << (ISAAC_BITS - 1)) - 1;
  return a & desired_bits;
}

 
static inline isaac_word
ind (isaac_word const *m, isaac_word x)
{
  if (sizeof *m * CHAR_BIT == ISAAC_BITS)
    {
       
      void const *void_m = m;
      char const *base_p = void_m;
      void const *word_p = base_p + (x & ((ISAAC_WORDS - 1) * sizeof *m));
      isaac_word const *p = word_p;
      return *p;
    }
  else
    {
       
      return m[(x / (ISAAC_BITS / CHAR_BIT)) & (ISAAC_WORDS - 1)];
    }
}

 
void ATTRIBUTE_NO_WARN_SANITIZE_UNDEFINED
isaac_refill (struct isaac_state *s, isaac_word result[ISAAC_WORDS])
{
   
  isaac_word a = s->a;
  isaac_word b = s->b + (++s->c);

   
  isaac_word *m = s->m;
  isaac_word *r = result;

  enum { HALF = ISAAC_WORDS / 2 };

   
  #define ISAAC_STEP(i, off, mix)                             \
    {                                                         \
      isaac_word x, y;                                        \
      a = (IF32 (a, 0) ^ (mix)) + m[off + (i)];               \
      x = m[i];                                               \
      m[i] = y = ind (s->m, x) + a + b;                       \
      r[i] = b = just (ind (s->m, y >> ISAAC_WORDS_LOG) + x); \
    }

  do
    {
      ISAAC_STEP (0, HALF, IF32 (      a  << 13, ~ (a ^ (a << 21))));
      ISAAC_STEP (1, HALF, IF32 (just (a) >>  6, a ^ (just (a) >>  5)));
      ISAAC_STEP (2, HALF, IF32 (      a  <<  2, a ^ (      a  << 12)));
      ISAAC_STEP (3, HALF, IF32 (just (a) >> 16, a ^ (just (a) >> 33)));
      r += 4;
    }
  while ((m += 4) < s->m + HALF);

  do
    {
      ISAAC_STEP (0, -HALF, IF32 (      a  << 13, ~ (a ^ (a << 21))));
      ISAAC_STEP (1, -HALF, IF32 (just (a) >>  6, a ^ (just (a) >>  5)));
      ISAAC_STEP (2, -HALF, IF32 (      a  <<  2, a ^ (      a  << 12)));
      ISAAC_STEP (3, -HALF, IF32 (just (a) >> 16, a ^ (just (a) >> 33)));
      r += 4;
    }
  while ((m += 4) < s->m + ISAAC_WORDS);

  s->a = a;
  s->b = b;
}

 
#if ISAAC_BITS == 32
 #define mix(a, b, c, d, e, f, g, h)       \
    {                                      \
              a ^=       b  << 11; d += a; \
      b += c; b ^= just (c) >>  2; e += b; \
      c += d; c ^=       d  <<  8; f += c; \
      d += e; d ^= just (e) >> 16; g += d; \
      e += f; e ^=       f  << 10; h += e; \
      f += g; f ^= just (g) >>  4; a += f; \
      g += h; g ^=       h  <<  8; b += g; \
      h += a; h ^= just (a) >>  9; c += h; \
      a += b;                              \
    }
#else
 #define mix(a, b, c, d, e, f, g, h)       \
    {                                      \
      a -= e; f ^= just (h) >>  9; h += a; \
      b -= f; g ^=       a  <<  9; a += b; \
      c -= g; h ^= just (b) >> 23; b += c; \
      d -= h; a ^=       c  << 15; c += d; \
      e -= a; b ^= just (d) >> 14; d += e; \
      f -= b; c ^=       e  << 20; e += f; \
      g -= c; d ^= just (f) >> 17; f += g; \
      h -= d; e ^=       g  << 14; g += h; \
    }
#endif


 
#define ISAAC_MIX(s, a, b, c, d, e, f, g, h, seed) \
  {                                                \
    int i;                                         \
                                                   \
    for (i = 0; i < ISAAC_WORDS; i += 8)           \
      {                                            \
        a += seed[i];                              \
        b += seed[i + 1];                          \
        c += seed[i + 2];                          \
        d += seed[i + 3];                          \
        e += seed[i + 4];                          \
        f += seed[i + 5];                          \
        g += seed[i + 6];                          \
        h += seed[i + 7];                          \
        mix (a, b, c, d, e, f, g, h);              \
        s->m[i] = a;                               \
        s->m[i + 1] = b;                           \
        s->m[i + 2] = c;                           \
        s->m[i + 3] = d;                           \
        s->m[i + 4] = e;                           \
        s->m[i + 5] = f;                           \
        s->m[i + 6] = g;                           \
        s->m[i + 7] = h;                           \
      }                                            \
  }

#if 0  
 
static void
isaac_init (struct isaac_state *s, isaac_word const *seed, size_t seedsize)
{
  isaac_word a, b, c, d, e, f, g, h;

  a = b = c = d = e = f = g = h =           
    IF32 (UINT32_C (0x9e3779b9), UINT64_C (0x9e3779b97f4a7c13));
  for (int i = 0; i < 4; i++)               
    mix (a, b, c, d, e, f, g, h);
  s->a = s->b = s->c = 0;

  if (seedsize)
    {
       
      ISAAC_MIX (s, a, b, c, d, e, f, g, h, seed);
       
      while (seedsize -= ISAAC_BYTES)
        {
          seed += ISAAC_WORDS;
          for (i = 0; i < ISAAC_WORDS; i++)
            s->m[i] += seed[i];
          ISAAC_MIX (s, a, b, c, d, e, f, g, h, s->m);
        }
    }
  else
    {
       
      for (i = 0; i < ISAAC_WORDS; i++)
        s->m[i] = 0;
    }

   
  ISAAC_MIX (s, a, b, c, d, e, f, g, h, s->m);
}
#endif

 
void
isaac_seed (struct isaac_state *s)
{
  isaac_word a = IF32 (UINT32_C (0x1367df5a), UINT64_C (0x647c4677a2884b7c));
  isaac_word b = IF32 (UINT32_C (0x95d90059), UINT64_C (0xb9f8b322c73ac862));
  isaac_word c = IF32 (UINT32_C (0xc3163e4b), UINT64_C (0x8c0ea5053d4712a0));
  isaac_word d = IF32 (UINT32_C (0x0f421ad8), UINT64_C (0xb29b2e824a595524));
  isaac_word e = IF32 (UINT32_C (0xd92a4a78), UINT64_C (0x82f053db8355e0ce));
  isaac_word f = IF32 (UINT32_C (0xa51a3c49), UINT64_C (0x48fe4a0fa5a09315));
  isaac_word g = IF32 (UINT32_C (0xc4efea1b), UINT64_C (0xae985bf2cbfc89ed));
  isaac_word h = IF32 (UINT32_C (0x30609119), UINT64_C (0x98f5704f6c44c0ab));

#if 0
   
  a = b = c = d = e = f = g = h =           
    IF32 (UINT32_C (0x9e3779b9), UINT64_C (0x9e3779b97f4a7c13));
  for (int i = 0; i < 4; i++)               
    mix (a, b, c, d, e, f, g, h);
#endif

   
  ISAAC_MIX (s, a, b, c, d, e, f, g, h, s->m);
  ISAAC_MIX (s, a, b, c, d, e, f, g, h, s->m);

  s->a = s->b = s->c = 0;
}
