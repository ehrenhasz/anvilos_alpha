 
 

#if IS_ENABLED(CONFIG_NVMEM_STM32_BSEC_OPTEE_TA)
 
int stm32_bsec_optee_ta_open(struct tee_context **ctx);

 
void stm32_bsec_optee_ta_close(void *ctx);

 
int stm32_bsec_optee_ta_read(struct tee_context *ctx, unsigned int offset,
			     void *buf, size_t bytes);

 
int stm32_bsec_optee_ta_write(struct tee_context *ctx, unsigned int lower,
			      unsigned int offset, void *buf, size_t bytes);

#else

static inline int stm32_bsec_optee_ta_open(struct tee_context **ctx)
{
	return -EOPNOTSUPP;
}

static inline void stm32_bsec_optee_ta_close(void *ctx)
{
}

static inline int stm32_bsec_optee_ta_read(struct tee_context *ctx,
					   unsigned int offset, void *buf,
					   size_t bytes)
{
	return -EOPNOTSUPP;
}

static inline int stm32_bsec_optee_ta_write(struct tee_context *ctx,
					    unsigned int lower,
					    unsigned int offset, void *buf,
					    size_t bytes)
{
	return -EOPNOTSUPP;
}
#endif  
