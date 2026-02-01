 
 

#ifndef _ICE_VF_LIB_PRIVATE_H_
#define _ICE_VF_LIB_PRIVATE_H_

#include "ice_vf_lib.h"

 

#ifndef CONFIG_PCI_IOV
#warning "Only include ice_vf_lib_private.h in CONFIG_PCI_IOV virtualization files"
#endif

void ice_initialize_vf_entry(struct ice_vf *vf);
void ice_dis_vf_qs(struct ice_vf *vf);
int ice_check_vf_init(struct ice_vf *vf);
enum virtchnl_status_code ice_err_to_virt_err(int err);
struct ice_port_info *ice_vf_get_port_info(struct ice_vf *vf);
int ice_vsi_apply_spoofchk(struct ice_vsi *vsi, bool enable);
bool ice_is_vf_trusted(struct ice_vf *vf);
bool ice_vf_has_no_qs_ena(struct ice_vf *vf);
bool ice_is_vf_link_up(struct ice_vf *vf);
void ice_vf_ctrl_invalidate_vsi(struct ice_vf *vf);
void ice_vf_ctrl_vsi_release(struct ice_vf *vf);
struct ice_vsi *ice_vf_ctrl_vsi_setup(struct ice_vf *vf);
int ice_vf_init_host_cfg(struct ice_vf *vf, struct ice_vsi *vsi);
void ice_vf_invalidate_vsi(struct ice_vf *vf);
void ice_vf_vsi_release(struct ice_vf *vf);

#endif  
