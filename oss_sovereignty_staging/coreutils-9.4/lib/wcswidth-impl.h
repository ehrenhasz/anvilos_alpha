 
  for (; n > 0; s++, n--)
    {
      UNIT c = *s;
      if (c == (UNIT)'\0')
        break;
      {
        int width = CHARACTER_WIDTH (c);
        if (width < 0)
          goto found_nonprinting;
      }
     overflow: ;
    }
  return INT_MAX;

 found_nonprinting:
  return -1;
}
