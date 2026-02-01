 

#ifndef XSTRTOD_H
# define XSTRTOD_H 1

bool xstrtod (const char *str, const char **ptr, double *result,
              double (*convert) (char const *, char **));
bool xstrtold (const char *str, const char **ptr, long double *result,
               long double (*convert) (char const *, char **));

#endif  
