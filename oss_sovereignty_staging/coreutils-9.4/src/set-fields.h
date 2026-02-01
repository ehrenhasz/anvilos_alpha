 
extern struct field_range_pair *frp;

 
extern size_t n_frp;

 
enum
{
  SETFLD_ALLOW_DASH = 0x01,      
  SETFLD_COMPLEMENT = 0x02,      
  SETFLD_ERRMSG_USE_POS = 0x04   
};

 
extern void set_fields (char const *fieldstr, unsigned int options);

#endif
