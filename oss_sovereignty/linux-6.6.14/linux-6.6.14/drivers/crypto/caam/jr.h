#ifndef JR_H
#define JR_H
struct device *caam_jr_alloc(void);
void caam_jr_free(struct device *rdev);
int caam_jr_enqueue(struct device *dev, u32 *desc,
		    void (*cbk)(struct device *dev, u32 *desc, u32 status,
				void *areq),
		    void *areq);
#endif  
