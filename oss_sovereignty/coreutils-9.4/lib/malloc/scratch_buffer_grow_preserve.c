 
      new_ptr = malloc (new_length);
      if (new_ptr == NULL)
	return false;
      memcpy (new_ptr, buffer->__space.__c, buffer->length);
    }
  else
    {
       
      if (__glibc_likely (new_length >= buffer->length))
	new_ptr = realloc (buffer->data, new_length);
      else
	{
	  __set_errno (ENOMEM);
	  new_ptr = NULL;
	}

      if (__glibc_unlikely (new_ptr == NULL))
	{
	   
	  free (buffer->data);
	  scratch_buffer_init (buffer);
	  return false;
	}
    }

   
  buffer->data = new_ptr;
  buffer->length = new_length;
  return true;
}
libc_hidden_def (__libc_scratch_buffer_grow_preserve)
