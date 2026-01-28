




#ifndef __REQUEST_MGR_H__
#define __REQUEST_MGR_H__

#include "cc_hw_queue_defs.h"

int cc_req_mgr_init(struct cc_drvdata *drvdata);


int cc_send_request(struct cc_drvdata *drvdata, struct cc_crypto_req *cc_req,
		    struct cc_hw_desc *desc, unsigned int len,
		    struct crypto_async_request *req);

int cc_send_sync_request(struct cc_drvdata *drvdata,
			 struct cc_crypto_req *cc_req, struct cc_hw_desc *desc,
			 unsigned int len);

int send_request_init(struct cc_drvdata *drvdata, struct cc_hw_desc *desc,
		      unsigned int len);

void complete_request(struct cc_drvdata *drvdata);

void cc_req_mgr_fini(struct cc_drvdata *drvdata);

#endif 
