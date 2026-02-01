 

 

#include <config.h>

#include "hash.h"

#include "bitrotate.h"
#include "xalloc-oversized.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if USE_OBSTACK
# include "obstack.h"
# ifndef obstack_chunk_alloc
#  define obstack_chunk_alloc malloc
# endif
# ifndef obstack_chunk_free
#  define obstack_chunk_free free
# endif
#endif

struct hash_entry
  {
    void *data;
    struct hash_entry *next;
  };

struct hash_table
  {
     
    struct hash_entry *bucket;
    struct hash_entry const *bucket_limit;
    size_t n_buckets;
    size_t n_buckets_used;
    size_t n_entries;

     
    const Hash_tuning *tuning;

     
    Hash_hasher hasher;
    Hash_comparator comparator;
    Hash_data_freer data_freer;

     
    struct hash_entry *free_entry_list;

#if USE_OBSTACK
     
    struct obstack entry_stack;
#endif
  };

 

 
#define DEFAULT_GROWTH_THRESHOLD 0.8f
#define DEFAULT_GROWTH_FACTOR 1.414f

 
#define DEFAULT_SHRINK_THRESHOLD 0.0f
#define DEFAULT_SHRINK_FACTOR 1.0f

 
static const Hash_tuning default_tuning =
  {
    DEFAULT_SHRINK_THRESHOLD,
    DEFAULT_SHRINK_FACTOR,
    DEFAULT_GROWTH_THRESHOLD,
    DEFAULT_GROWTH_FACTOR,
    false
  };

 

size_t
hash_get_n_buckets (const Hash_table *table)
{
  return table->n_buckets;
}

size_t
hash_get_n_buckets_used (const Hash_table *table)
{
  return table->n_buckets_used;
}

size_t
hash_get_n_entries (const Hash_table *table)
{
  return table->n_entries;
}

size_t
hash_get_max_bucket_length (const Hash_table *table)
{
  struct hash_entry const *bucket;
  size_t max_bucket_length = 0;

  for (bucket = table->bucket; bucket < table->bucket_limit; bucket++)
    {
      if (bucket->data)
        {
          struct hash_entry const *cursor = bucket;
          size_t bucket_length = 1;

          while (cursor = cursor->next, cursor)
            bucket_length++;

          if (bucket_length > max_bucket_length)
            max_bucket_length = bucket_length;
        }
    }

  return max_bucket_length;
}

bool
hash_table_ok (const Hash_table *table)
{
  struct hash_entry const *bucket;
  size_t n_buckets_used = 0;
  size_t n_entries = 0;

  for (bucket = table->bucket; bucket < table->bucket_limit; bucket++)
    {
      if (bucket->data)
        {
          struct hash_entry const *cursor = bucket;

           
          n_buckets_used++;
          n_entries++;

           
          while (cursor = cursor->next, cursor)
            n_entries++;
        }
    }

  if (n_buckets_used == table->n_buckets_used && n_entries == table->n_entries)
    return true;

  return false;
}

void
hash_print_statistics (const Hash_table *table, FILE *stream)
{
  size_t n_entries = hash_get_n_entries (table);
  size_t n_buckets = hash_get_n_buckets (table);
  size_t n_buckets_used = hash_get_n_buckets_used (table);
  size_t max_bucket_length = hash_get_max_bucket_length (table);

  fprintf (stream, "# entries:         %lu\n", (unsigned long int) n_entries);
  fprintf (stream, "# buckets:         %lu\n", (unsigned long int) n_buckets);
  fprintf (stream, "# buckets used:    %lu (%.2f%%)\n",
           (unsigned long int) n_buckets_used,
           (100.0 * n_buckets_used) / n_buckets);
  fprintf (stream, "max bucket length: %lu\n",
           (unsigned long int) max_bucket_length);
}

 
static struct hash_entry *
safe_hasher (const Hash_table *table, const void *key)
{
  size_t n = table->hasher (key, table->n_buckets);
  if (! (n < table->n_buckets))
    abort ();
  return table->bucket + n;
}

