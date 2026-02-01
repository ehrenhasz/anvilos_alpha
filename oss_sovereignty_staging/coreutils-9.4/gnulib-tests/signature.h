 
# define SIGNATURE_CHECK(fn, ret, args) \
  SIGNATURE_CHECK1 (fn, ret, args, __LINE__)

 
# define SIGNATURE_CHECK1(fn, ret, args, id) \
  SIGNATURE_CHECK2 (fn, ret, args, id)  
# define SIGNATURE_CHECK2(fn, ret, args, id) \
  _GL_UNUSED static ret (*signature_check ## id) args = fn

#endif  
