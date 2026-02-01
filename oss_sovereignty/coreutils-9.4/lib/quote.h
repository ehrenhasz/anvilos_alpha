 
extern struct quoting_options quote_quoting_options;

 
char const *quote_n_mem (int n, char const *arg, size_t argsize);

 
char const *quote_mem (char const *arg, size_t argsize);

 
char const *quote_n (int n, char const *arg);

 
char const *quote (char const *arg);

#endif  