void *
hash_lookup (const Hash_table *table, const void *entry)
{
  struct hash_entry const *bucket = safe_hasher (table, entry);
  struct hash_entry const *cursor;

  if (bucket->data == NULL)
    return NULL;

  for (cursor = bucket; cursor; cursor = cursor->next)
    if (entry == cursor->data || table->comparator (entry, cursor->data))
      return cursor->data;

  return NULL;
}

 

void *
hash_get_first (const Hash_table *table)
{
  struct hash_entry const *bucket;

  if (table->n_entries == 0)
    return NULL;

  for (bucket = table->bucket; ; bucket++)
    if (! (bucket < table->bucket_limit))
      abort ();
    else if (bucket->data)
      return bucket->data;
}

void *
hash_get_next (const Hash_table *table, const void *entry)
{
  struct hash_entry const *bucket = safe_hasher (table, entry);
  struct hash_entry const *cursor;

   
  cursor = bucket;
  do
    {
      if (cursor->data == entry && cursor->next)
        return cursor->next->data;
      cursor = cursor->next;
    }
  while (cursor != NULL);

   
  while (++bucket < table->bucket_limit)
    if (bucket->data)
      return bucket->data;

   
  return NULL;
}

size_t
hash_get_entries (const Hash_table *table, void **buffer,
                  size_t buffer_size)
{
  size_t counter = 0;
  struct hash_entry const *bucket;
  struct hash_entry const *cursor;

  for (bucket = table->bucket; bucket < table->bucket_limit; bucket++)
    {
      if (bucket->data)
        {
          for (cursor = bucket; cursor; cursor = cursor->next)
            {
              if (counter >= buffer_size)
                return counter;
              buffer[counter++] = cursor->data;
            }
        }
    }

  return counter;
}

size_t
hash_do_for_each (const Hash_table *table, Hash_processor processor,
                  void *processor_data)
{
  size_t counter = 0;
  struct hash_entry const *bucket;
  struct hash_entry const *cursor;

  for (bucket = table->bucket; bucket < table->bucket_limit; bucket++)
    {
      if (bucket->data)
        {
          for (cursor = bucket; cursor; cursor = cursor->next)
            {
              if (! processor (cursor->data, processor_data))
                return counter;
              counter++;
            }
        }
    }

  return counter;
}

 

#if USE_DIFF_HASH

 

size_t
hash_string (const char *string, size_t n_buckets)
{
# define HASH_ONE_CHAR(Value, Byte) \
  ((Byte) + rotl_sz (Value, 7))

  size_t value = 0;
  unsigned char ch;

  for (; (ch = *string); string++)
    value = HASH_ONE_CHAR (value, ch);
  return value % n_buckets;

# undef HASH_ONE_CHAR
}

#else  

 

size_t
hash_string (const char *string, size_t n_buckets)
{
  size_t value = 0;
  unsigned char ch;

  for (; (ch = *string); string++)
    value = (value * 31 + ch) % n_buckets;
  return value;
}

#endif  

 

static bool _GL_ATTRIBUTE_CONST
is_prime (size_t candidate)
{
  size_t divisor = 3;
  size_t square = divisor * divisor;

  while (square < candidate && (candidate % divisor))
    {
      divisor++;
      square += 4 * divisor;
      divisor++;
    }

  return (candidate % divisor ? true : false);
}

 

static size_t _GL_ATTRIBUTE_CONST
next_prime (size_t candidate)
{
   
  if (candidate < 10)
    candidate = 10;

   
  candidate |= 1;

  while (SIZE_MAX != candidate && !is_prime (candidate))
    candidate += 2;

  return candidate;
}

void
hash_reset_tuning (Hash_tuning *tuning)
{
  *tuning = default_tuning;
}

 
static size_t
raw_hasher (const void *data, size_t n)
{
   
  size_t val = rotr_sz ((size_t) data, 3);
  return val % n;
}

 
static bool
raw_comparator (const void *a, const void *b)
{
  return a == b;
}


 

