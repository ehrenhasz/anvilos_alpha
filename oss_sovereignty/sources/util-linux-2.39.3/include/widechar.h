




#ifdef HAVE_WIDECHAR

# include <wchar.h>
# include <wctype.h>

#else 

# include <ctype.h>
  
# define wchar_t char
# define wint_t int
# ifndef WEOF
#  define WEOF EOF
# endif

 
# define fgetwc fgetc
# define getwc getc
# define getwchar getchar
# define fgetws fgets

  
# define fputwc fputc
# define putwc putc
# define putwchar putchar
# define fputws fputs

  
# define iswgraph isgraph
# define iswprint isprint
# define iswspace isspace

  
# define wcschr strchr
# define wcsdup strdup
# define wcslen strlen
# define wcspbrk strpbrk

# define wcwidth(c) (1)
# define wmemset memset
# define ungetwc ungetc

#endif 
