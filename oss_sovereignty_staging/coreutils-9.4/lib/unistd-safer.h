 

int dup_safer (int);
int fd_safer (int);
int pipe_safer (int[2]);

#if GNULIB_FD_SAFER_FLAG
int dup_safer_flag (int, int);
int fd_safer_flag (int, int);
#endif

#if GNULIB_PIPE2_SAFER
int pipe2_safer (int[2], int);
#endif
