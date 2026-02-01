 

struct linebuffer
{
  idx_t size;                   
  idx_t length;                 
  char *buffer;
};

 
void initbuffer (struct linebuffer *linebuffer);

 
struct linebuffer *readlinebuffer_delim (struct linebuffer *linebuffer,
                                         FILE *stream, char delimiter);

 
struct linebuffer *readlinebuffer (struct linebuffer *linebuffer, FILE *stream);

 
void freebuffer (struct linebuffer *);

#endif  
