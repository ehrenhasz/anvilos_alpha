 
 

int	 vasnmprintf(char **, size_t, int *, const char *, va_list);
int	 mprintf(const char *, ...)
	    __attribute__((format(printf, 1, 2)));
int	 fmprintf(FILE *, const char *, ...)
	    __attribute__((format(printf, 2, 3)));
int	 vfmprintf(FILE *, const char *, va_list);
int	 snmprintf(char *, size_t, int *, const char *, ...)
	    __attribute__((format(printf, 4, 5)));
int	 asmprintf(char **, size_t, int *, const char *, ...)
	    __attribute__((format(printf, 4, 5)));
void	 msetlocale(void);
