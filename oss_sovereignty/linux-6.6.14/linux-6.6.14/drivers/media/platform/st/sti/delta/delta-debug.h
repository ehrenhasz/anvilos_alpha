#ifndef DELTA_DEBUG_H
#define DELTA_DEBUG_H
char *delta_streaminfo_str(struct delta_streaminfo *s, char *str,
			   unsigned int len);
char *delta_frameinfo_str(struct delta_frameinfo *f, char *str,
			  unsigned int len);
void delta_trace_summary(struct delta_ctx *ctx);
#endif  
