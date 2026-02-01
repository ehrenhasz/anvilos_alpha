 
  if ((nelem | size) >> (sizeof (size_t) * CHAR_BIT / 2) != 0
      && nelem != 0 && size != new_length / nelem)
    {
       
      scratch_buffer_free (buffer);
      scratch_buffer_init (buffer);
      __set_errno (ENOMEM);
      return false;
    }

  if (new_length <= buffer->length)
    return true;

   
  scratch_buffer_free (buffer);

  char *new_ptr = malloc (new_length);
  if (new_ptr == NULL)
    {
       
      scratch_buffer_init (buffer);
      return false;
    }

   
  buffer->data = new_ptr;
  buffer->length = new_length;
  return true;
}
libc_hidden_def (__libc_scratch_buffer_set_array_size)