static bool
check_tuning (Hash_table *table)
{
  const Hash_tuning *tuning = table->tuning;
  float epsilon;
  if (tuning == &default_tuning)
    return true;

   
  epsilon = 0.1f;

  if (epsilon < tuning->growth_threshold
      && tuning->growth_threshold < 1 - epsilon
      && 1 + epsilon < tuning->growth_factor
      && 0 <= tuning->shrink_threshold
      && tuning->shrink_threshold + epsilon < tuning->shrink_factor
      && tuning->shrink_factor <= 1
      && tuning->shrink_threshold + epsilon < tuning->growth_threshold)
    return true;

  table->tuning = &default_tuning;
  return false;
}

 

static size_t _GL_ATTRIBUTE_PURE
compute_bucket_size (size_t candidate, const Hash_tuning *tuning)
{
  if (!tuning->is_n_buckets)
    {
      float new_candidate = candidate / tuning->growth_threshold;
      if ((float) SIZE_MAX <= new_candidate)
        goto nomem;
      candidate = new_candidate;
    }
  candidate = next_prime (candidate);
  if (xalloc_oversized (candidate, sizeof (struct hash_entry *)))
    goto nomem;
  return candidate;

 nomem:
  errno = ENOMEM;
  return 0;
}

Hash_table *
hash_initialize (size_t candidate, const Hash_tuning *tuning,
                 Hash_hasher hasher, Hash_comparator comparator,
                 Hash_data_freer data_freer)
{
  Hash_table *table;

  if (hasher == NULL)
    hasher = raw_hasher;
  if (comparator == NULL)
    comparator = raw_comparator;

  table = malloc (sizeof *table);
  if (table == NULL)
    return NULL;

  if (!tuning)
    tuning = &default_tuning;
  table->tuning = tuning;
  if (!check_tuning (table))
    {
       
      errno = EINVAL;
      goto fail;
    }

  table->n_buckets = compute_bucket_size (candidate, tuning);
  if (!table->n_buckets)
    goto fail;

  table->bucket = calloc (table->n_buckets, sizeof *table->bucket);
  if (table->bucket == NULL)
    goto fail;
  table->bucket_limit = table->bucket + table->n_buckets;
  table->n_buckets_used = 0;
  table->n_entries = 0;

  table->hasher = hasher;
  table->comparator = comparator;
  table->data_freer = data_freer;

  table->free_entry_list = NULL;
#if USE_OBSTACK
  obstack_init (&table->entry_stack);
#endif
  return table;

 fail:
  free (table);
  return NULL;
}

void
hash_clear (Hash_table *table)
{
  struct hash_entry *bucket;

  for (bucket = table->bucket; bucket < table->bucket_limit; bucket++)
    {
      if (bucket->data)
        {
          struct hash_entry *cursor;
          struct hash_entry *next;

           
          for (cursor = bucket->next; cursor; cursor = next)
            {
              if (table->data_freer)
                table->data_freer (cursor->data);
              cursor->data = NULL;

              next = cursor->next;
               
              cursor->next = table->free_entry_list;
              table->free_entry_list = cursor;
            }

           
          if (table->data_freer)
            table->data_freer (bucket->data);
          bucket->data = NULL;
          bucket->next = NULL;
        }
    }

  table->n_buckets_used = 0;
  table->n_entries = 0;
}

void
hash_free (Hash_table *table)
{
  struct hash_entry *bucket;
  struct hash_entry *cursor;
  struct hash_entry *next;
  int err = errno;

   
  if (table->data_freer && table->n_entries)
    {
      for (bucket = table->bucket; bucket < table->bucket_limit; bucket++)
        {
          if (bucket->data)
            {
              for (cursor = bucket; cursor; cursor = cursor->next)
                table->data_freer (cursor->data);
            }
        }
    }

#if USE_OBSTACK

  obstack_free (&table->entry_stack, NULL);

#else

   
  for (bucket = table->bucket; bucket < table->bucket_limit; bucket++)
    {
      for (cursor = bucket->next; cursor; cursor = next)
        {
          next = cursor->next;
          free (cursor);
        }
    }

   
  for (cursor = table->free_entry_list; cursor; cursor = next)
    {
      next = cursor->next;
      free (cursor);
    }

#endif

   
  free (table->bucket);
  free (table);

  errno = err;
}

 

 

