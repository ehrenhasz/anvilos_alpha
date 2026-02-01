 
  scratch_buffer_free (buffer);

   
  if (__glibc_likely (new_length >= buffer->length))
    new_ptr = malloc (new_length);
  else
    {
      __set_errno (ENOMEM);
      new_ptr = NULL;
    }

  if (__glibc_unlikely (new_ptr == NULL))
    {
       
      scratch_buffer_init (buffer);
      return false;
    }

   
  buffer->data = new_ptr;
  buffer->length = new_length;
  return true;
}
libc_hidden_def (__libc_scratch_buffer_grow)
