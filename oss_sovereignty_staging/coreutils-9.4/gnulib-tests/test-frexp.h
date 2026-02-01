 
    int exp = -9999;
    DOUBLE mantissa;
    x = NAN;
    mantissa = FREXP (x, &exp);
    ASSERT (ISNAN (mantissa));
  }

  {  
    int exp = -9999;
    DOUBLE mantissa;
    x = INFINITY;
    mantissa = FREXP (x, &exp);
    ASSERT (mantissa == x);
  }

  {  
    int exp = -9999;
    DOUBLE mantissa;
    x = - INFINITY;
    mantissa = FREXP (x, &exp);
    ASSERT (mantissa == x);
  }

  {  
    int exp = -9999;
    DOUBLE mantissa;
    x = L_(0.0);
    mantissa = FREXP (x, &exp);
    ASSERT (exp == 0);
    ASSERT (mantissa == x);
    ASSERT (!signbit (mantissa));
  }

  {  
    int exp = -9999;
    DOUBLE mantissa;
    x = MINUS_ZERO;
    mantissa = FREXP (x, &exp);
    ASSERT (exp == 0);
    ASSERT (mantissa == x);
    ASSERT (signbit (mantissa));
  }

  for (i = 1, x = L_(1.0); i <= MAX_EXP; i++, x *= L_(2.0))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == L_(0.5));
    }
  for (i = 1, x = L_(1.0); i >= MIN_NORMAL_EXP; i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == L_(0.5));
    }
  for (; i >= MIN_EXP - 100 && x > L_(0.0); i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == L_(0.5));
    }

  for (i = 1, x = - L_(1.0); i <= MAX_EXP; i++, x *= L_(2.0))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == - L_(0.5));
    }
  for (i = 1, x = - L_(1.0); i >= MIN_NORMAL_EXP; i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == - L_(0.5));
    }
  for (; i >= MIN_EXP - 100 && x < L_(0.0); i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == - L_(0.5));
    }

  for (i = 1, x = L_(1.01); i <= MAX_EXP; i++, x *= L_(2.0))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == L_(0.505));
    }
  for (i = 1, x = L_(1.01); i >= MIN_NORMAL_EXP; i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == L_(0.505));
    }
  for (; i >= MIN_EXP - 100 && x > L_(0.0); i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa >= L_(0.5));
      ASSERT (mantissa < L_(1.0));
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

  for (i = 1, x = L_(1.73205); i <= MAX_EXP; i++, x *= L_(2.0))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == L_(0.866025));
    }
  for (i = 1, x = L_(1.73205); i >= MIN_NORMAL_EXP; i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i);
      ASSERT (mantissa == L_(0.866025));
    }
  for (; i >= MIN_EXP - 100 && x > L_(0.0); i--, x *= L_(0.5))
    {
      int exp = -9999;
      DOUBLE mantissa = FREXP (x, &exp);
      ASSERT (exp == i || exp == i + 1);
      ASSERT (mantissa >= L_(0.5));
      ASSERT (mantissa < L_(1.0));
      ASSERT (mantissa == my_ldexp (x, - exp));
    }

   
  for (i = 0; i < SIZEOF (RANDOM); i++)
    {
      x = L_(20.0) * RANDOM[i] - L_(10.0);  
      {
        int exp = -9999;
        DOUBLE mantissa = FREXP (x, &exp);
        ASSERT (x == my_ldexp (mantissa, exp));
      }
    }
}
