 
 

#ifndef __OSDEP_INTF_H_
#define __OSDEP_INTF_H_


struct intf_priv {

	u8 *intf_dev;
	u32 max_iosz;	 
	u32 max_xmitsz;  
	u32 max_recvsz;  

	volatile u8 *io_rwmem;
	volatile u8 *allocated_io_rwmem;
	u32 io_wsz;  
	u32 io_rsz; 
	u8 intf_status;

	void (*_bus_io)(u8 *priv);

 

	struct mutex ioctl_mutex;
};

struct dvobj_priv *devobj_init(void);
void devobj_deinit(struct dvobj_priv *pdvobj);

u8 rtw_init_drv_sw(struct adapter *padapter);
u8 rtw_free_drv_sw(struct adapter *padapter);
void rtw_reset_drv_sw(struct adapter *padapter);
void rtw_dev_unload(struct adapter *padapter);

u32 rtw_start_drv_threads(struct adapter *padapter);
void rtw_stop_drv_threads(struct adapter *padapter);
void rtw_cancel_all_timer(struct adapter *padapter);

int rtw_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

int rtw_init_netdev_name(struct net_device *pnetdev, const char *ifname);
struct net_device *rtw_init_netdev(struct adapter *padapter);
void rtw_unregister_netdevs(struct dvobj_priv *dvobj);

u16 rtw_recv_select_queue(struct sk_buff *skb);

int rtw_ndev_notifier_register(void);
void rtw_ndev_notifier_unregister(void);

void rtw_ips_dev_unload(struct adapter *padapter);

int rtw_ips_pwr_up(struct adapter *padapter);
void rtw_ips_pwr_down(struct adapter *padapter);

int rtw_drv_register_netdev(struct adapter *padapter);
void rtw_ndev_destructor(struct net_device *ndev);

void rtw_suspend_common(struct adapter *padapter);
int rtw_resume_common(struct adapter *padapter);

int netdev_open(struct net_device *pnetdev);

#endif	 
