#ifndef _IPA_QMI_H_
#define _IPA_QMI_H_
#include <linux/types.h>
#include <linux/soc/qcom/qmi.h>
struct ipa;
struct ipa_qmi {
	struct qmi_handle client_handle;
	struct qmi_handle server_handle;
	struct sockaddr_qrtr modem_sq;
	struct work_struct init_driver_work;
	bool initial_boot;
	bool uc_ready;
	bool modem_ready;
	bool indication_requested;
	bool indication_sent;
};
int ipa_qmi_setup(struct ipa *ipa);
void ipa_qmi_teardown(struct ipa *ipa);
#endif  
