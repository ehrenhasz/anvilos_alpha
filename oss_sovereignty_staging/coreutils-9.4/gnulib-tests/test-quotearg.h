 

struct result_strings {
  char const *str1;  
  char const *str2;  
  size_t len2;  
  char const *str3;  
  char const *str4;  
  char const *str5;  
  char const *str6;  
  char const *str7;  
  char const *str8a;  
  char const *str8b;  
};

struct result_groups {
  struct result_strings group1;  
  struct result_strings group2;  
  struct result_strings group3;  
};

 
# define LQ "\302\253"
# define RQ "\302\273"
# define LQ_ENC "\\302\\253"
# define RQ_ENC "\\302\\273"
# define RQ_ESC "\\\302\273"

static struct result_strings inputs = {
  "", "\0001\0", 3, "simple", " \t\n'\"\033?""?/\\", "a:b", "a\\b",
  "a' b", LQ RQ, NULL
};

static void
compare (char const *a, size_t la, char const *b, size_t lb)
{
  ASSERT (la == lb);
  ASSERT (memcmp (a, b, la) == 0);
  ASSERT (b[lb] == '\0');
}

static void
compare_strings (char *(func) (char const *, size_t *),
                 struct result_strings *results, bool ascii_only)
{
  size_t len;
  char *p;

  len = 0;
  p = func (inputs.str1, &len);
  compare (results->str1, strlen (results->str1), p, len);

  len = inputs.len2;
  p = func (inputs.str2, &len);
  compare (results->str2, results->len2, p, len);

  len = SIZE_MAX;
  p = func (inputs.str3, &len);
  compare (results->str3, strlen (results->str3), p, len);

  len = strlen (inputs.str4);
  p = func (inputs.str4, &len);
  compare (results->str4, strlen (results->str4), p, len);

  len = SIZE_MAX;
  p = func (inputs.str5, &len);
  compare (results->str5, strlen (results->str5), p, len);

  len = strlen (inputs.str6);
  p = func (inputs.str6, &len);
  compare (results->str6, strlen (results->str6), p, len);

  len = strlen (inputs.str7);
  p = func (inputs.str7, &len);
  compare (results->str7, strlen (results->str7), p, len);

  len = strlen (inputs.str8a);
  p = func (inputs.str8a, &len);
  if (ascii_only)
    compare (results->str8a, strlen (results->str8a), p, len);
  else
    compare (results->str8b, strlen (results->str8b), p, len);
}

static char *
use_quotearg_buffer (const char *str, size_t *len)
{
  static char buf[100];
  size_t size;
  memset (buf, 0xa5, 100);
  size = quotearg_buffer (buf, 100, str, *len, NULL);
  *len = size;
  ASSERT ((unsigned char) buf[size + 1] == 0xa5);
  return buf;
}

static char *
use_quotearg (const char *str, size_t *len)
{
  char *p = *len == SIZE_MAX ? quotearg (str) : quotearg_mem (str, *len);
  *len = strlen (p);
  return p;
}

static char *
use_quotearg_colon (const char *str, size_t *len)
{
  char *p = (*len == SIZE_MAX ? quotearg_colon (str)
             : quotearg_colon_mem (str, *len));
  *len = strlen (p);
  return p;
}
