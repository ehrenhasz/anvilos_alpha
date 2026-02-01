 
  if (size <= list->allocated)
    {
      list->used = size;
      return true;
    }

   

  size_t new_size_bytes;
  if (ckd_mul (&new_size_bytes, size, element_size))
    {
       
      __set_errno (ENOMEM);
      return false;
    }
  void *new_array;
  if (list->array == scratch)
    {
       
      new_array = malloc (new_size_bytes);
      if (new_array != NULL && list->array != NULL)
        memcpy (new_array, list->array, list->used * element_size);
    }
  else
    new_array = realloc (list->array, new_size_bytes);
  if (new_array == NULL)
    return false;
  list->array = new_array;
  list->allocated = size;
  list->used = size;
  return true;
}
libc_hidden_def (__libc_dynarray_resize)
