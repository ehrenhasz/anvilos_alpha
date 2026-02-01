 

#include <stddef.h>

struct heap;

void heap_free (struct heap *) _GL_ATTRIBUTE_NONNULL ();

struct heap *heap_alloc (int (*) (void const *, void const *), size_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (heap_free, 1)
  _GL_ATTRIBUTE_RETURNS_NONNULL;

int heap_insert (struct heap *heap, void *item) _GL_ATTRIBUTE_NONNULL ();
void *heap_remove_top (struct heap *heap) _GL_ATTRIBUTE_NONNULL ();
