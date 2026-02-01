 
 

#ifndef _CORE_H_
#define _CORE_H_

#include "dma.h"

 
struct qce_device {
	struct crypto_queue queue;
	spinlock_t lock;
	struct tasklet_struct done_tasklet;
	struct crypto_async_request *req;
	int result;
	void __iomem *base;
	struct device *dev;
	struct clk *core, *iface, *bus;
	struct icc_path *mem_path;
	struct qce_dma_data dma;
	int burst_size;
	unsigned int pipe_pair_id;
	int (*async_req_enqueue)(struct qce_device *qce,
				 struct crypto_async_request *req);
	void (*async_req_done)(struct qce_device *qce, int ret);
};

 
struct qce_algo_ops {
	u32 type;
	int (*register_algs)(struct qce_device *qce);
	void (*unregister_algs)(struct qce_device *qce);
	int (*async_req_handle)(struct crypto_async_request *async_req);
};

#endif  
