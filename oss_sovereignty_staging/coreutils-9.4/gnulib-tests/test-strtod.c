 
#define FABS(d) ((d) < 0.0 ? -(d) : (d))

int
main (void)
{
  int status = 0;
   
  {
    const char input[] = "";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " ";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " +";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " .";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " .e0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);               
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " +.e-0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);               
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " in";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }
  {
    const char input[] = " na";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
  }

   
  {
    const char input[] = "1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1.";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = ".5";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.5);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = " 1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "+1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == -1.0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e+0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e-0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 10.0);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "5e-1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.5);
    ASSERT (ptr == input + 4);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = ".0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0e0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0e+9999999";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0e-9999999";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 10);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-0";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!!signbit (result) == !!signbit (minus_zerod));  
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "1f";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1.f";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e+";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1e-";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1E 2";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);              
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "00x1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "-0x";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!!signbit (result) == !!signbit (minus_zerod));  
    ASSERT (ptr == input + 2);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xg";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xp";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0XP";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x.";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xp+";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0xp+1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x.p+1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1p+1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1P+1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);
    ASSERT (ptr == input + 1);
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "1E1000000";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == HUGE_VAL);
    ASSERT (ptr == input + 9);           
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "-1E1000000";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == -HUGE_VAL);
    ASSERT (ptr == input + 10);
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "1E-100000";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (0.0 <= result && result <= DBL_MIN);
    ASSERT (!signbit (result));
    ASSERT (ptr == input + 9);
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "-1E-100000";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (-DBL_MIN <= result && result <= 0.0);
#if 0
     
    ASSERT (!!signbit (result) == !!signbit (minus_zerod));  
#endif
    ASSERT (ptr == input + 10);
    ASSERT (errno == ERANGE);
  }
  {
    const char input[] = "1E 1000000";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);              
    ASSERT (ptr == input + 1);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1P 1000000";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);              
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);
  }

   
  {
    const char input[] = "iNf";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == HUGE_VAL);         
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);                 
  }
  {
    const char input[] = "-InF";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == -HUGE_VAL);        
    ASSERT (ptr == input + 4);           
    ASSERT (errno == 0);                 
  }
  {
    const char input[] = "infinite";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == HUGE_VAL);         
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);                 
  }
  {
    const char input[] = "infinitY";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == HUGE_VAL);         
    ASSERT (ptr == input + 8);           
    ASSERT (errno == 0);                 
  }
  {
    const char input[] = "infinitY.";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == HUGE_VAL);         
    ASSERT (ptr == input + 8);           
    ASSERT (errno == 0);                 
  }

   
  {
    const char input[] = "-nan";
    char *ptr1;
    char *ptr2;
    double result1;
    double result2;
    errno = 0;
    result1 = strtod (input, &ptr1);
    result2 = strtod (input + 1, &ptr2);
#if 1  
    ASSERT (isnand (result1));           
    ASSERT (isnand (result2));           
# if 0
     
    ASSERT (!!signbit (result1) != !!signbit (result2));  
# endif
    ASSERT (ptr1 == input + 4);          
    ASSERT (ptr2 == input + 4);          
    ASSERT (errno == 0);                 
#else
    ASSERT (result1 == 0.0);
    ASSERT (result2 == 0.0);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
    const char input[] = "+nan(";
    char *ptr1;
    char *ptr2;
    double result1;
    double result2;
    errno = 0;
    result1 = strtod (input, &ptr1);
    result2 = strtod (input + 1, &ptr2);
#if 1  
    ASSERT (isnand (result1));           
    ASSERT (isnand (result2));           
    ASSERT (!!signbit (result1) == !!signbit (result2));
    ASSERT (ptr1 == input + 4);          
    ASSERT (ptr2 == input + 4);          
    ASSERT (errno == 0);
#else
    ASSERT (result1 == 0.0);
    ASSERT (result2 == 0.0);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
    const char input[] = "-nan()";
    char *ptr1;
    char *ptr2;
    double result1;
    double result2;
    errno = 0;
    result1 = strtod (input, &ptr1);
    result2 = strtod (input + 1, &ptr2);
#if 1  
    ASSERT (isnand (result1));           
    ASSERT (isnand (result2));           
# if 0
     
    ASSERT (!!signbit (result1) != !!signbit (result2));  
# endif
    ASSERT (ptr1 == input + 6);          
    ASSERT (ptr2 == input + 6);          
    ASSERT (errno == 0);
#else
    ASSERT (result1 == 0.0);
    ASSERT (result2 == 0.0);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
    const char input[] = " nan().";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
#if 1  
    ASSERT (isnand (result));            
    ASSERT (ptr == input + 6);           
    ASSERT (errno == 0);
#else
    ASSERT (result == 0.0);
    ASSERT (!signbit (result));
    ASSERT (ptr == input);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }
  {
     
    const char input[] = "-nan(0).";
    char *ptr1;
    char *ptr2;
    double result1;
    double result2;
    errno = 0;
    result1 = strtod (input, &ptr1);
    result2 = strtod (input + 1, &ptr2);
#if 1  
    ASSERT (isnand (result1));           
    ASSERT (isnand (result2));           
# if 0
     
    ASSERT (!!signbit (result1) != !!signbit (result2));  
# endif
    ASSERT (ptr1 == input + 7);          
    ASSERT (ptr2 == input + 7);          
    ASSERT (errno == 0);
#else
    ASSERT (result1 == 0.0);
    ASSERT (result2 == 0.0);
    ASSERT (!signbit (result1));
    ASSERT (!signbit (result2));
    ASSERT (ptr1 == input);
    ASSERT (ptr2 == input + 1);
    ASSERT (errno == 0 || errno == EINVAL);
#endif
  }

   
  {
    const char input[] = "0xa";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 10.0);             
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0XA";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 10.0);             
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);              
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p+";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);              
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1P+";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);              
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p+1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 2.0);              
    ASSERT (ptr == input + 6);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0X1P+1";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 2.0);              
    ASSERT (ptr == input + 6);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p+1a";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 2.0);              
    ASSERT (ptr == input + 6);           
    ASSERT (errno == 0);
  }
  {
    const char input[] = "0x1p 2";
    char *ptr;
    double result;
    errno = 0;
    result = strtod (input, &ptr);
    ASSERT (result == 1.0);              
    ASSERT (ptr == input + 3);           
    ASSERT (errno == 0);
  }

   
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        double result;
        memset (input, '\t', m - 1);
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = strtod (input, &ptr);
        ASSERT (result == 1.0);
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);
      }
    free (input);
  }
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        double result;
        memset (input, '0', m - 1);
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = strtod (input, &ptr);
        ASSERT (result == 1.0);
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);
      }
    free (input);
  }
