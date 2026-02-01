 
 

#ifndef __IRQ_PUBLIC_H_INCLUDED__
#define __IRQ_PUBLIC_H_INCLUDED__

#include <type_support.h>
#include "system_local.h"

 
void irq_controller_get_state(const irq_ID_t ID,
			      struct irq_controller_state *state);

 
STORAGE_CLASS_IRQ_H void irq_reg_store(
    const irq_ID_t		ID,
    const unsigned int	reg,
    const hrt_data		value);

 
STORAGE_CLASS_IRQ_H hrt_data irq_reg_load(
    const irq_ID_t		ID,
    const unsigned int	reg);

 
void irq_enable_channel(
    const irq_ID_t				ID,
    const unsigned int			irq_ID);

 
void irq_enable_pulse(
    const irq_ID_t	ID,
    bool			pulse);

 
void irq_disable_channel(
    const irq_ID_t				ID,
    const unsigned int			irq);

 
void irq_clear_all(
    const irq_ID_t				ID);

 
enum hrt_isp_css_irq_status irq_get_channel_id(
    const irq_ID_t				ID,
    unsigned int				*irq_id);

 
void irq_raise(
    const irq_ID_t				ID,
    const irq_sw_channel_id_t	irq_id);

 
bool any_virq_signal(void);

 
void cnd_virq_enable_channel(
    const enum virq_id				irq_ID,
    const bool					en);

 
void virq_clear_all(void);

 
void virq_clear_info(struct virq_info *irq_info);

 
enum hrt_isp_css_irq_status virq_get_channel_id(
    enum virq_id					*irq_id);

 
enum hrt_isp_css_irq_status
virq_get_channel_signals(struct virq_info *irq_info);

#endif  
