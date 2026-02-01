 

 

#ifndef HASH_H_
# define HASH_H_

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

# include <stdio.h>

# ifdef __cplusplus
extern "C" {
# endif

struct hash_tuning
  {
     

    float shrink_threshold;      
    float shrink_factor;         
    float growth_threshold;      
    float growth_factor;         
    bool is_n_buckets;           
  };

typedef struct hash_tuning Hash_tuning;

struct hash_table;

typedef struct hash_table Hash_table;

 

 

 
extern size_t hash_get_n_buckets (const Hash_table *table)
       _GL_ATTRIBUTE_PURE;

 
extern size_t hash_get_n_buckets_used (const Hash_table *table)
       _GL_ATTRIBUTE_PURE;

 
extern size_t hash_get_n_entries (const Hash_table *table)
       _GL_ATTRIBUTE_PURE;

 
extern size_t hash_get_max_bucket_length (const Hash_table *table)
       _GL_ATTRIBUTE_PURE;

 
extern bool hash_table_ok (const Hash_table *table)
       _GL_ATTRIBUTE_PURE;

extern void hash_print_statistics (const Hash_table *table, FILE *stream);

 
extern void *hash_lookup (const Hash_table *table, const void *entry);

 

 

 
extern void *hash_get_first (const Hash_table *table)
       _GL_ATTRIBUTE_PURE;

 
extern void *hash_get_next (const Hash_table *table, const void *entry);

 
extern size_t hash_get_entries (const Hash_table *table, void **buffer,
                                size_t buffer_size);

typedef bool (*Hash_processor) (void *entry, void *processor_data);

 
extern size_t hash_do_for_each (const Hash_table *table,
                                Hash_processor processor, void *processor_data);

 

 
extern size_t hash_string (const char *string, size_t n_buckets)
       _GL_ATTRIBUTE_PURE;

extern void hash_reset_tuning (Hash_tuning *tuning);

typedef size_t (*Hash_hasher) (const void *entry, size_t table_size);
typedef bool (*Hash_comparator) (const void *entry1, const void *entry2);
typedef void (*Hash_data_freer) (void *entry);

 
extern void hash_free (Hash_table *table);

 
_GL_ATTRIBUTE_NODISCARD
extern Hash_table *hash_initialize (size_t candidate,
                                    const Hash_tuning *tuning,
                                    Hash_hasher hasher,
                                    Hash_comparator comparator,
                                    Hash_data_freer data_freer)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (hash_free, 1);

 
 
_GL_ATTRIBUTE_NODISCARD
extern Hash_table *hash_xinitialize (size_t candidate,
                                     const Hash_tuning *tuning,
                                     Hash_hasher hasher,
                                     Hash_comparator comparator,
                                     Hash_data_freer data_freer)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (hash_free, 1)
  _GL_ATTRIBUTE_RETURNS_NONNULL;

 
extern void hash_clear (Hash_table *table);

 

 
_GL_ATTRIBUTE_NODISCARD
extern bool hash_rehash (Hash_table *table, size_t candidate);

 
_GL_ATTRIBUTE_NODISCARD
extern void *hash_insert (Hash_table *table, const void *entry);

 
 
extern void *hash_xinsert (Hash_table *table, const void *entry);

 
extern int hash_insert_if_absent (Hash_table *table, const void *entry,
                                  const void **matched_ent);

 
extern void *hash_remove (Hash_table *table, const void *entry);

 
_GL_ATTRIBUTE_DEPRECATED
extern void *hash_delete (Hash_table *table, const void *entry);

# ifdef __cplusplus
}
# endif

#endif