#if 0
   
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        double result;
        input[0] = '.';
        memset (input + 1, '0', m - 10);
        input[m - 9] = '1';
        input[m - 8] = 'e';
        input[m - 7] = '+';
        input[m - 6] = '9';
        input[m - 5] = '9';
        input[m - 4] = '9';
        input[m - 3] = '9';
        input[m - 2] = '9';
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = strtod (input, &ptr);
        ASSERT (result == 1.0);          
        ASSERT (ptr == input + m);       
        ASSERT (errno == 0);             
      }
    free (input);
  }
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        double result;
        input[0] = '1';
        memset (input + 1, '0', m - 9);
        input[m - 8] = 'e';
        input[m - 7] = '-';
        input[m - 6] = '9';
        input[m - 5] = '9';
        input[m - 4] = '9';
        input[m - 3] = '9';
        input[m - 2] = '9';
        input[m - 1] = '1';
        input[m] = '\0';
        errno = 0;
        result = strtod (input, &ptr);
        ASSERT (result == 1.0);          
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);             
      }
    free (input);
  }
#endif
  {
    size_t m = 1000000;
    char *input = malloc (m + 1);
    if (input)
      {
        char *ptr;
        double result;
        input[0] = '-';
        input[1] = '0';
        input[2] = 'e';
        input[3] = '1';
        memset (input + 4, '0', m - 3);
        input[m] = '\0';
        errno = 0;
        result = strtod (input, &ptr);
        ASSERT (result == 0.0);
        ASSERT (!!signbit (result) == !!signbit (minus_zerod));  
        ASSERT (ptr == input + m);
        ASSERT (errno == 0);
      }
    free (input);
  }

   
   

  return status;
}
