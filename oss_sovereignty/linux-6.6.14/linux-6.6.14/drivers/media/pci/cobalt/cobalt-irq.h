#include <linux/interrupt.h>
irqreturn_t cobalt_irq_handler(int irq, void *dev_id);
void cobalt_irq_work_handler(struct work_struct *work);
void cobalt_irq_log_status(struct cobalt *cobalt);
