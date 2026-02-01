 
      if (element_size < 4)
        new_allocated = 16;
      else if (element_size < 8)
        new_allocated = 8;
      else
        new_allocated = 4;
    }
  else
     
    {
      new_allocated = list->allocated + list->allocated / 2 + 1;
      if (new_allocated <= list->allocated)
        {
           
          __set_errno (ENOMEM);
          return false;
        }
    }

  size_t new_size;
  if (ckd_mul (&new_size, new_allocated, element_size))
    return false;
  void *new_array;
  if (list->array == scratch)
    {
       
      new_array = malloc (new_size);
      if (new_array != NULL && list->array != NULL)
        memcpy (new_array, list->array, list->used * element_size);
    }
  else
    new_array = realloc (list->array, new_size);
  if (new_array == NULL)
    return false;
  list->array = new_array;
  list->allocated = new_allocated;
  return true;
}
libc_hidden_def (__libc_dynarray_emplace_enlarge)
