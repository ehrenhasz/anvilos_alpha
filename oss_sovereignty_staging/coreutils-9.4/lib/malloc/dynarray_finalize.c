 
    return false;

  size_t used = list->used;

   
  if (used == 0)
    {
       
      if (list->array != scratch)
        free (list->array);
      *result = (struct dynarray_finalize_result) { NULL, 0 };
      return true;
    }

  size_t allocation_size = used * element_size;
  void *heap_array = malloc (allocation_size);
  if (heap_array != NULL)
    {
       
      if (list->array != NULL)
        memcpy (heap_array, list->array, allocation_size);
      if (list->array != scratch)
        free (list->array);
      *result = (struct dynarray_finalize_result)
        { .array = heap_array, .length = used };
      return true;
    }
  else
     
    return false;
}
libc_hidden_def (__libc_dynarray_finalize)
