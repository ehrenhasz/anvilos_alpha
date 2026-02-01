 

#include <config.h>

#include "heap.h"
#include "stdlib--.h"
#include "xalloc.h"

static int heap_default_compare (void const *, void const *);
static size_t heapify_down (void **, size_t, size_t,
                            int (*) (void const *, void const *));
static void heapify_up (void **, size_t,
                        int (*) (void const *, void const *));

struct heap
{
  void **array;      
  size_t capacity;   
  size_t count;      
  int (*compare) (void const *, void const *);
};

 

struct heap *
heap_alloc (int (*compare) (void const *, void const *), size_t n_reserve)
{
  struct heap *heap = xmalloc (sizeof *heap);

  if (n_reserve == 0)
    n_reserve = 1;

  heap->array = xnmalloc (n_reserve, sizeof *(heap->array));

  heap->array[0] = nullptr;
  heap->capacity = n_reserve;
  heap->count = 0;
  heap->compare = compare ? compare : heap_default_compare;

  return heap;
}


static int
heap_default_compare (void const *a, void const *b)
{
  return 0;
}


void
heap_free (struct heap *heap)
{
  free (heap->array);
  free (heap);
}

 

int
heap_insert (struct heap *heap, void *item)
{
  if (heap->capacity - 1 <= heap->count)
    heap->array = x2nrealloc (heap->array, &heap->capacity,
                              sizeof *(heap->array));

  heap->array[++heap->count] = item;
  heapify_up (heap->array, heap->count, heap->compare);

  return 0;
}

 

void *
heap_remove_top (struct heap *heap)
{
  void *top;

  if (heap->count == 0)
    return nullptr;

  top = heap->array[1];
  heap->array[1] = heap->array[heap->count--];
  heapify_down (heap->array, heap->count, 1, heap->compare);

  return top;
}

 

static size_t
heapify_down (void **array, size_t count, size_t initial,
              int (*compare) (void const *, void const *))
{
  void *element = array[initial];

  size_t parent = initial;
  while (parent <= count / 2)
    {
      size_t child = 2 * parent;

      if (child < count && compare (array[child], array[child + 1]) < 0)
        child++;

      if (compare (array[child], element) <= 0)
        break;

      array[parent] = array[child];
      parent = child;
    }

  array[parent] = element;
  return parent;
}

 

static void
heapify_up (void **array, size_t count,
            int (*compare) (void const *, void const *))
{
  size_t k = count;
  void *new_element = array[k];

  while (k != 1 && compare (array[k / 2], new_element) <= 0)
    {
      array[k] = array[k / 2];
      k /= 2;
    }

  array[k] = new_element;
}
