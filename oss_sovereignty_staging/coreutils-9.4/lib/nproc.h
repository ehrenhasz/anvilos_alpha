 

 
#ifdef __cplusplus
extern "C" {
#endif

 

enum nproc_query
{
  NPROC_ALL,                  
  NPROC_CURRENT,              
  NPROC_CURRENT_OVERRIDABLE   
};

 
extern unsigned long int num_processors (enum nproc_query query);

#ifdef __cplusplus
}
#endif  
