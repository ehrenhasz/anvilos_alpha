#ifndef	OTX2_DEVLINK_H
#define	OTX2_DEVLINK_H
struct otx2_devlink {
	struct devlink *dl;
	struct otx2_nic *pfvf;
};
int otx2_register_dl(struct otx2_nic *pfvf);
void otx2_unregister_dl(struct otx2_nic *pfvf);
#endif  
