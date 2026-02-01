 
  {
    size_t i;
    for (i = 0; i < size; i++)
      memblock[i] =
        (unsigned char) (((i * (i-1) * (i-5)) >> 6) + (i % 499) + (i % 101));
  }

  struct timings_state ts;
  timing_start (&ts);

  int count;
  for (count = 0; count < repeat; count++)
    {
      char digest[64];
      FUNC (memblock, size, digest);
    }

  timing_end (&ts);
  timing_output (&ts);

  return 0;
}
