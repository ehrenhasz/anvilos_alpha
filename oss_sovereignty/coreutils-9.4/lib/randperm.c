 

#include <config.h>

#include "randperm.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "attribute.h"
#include "count-leading-zeros.h"
#include "hash.h"
#include "xalloc.h"

 

ATTRIBUTE_CONST static int
floor_lg (size_t n)
{
  static_assert (SIZE_WIDTH <= ULLONG_WIDTH);
  return (n == 0 ? -1
          : SIZE_WIDTH <= UINT_WIDTH
          ? UINT_WIDTH - 1 - count_leading_zeros (n)
          : SIZE_WIDTH <= ULONG_WIDTH
          ? ULONG_WIDTH - 1 - count_leading_zeros_l (n)
          : ULLONG_WIDTH - 1 - count_leading_zeros_ll (n));
}

 

size_t
randperm_bound (size_t h, size_t n)
{
   
  uintmax_t lg_n = floor_lg (n) + 1;

   
  uintmax_t ar = lg_n * h;

   
  size_t bound = (ar + CHAR_BIT - 1) / CHAR_BIT;

  return bound;
}

 

static void
swap (size_t *v, size_t i, size_t j)
{
  size_t t = v[i];
  v[i] = v[j];
  v[j] = t;
}

 

struct sparse_ent_
{
   size_t index;
   size_t val;
};

static size_t
sparse_hash_ (void const *x, size_t table_size)
{
  struct sparse_ent_ const *ent = x;
  return ent->index % table_size;
}

static bool
sparse_cmp_ (void const *x, void const *y)
{
  struct sparse_ent_ const *ent1 = x;
  struct sparse_ent_ const *ent2 = y;
  return ent1->index == ent2->index;
}

typedef Hash_table sparse_map;

 

static sparse_map *
sparse_new (size_t size_hint)
{
  return hash_initialize (size_hint, nullptr, sparse_hash_, sparse_cmp_, free);
}

 

static void
sparse_swap (sparse_map *sv, size_t *v, size_t i, size_t j)
{
  struct sparse_ent_ *v1 = hash_remove (sv, &(struct sparse_ent_) {i,0});
  struct sparse_ent_ *v2 = hash_remove (sv, &(struct sparse_ent_) {j,0});

   
  if (!v1)
    {
      v1 = xmalloc (sizeof *v1);
      v1->index = v1->val = i;
    }
  if (!v2)
    {
      v2 = xmalloc (sizeof *v2);
      v2->index = v2->val = j;
    }

  size_t t = v1->val;
  v1->val = v2->val;
  v2->val = t;
  if (!hash_insert (sv, v1))
    xalloc_die ();
  if (!hash_insert (sv, v2))
    xalloc_die ();

  v[i] = v1->val;
}

static void
sparse_free (sparse_map *sv)
{
  hash_free (sv);
}


 

size_t *
randperm_new (struct randint_source *r, size_t h, size_t n)
{
  size_t *v;

  switch (h)
    {
    case 0:
      v = nullptr;
      break;

    case 1:
      v = xmalloc (sizeof *v);
      v[0] = randint_choose (r, n);
      break;

    default:
      {
         
        bool sparse = (n >= (128 * 1024)) && (n / h >= 32);

        size_t i;
        sparse_map *sv;

        if (sparse)
          {
            sv = sparse_new (h * 2);
            if (sv == nullptr)
              xalloc_die ();
            v = xnmalloc (h, sizeof *v);
          }
        else
          {
            sv = nullptr;  
            v = xnmalloc (n, sizeof *v);
            for (i = 0; i < n; i++)
              v[i] = i;
          }

        for (i = 0; i < h; i++)
          {
            size_t j = i + randint_choose (r, n - i);
            if (sparse)
              sparse_swap (sv, v, i, j);
            else
              swap (v, i, j);
          }

        if (sparse)
          sparse_free (sv);
        else
          v = xnrealloc (v, h, sizeof *v);
      }
      break;
    }

  return v;
}
