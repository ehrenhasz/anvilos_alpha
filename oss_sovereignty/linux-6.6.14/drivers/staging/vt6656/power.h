 
 

#ifndef __POWER_H__
#define __POWER_H__

#define C_PWBT	1000  

int vnt_disable_power_saving(struct vnt_private *priv);
void vnt_enable_power_saving(struct vnt_private *priv, u16 listen_interval);
int vnt_next_tbtt_wakeup(struct vnt_private *priv);

#endif  
