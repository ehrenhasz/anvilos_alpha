 
  char *array = list->array;
  memset (array + (old_size * element_size), 0,
          (size - old_size) * element_size);
  return true;
}
libc_hidden_def (__libc_dynarray_resize_clear)