static struct hash_entry *
allocate_entry (Hash_table *table)
{
  struct hash_entry *new;

  if (table->free_entry_list)
    {
      new = table->free_entry_list;
      table->free_entry_list = new->next;
    }
  else
    {
#if USE_OBSTACK
      new = obstack_alloc (&table->entry_stack, sizeof *new);
#else
      new = malloc (sizeof *new);
#endif
    }

  return new;
}

 

static void
free_entry (Hash_table *table, struct hash_entry *entry)
{
  entry->data = NULL;
  entry->next = table->free_entry_list;
  table->free_entry_list = entry;
}

 

static void *
hash_find_entry (Hash_table *table, const void *entry,
                 struct hash_entry **bucket_head, bool delete)
{
  struct hash_entry *bucket = safe_hasher (table, entry);
  struct hash_entry *cursor;

  *bucket_head = bucket;

   
  if (bucket->data == NULL)
    return NULL;

   
  if (entry == bucket->data || table->comparator (entry, bucket->data))
    {
      void *data = bucket->data;

      if (delete)
        {
          if (bucket->next)
            {
              struct hash_entry *next = bucket->next;

               
              *bucket = *next;
              free_entry (table, next);
            }
          else
            {
              bucket->data = NULL;
            }
        }

      return data;
    }

   
  for (cursor = bucket; cursor->next; cursor = cursor->next)
    {
      if (entry == cursor->next->data
          || table->comparator (entry, cursor->next->data))
        {
          void *data = cursor->next->data;

          if (delete)
            {
              struct hash_entry *next = cursor->next;

               
              cursor->next = next->next;
              free_entry (table, next);
            }

          return data;
        }
    }

   
  return NULL;
}

 

static bool
transfer_entries (Hash_table *dst, Hash_table *src, bool safe)
{
  struct hash_entry *bucket;
  struct hash_entry *cursor;
  struct hash_entry *next;
  for (bucket = src->bucket; bucket < src->bucket_limit; bucket++)
    if (bucket->data)
      {
        void *data;
        struct hash_entry *new_bucket;

         
        for (cursor = bucket->next; cursor; cursor = next)
          {
            data = cursor->data;
            new_bucket = safe_hasher (dst, data);

            next = cursor->next;

            if (new_bucket->data)
              {
                 
                cursor->next = new_bucket->next;
                new_bucket->next = cursor;
              }
            else
              {
                 
                new_bucket->data = data;
                dst->n_buckets_used++;
                free_entry (dst, cursor);
              }
          }
         
        data = bucket->data;
        bucket->next = NULL;
        if (safe)
          continue;
        new_bucket = safe_hasher (dst, data);

        if (new_bucket->data)
          {
             
            struct hash_entry *new_entry = allocate_entry (dst);

            if (new_entry == NULL)
              return false;

            new_entry->data = data;
            new_entry->next = new_bucket->next;
            new_bucket->next = new_entry;
          }
        else
          {
             
            new_bucket->data = data;
            dst->n_buckets_used++;
          }
        bucket->data = NULL;
        src->n_buckets_used--;
      }
  return true;
}

bool
hash_rehash (Hash_table *table, size_t candidate)
{
  Hash_table storage;
  Hash_table *new_table;
  size_t new_size = compute_bucket_size (candidate, table->tuning);

  if (!new_size)
    return false;
  if (new_size == table->n_buckets)
    return true;
  new_table = &storage;
  new_table->bucket = calloc (new_size, sizeof *new_table->bucket);
  if (new_table->bucket == NULL)
    return false;
  new_table->n_buckets = new_size;
  new_table->bucket_limit = new_table->bucket + new_size;
  new_table->n_buckets_used = 0;
  new_table->n_entries = 0;
  new_table->tuning = table->tuning;
  new_table->hasher = table->hasher;
  new_table->comparator = table->comparator;
  new_table->data_freer = table->data_freer;

   

   
#if USE_OBSTACK
  new_table->entry_stack = table->entry_stack;
#endif
  new_table->free_entry_list = table->free_entry_list;

  if (transfer_entries (new_table, table, false))
    {
       
      free (table->bucket);
      table->bucket = new_table->bucket;
      table->bucket_limit = new_table->bucket_limit;
      table->n_buckets = new_table->n_buckets;
      table->n_buckets_used = new_table->n_buckets_used;
      table->free_entry_list = new_table->free_entry_list;
       
      return true;
    }

   
  int err = errno;
  table->free_entry_list = new_table->free_entry_list;
  if (! (transfer_entries (table, new_table, true)
         && transfer_entries (table, new_table, false)))
    abort ();
   
  free (new_table->bucket);
  errno = err;
  return false;
}

