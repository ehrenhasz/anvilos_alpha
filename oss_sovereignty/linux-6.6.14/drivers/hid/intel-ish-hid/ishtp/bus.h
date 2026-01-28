#ifndef _LINUX_ISHTP_CL_BUS_H
#define _LINUX_ISHTP_CL_BUS_H
#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/intel-ish-client-if.h>
struct ishtp_cl;
struct ishtp_cl_device;
struct ishtp_device;
struct ishtp_msg_hdr;
struct ishtp_cl_device {
	struct device		dev;
	struct ishtp_device	*ishtp_dev;
	struct ishtp_fw_client	*fw_client;
	struct list_head	device_link;
	struct work_struct	event_work;
	void			*driver_data;
	int			reference_count;
	void (*event_cb)(struct ishtp_cl_device *device);
};
int	ishtp_bus_new_client(struct ishtp_device *dev);
void	ishtp_remove_all_clients(struct ishtp_device *dev);
int	ishtp_cl_device_bind(struct ishtp_cl *cl);
void	ishtp_cl_bus_rx_event(struct ishtp_cl_device *device);
int	ishtp_send_msg(struct ishtp_device *dev,
		       struct ishtp_msg_hdr *hdr, void *msg,
		       void (*ipc_send_compl)(void *),
		       void *ipc_send_compl_prm);
int	ishtp_write_message(struct ishtp_device *dev,
			    struct ishtp_msg_hdr *hdr,
			    void *buf);
int ishtp_use_dma_transfer(void);
void	ishtp_bus_remove_all_clients(struct ishtp_device *ishtp_dev,
				     bool warm_reset);
void	ishtp_recv(struct ishtp_device *dev);
void	ishtp_reset_handler(struct ishtp_device *dev);
void	ishtp_reset_compl_handler(struct ishtp_device *dev);
int	ishtp_fw_cl_by_uuid(struct ishtp_device *dev, const guid_t *cuuid);
#endif  
