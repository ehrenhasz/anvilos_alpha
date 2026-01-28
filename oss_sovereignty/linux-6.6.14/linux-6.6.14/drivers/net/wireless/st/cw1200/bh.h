#ifndef CW1200_BH_H
#define CW1200_BH_H
  struct cw1200_common;
int cw1200_register_bh(struct cw1200_common *priv);
void cw1200_unregister_bh(struct cw1200_common *priv);
void cw1200_irq_handler(struct cw1200_common *priv);
void cw1200_bh_wakeup(struct cw1200_common *priv);
int cw1200_bh_suspend(struct cw1200_common *priv);
int cw1200_bh_resume(struct cw1200_common *priv);
void cw1200_enable_powersave(struct cw1200_common *priv,
			     bool enable);
int wsm_release_tx_buffer(struct cw1200_common *priv, int count);
#endif  