int
hash_insert_if_absent (Hash_table *table, void const *entry,
                       void const **matched_ent)
{
  void *data;
  struct hash_entry *bucket;

   
  if (! entry)
    abort ();

   
  if ((data = hash_find_entry (table, entry, &bucket, false)) != NULL)
    {
      if (matched_ent)
        *matched_ent = data;
      return 0;
    }

   

  if (table->n_buckets_used
      > table->tuning->growth_threshold * table->n_buckets)
    {
       
      check_tuning (table);
      if (table->n_buckets_used
          > table->tuning->growth_threshold * table->n_buckets)
        {
          const Hash_tuning *tuning = table->tuning;
          float candidate =
            (tuning->is_n_buckets
             ? (table->n_buckets * tuning->growth_factor)
             : (table->n_buckets * tuning->growth_factor
                * tuning->growth_threshold));

          if ((float) SIZE_MAX <= candidate)
            {
              errno = ENOMEM;
              return -1;
            }

           
          if (!hash_rehash (table, candidate))
            return -1;

           
          if (hash_find_entry (table, entry, &bucket, false) != NULL)
            abort ();
        }
    }

   

  if (bucket->data)
    {
      struct hash_entry *new_entry = allocate_entry (table);

      if (new_entry == NULL)
        return -1;

       

      new_entry->data = (void *) entry;
      new_entry->next = bucket->next;
      bucket->next = new_entry;
      table->n_entries++;
      return 1;
    }

   

  bucket->data = (void *) entry;
  table->n_entries++;
  table->n_buckets_used++;

  return 1;
}

void *
hash_insert (Hash_table *table, void const *entry)
{
  void const *matched_ent;
  int err = hash_insert_if_absent (table, entry, &matched_ent);
  return (err == -1
          ? NULL
          : (void *) (err == 0 ? matched_ent : entry));
}

void *
hash_remove (Hash_table *table, const void *entry)
{
  void *data;
  struct hash_entry *bucket;

  data = hash_find_entry (table, entry, &bucket, true);
  if (!data)
    return NULL;

  table->n_entries--;
  if (!bucket->data)
    {
      table->n_buckets_used--;

       

      if (table->n_buckets_used
          < table->tuning->shrink_threshold * table->n_buckets)
        {
           
          check_tuning (table);
          if (table->n_buckets_used
              < table->tuning->shrink_threshold * table->n_buckets)
            {
              const Hash_tuning *tuning = table->tuning;
              size_t candidate =
                (tuning->is_n_buckets
                 ? table->n_buckets * tuning->shrink_factor
                 : (table->n_buckets * tuning->shrink_factor
                    * tuning->growth_threshold));

              if (!hash_rehash (table, candidate))
                {
                   
#if ! USE_OBSTACK
                  struct hash_entry *cursor = table->free_entry_list;
                  struct hash_entry *next;
                  while (cursor)
                    {
                      next = cursor->next;
                      free (cursor);
                      cursor = next;
                    }
                  table->free_entry_list = NULL;
#endif
                }
            }
        }
    }

  return data;
}

void *
hash_delete (Hash_table *table, const void *entry)
{
  return hash_remove (table, entry);
}

 

#if TESTING

void
hash_print (const Hash_table *table)
{
  struct hash_entry *bucket = (struct hash_entry *) table->bucket;

  for ( ; bucket < table->bucket_limit; bucket++)
    {
      struct hash_entry *cursor;

      if (bucket)
        printf ("%lu:\n", (unsigned long int) (bucket - table->bucket));

      for (cursor = bucket; cursor; cursor = cursor->next)
        {
          char const *s = cursor->data;
           
          if (s)
            printf ("  %s\n", s);
        }
    }
}

#endif  
