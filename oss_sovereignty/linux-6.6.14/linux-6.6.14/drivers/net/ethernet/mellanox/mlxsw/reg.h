#ifndef _MLXSW_REG_H
#define _MLXSW_REG_H
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/if_vlan.h>
#include "item.h"
#include "port.h"
struct mlxsw_reg_info {
	u16 id;
	u16 len;  
	const char *name;
};
#define MLXSW_REG_DEFINE(_name, _id, _len)				\
static const struct mlxsw_reg_info mlxsw_reg_##_name = {		\
	.id = _id,							\
	.len = _len,							\
	.name = #_name,							\
}
#define MLXSW_REG(type) (&mlxsw_reg_##type)
#define MLXSW_REG_LEN(type) MLXSW_REG(type)->len
#define MLXSW_REG_ZERO(type, payload) memset(payload, 0, MLXSW_REG(type)->len)
#define MLXSW_REG_SGCR_ID 0x2000
#define MLXSW_REG_SGCR_LEN 0x10
MLXSW_REG_DEFINE(sgcr, MLXSW_REG_SGCR_ID, MLXSW_REG_SGCR_LEN);
MLXSW_ITEM32(reg, sgcr, llb, 0x04, 0, 1);
static inline void mlxsw_reg_sgcr_pack(char *payload, bool llb)
{
	MLXSW_REG_ZERO(sgcr, payload);
	mlxsw_reg_sgcr_llb_set(payload, !!llb);
}
#define MLXSW_REG_SPAD_ID 0x2002
#define MLXSW_REG_SPAD_LEN 0x10
MLXSW_REG_DEFINE(spad, MLXSW_REG_SPAD_ID, MLXSW_REG_SPAD_LEN);
MLXSW_ITEM_BUF(reg, spad, base_mac, 0x02, 6);
#define MLXSW_REG_SSPR_ID 0x2008
#define MLXSW_REG_SSPR_LEN 0x8
MLXSW_REG_DEFINE(sspr, MLXSW_REG_SSPR_ID, MLXSW_REG_SSPR_LEN);
MLXSW_ITEM32(reg, sspr, m, 0x00, 31, 1);
MLXSW_ITEM32_LP(reg, sspr, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, sspr, system_port, 0x04, 0, 16);
static inline void mlxsw_reg_sspr_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(sspr, payload);
	mlxsw_reg_sspr_m_set(payload, 1);
	mlxsw_reg_sspr_local_port_set(payload, local_port);
	mlxsw_reg_sspr_system_port_set(payload, local_port);
}
#define MLXSW_REG_SFDAT_ID 0x2009
#define MLXSW_REG_SFDAT_LEN 0x8
MLXSW_REG_DEFINE(sfdat, MLXSW_REG_SFDAT_ID, MLXSW_REG_SFDAT_LEN);
MLXSW_ITEM32(reg, sfdat, swid, 0x00, 24, 8);
MLXSW_ITEM32(reg, sfdat, age_time, 0x04, 0, 20);
static inline void mlxsw_reg_sfdat_pack(char *payload, u32 age_time)
{
	MLXSW_REG_ZERO(sfdat, payload);
	mlxsw_reg_sfdat_swid_set(payload, 0);
	mlxsw_reg_sfdat_age_time_set(payload, age_time);
}
#define MLXSW_REG_SFD_ID 0x200A
#define MLXSW_REG_SFD_BASE_LEN 0x10  
#define MLXSW_REG_SFD_REC_LEN 0x10  
#define MLXSW_REG_SFD_REC_MAX_COUNT 64
#define MLXSW_REG_SFD_LEN (MLXSW_REG_SFD_BASE_LEN +	\
			   MLXSW_REG_SFD_REC_LEN * MLXSW_REG_SFD_REC_MAX_COUNT)
MLXSW_REG_DEFINE(sfd, MLXSW_REG_SFD_ID, MLXSW_REG_SFD_LEN);
MLXSW_ITEM32(reg, sfd, swid, 0x00, 24, 8);
enum mlxsw_reg_sfd_op {
	MLXSW_REG_SFD_OP_QUERY_DUMP = 0,
	MLXSW_REG_SFD_OP_QUERY_QUERY = 1,
	MLXSW_REG_SFD_OP_QUERY_QUERY_AND_CLEAR_ACTIVITY = 2,
	MLXSW_REG_SFD_OP_WRITE_TEST = 0,
	MLXSW_REG_SFD_OP_WRITE_EDIT = 1,
	MLXSW_REG_SFD_OP_WRITE_REMOVE = 2,
	MLXSW_REG_SFD_OP_WRITE_REMOVE_NOTIFICATION = 2,
};
MLXSW_ITEM32(reg, sfd, op, 0x04, 30, 2);
MLXSW_ITEM32(reg, sfd, record_locator, 0x04, 0, 30);
MLXSW_ITEM32(reg, sfd, num_rec, 0x08, 0, 8);
static inline void mlxsw_reg_sfd_pack(char *payload, enum mlxsw_reg_sfd_op op,
				      u32 record_locator)
{
	MLXSW_REG_ZERO(sfd, payload);
	mlxsw_reg_sfd_op_set(payload, op);
	mlxsw_reg_sfd_record_locator_set(payload, record_locator);
}
MLXSW_ITEM32_INDEXED(reg, sfd, rec_swid, MLXSW_REG_SFD_BASE_LEN, 24, 8,
		     MLXSW_REG_SFD_REC_LEN, 0x00, false);
enum mlxsw_reg_sfd_rec_type {
	MLXSW_REG_SFD_REC_TYPE_UNICAST = 0x0,
	MLXSW_REG_SFD_REC_TYPE_UNICAST_LAG = 0x1,
	MLXSW_REG_SFD_REC_TYPE_MULTICAST = 0x2,
	MLXSW_REG_SFD_REC_TYPE_UNICAST_TUNNEL = 0xC,
};
MLXSW_ITEM32_INDEXED(reg, sfd, rec_type, MLXSW_REG_SFD_BASE_LEN, 20, 4,
		     MLXSW_REG_SFD_REC_LEN, 0x00, false);
enum mlxsw_reg_sfd_rec_policy {
	MLXSW_REG_SFD_REC_POLICY_STATIC_ENTRY = 0,
	MLXSW_REG_SFD_REC_POLICY_DYNAMIC_ENTRY_MLAG = 1,
	MLXSW_REG_SFD_REC_POLICY_DYNAMIC_ENTRY_INGRESS = 3,
};
MLXSW_ITEM32_INDEXED(reg, sfd, rec_policy, MLXSW_REG_SFD_BASE_LEN, 18, 2,
		     MLXSW_REG_SFD_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, sfd, rec_a, MLXSW_REG_SFD_BASE_LEN, 16, 1,
		     MLXSW_REG_SFD_REC_LEN, 0x00, false);
MLXSW_ITEM_BUF_INDEXED(reg, sfd, rec_mac, MLXSW_REG_SFD_BASE_LEN, 6,
		       MLXSW_REG_SFD_REC_LEN, 0x02);
enum mlxsw_reg_sfd_rec_action {
	MLXSW_REG_SFD_REC_ACTION_NOP = 0,
	MLXSW_REG_SFD_REC_ACTION_MIRROR_TO_CPU = 1,
	MLXSW_REG_SFD_REC_ACTION_TRAP = 2,
	MLXSW_REG_SFD_REC_ACTION_FORWARD_IP_ROUTER = 3,
	MLXSW_REG_SFD_REC_ACTION_DISCARD_ERROR = 15,
};
MLXSW_ITEM32_INDEXED(reg, sfd, rec_action, MLXSW_REG_SFD_BASE_LEN, 28, 4,
		     MLXSW_REG_SFD_REC_LEN, 0x0C, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_sub_port, MLXSW_REG_SFD_BASE_LEN, 16, 8,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_set_vid, MLXSW_REG_SFD_BASE_LEN, 31, 1,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_fid_vid, MLXSW_REG_SFD_BASE_LEN, 0, 16,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_vid, MLXSW_REG_SFD_BASE_LEN, 16, 12,
		     MLXSW_REG_SFD_REC_LEN, 0x0C, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_system_port, MLXSW_REG_SFD_BASE_LEN, 0, 16,
		     MLXSW_REG_SFD_REC_LEN, 0x0C, false);
static inline void mlxsw_reg_sfd_rec_pack(char *payload, int rec_index,
					  enum mlxsw_reg_sfd_rec_type rec_type,
					  const char *mac,
					  enum mlxsw_reg_sfd_rec_action action)
{
	u8 num_rec = mlxsw_reg_sfd_num_rec_get(payload);
	if (rec_index >= num_rec)
		mlxsw_reg_sfd_num_rec_set(payload, rec_index + 1);
	mlxsw_reg_sfd_rec_swid_set(payload, rec_index, 0);
	mlxsw_reg_sfd_rec_type_set(payload, rec_index, rec_type);
	mlxsw_reg_sfd_rec_mac_memcpy_to(payload, rec_index, mac);
	mlxsw_reg_sfd_rec_action_set(payload, rec_index, action);
}
static inline void mlxsw_reg_sfd_uc_pack(char *payload, int rec_index,
					 enum mlxsw_reg_sfd_rec_policy policy,
					 const char *mac, u16 fid_vid, u16 vid,
					 enum mlxsw_reg_sfd_rec_action action,
					 u16 local_port)
{
	mlxsw_reg_sfd_rec_pack(payload, rec_index,
			       MLXSW_REG_SFD_REC_TYPE_UNICAST, mac, action);
	mlxsw_reg_sfd_rec_policy_set(payload, rec_index, policy);
	mlxsw_reg_sfd_uc_sub_port_set(payload, rec_index, 0);
	mlxsw_reg_sfd_uc_fid_vid_set(payload, rec_index, fid_vid);
	mlxsw_reg_sfd_uc_set_vid_set(payload, rec_index, vid ? true : false);
	mlxsw_reg_sfd_uc_vid_set(payload, rec_index, vid);
	mlxsw_reg_sfd_uc_system_port_set(payload, rec_index, local_port);
}
MLXSW_ITEM32_INDEXED(reg, sfd, uc_lag_sub_port, MLXSW_REG_SFD_BASE_LEN, 16, 8,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_lag_set_vid, MLXSW_REG_SFD_BASE_LEN, 31, 1,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_lag_fid_vid, MLXSW_REG_SFD_BASE_LEN, 0, 16,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_lag_lag_vid, MLXSW_REG_SFD_BASE_LEN, 16, 12,
		     MLXSW_REG_SFD_REC_LEN, 0x0C, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_lag_lag_id, MLXSW_REG_SFD_BASE_LEN, 0, 10,
		     MLXSW_REG_SFD_REC_LEN, 0x0C, false);
static inline void
mlxsw_reg_sfd_uc_lag_pack(char *payload, int rec_index,
			  enum mlxsw_reg_sfd_rec_policy policy,
			  const char *mac, u16 fid_vid,
			  enum mlxsw_reg_sfd_rec_action action, u16 lag_vid,
			  u16 lag_id)
{
	mlxsw_reg_sfd_rec_pack(payload, rec_index,
			       MLXSW_REG_SFD_REC_TYPE_UNICAST_LAG,
			       mac, action);
	mlxsw_reg_sfd_rec_policy_set(payload, rec_index, policy);
	mlxsw_reg_sfd_uc_lag_sub_port_set(payload, rec_index, 0);
	mlxsw_reg_sfd_uc_lag_fid_vid_set(payload, rec_index, fid_vid);
	mlxsw_reg_sfd_uc_lag_set_vid_set(payload, rec_index, true);
	mlxsw_reg_sfd_uc_lag_lag_vid_set(payload, rec_index, lag_vid);
	mlxsw_reg_sfd_uc_lag_lag_id_set(payload, rec_index, lag_id);
}
MLXSW_ITEM32_INDEXED(reg, sfd, mc_pgi, MLXSW_REG_SFD_BASE_LEN, 16, 13,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, mc_fid_vid, MLXSW_REG_SFD_BASE_LEN, 0, 16,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, mc_mid, MLXSW_REG_SFD_BASE_LEN, 0, 16,
		     MLXSW_REG_SFD_REC_LEN, 0x0C, false);
static inline void
mlxsw_reg_sfd_mc_pack(char *payload, int rec_index,
		      const char *mac, u16 fid_vid,
		      enum mlxsw_reg_sfd_rec_action action, u16 mid)
{
	mlxsw_reg_sfd_rec_pack(payload, rec_index,
			       MLXSW_REG_SFD_REC_TYPE_MULTICAST, mac, action);
	mlxsw_reg_sfd_mc_pgi_set(payload, rec_index, 0x1FFF);
	mlxsw_reg_sfd_mc_fid_vid_set(payload, rec_index, fid_vid);
	mlxsw_reg_sfd_mc_mid_set(payload, rec_index, mid);
}
MLXSW_ITEM32_INDEXED(reg, sfd, uc_tunnel_uip_msb, MLXSW_REG_SFD_BASE_LEN, 24,
		     8, MLXSW_REG_SFD_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_tunnel_fid, MLXSW_REG_SFD_BASE_LEN, 0, 16,
		     MLXSW_REG_SFD_REC_LEN, 0x08, false);
enum mlxsw_reg_sfd_uc_tunnel_protocol {
	MLXSW_REG_SFD_UC_TUNNEL_PROTOCOL_IPV4,
	MLXSW_REG_SFD_UC_TUNNEL_PROTOCOL_IPV6,
};
MLXSW_ITEM32_INDEXED(reg, sfd, uc_tunnel_protocol, MLXSW_REG_SFD_BASE_LEN, 27,
		     1, MLXSW_REG_SFD_REC_LEN, 0x0C, false);
MLXSW_ITEM32_INDEXED(reg, sfd, uc_tunnel_uip_lsb, MLXSW_REG_SFD_BASE_LEN, 0,
		     24, MLXSW_REG_SFD_REC_LEN, 0x0C, false);
static inline void
mlxsw_reg_sfd_uc_tunnel_pack(char *payload, int rec_index,
			     enum mlxsw_reg_sfd_rec_policy policy,
			     const char *mac, u16 fid,
			     enum mlxsw_reg_sfd_rec_action action,
			     enum mlxsw_reg_sfd_uc_tunnel_protocol proto)
{
	mlxsw_reg_sfd_rec_pack(payload, rec_index,
			       MLXSW_REG_SFD_REC_TYPE_UNICAST_TUNNEL, mac,
			       action);
	mlxsw_reg_sfd_rec_policy_set(payload, rec_index, policy);
	mlxsw_reg_sfd_uc_tunnel_fid_set(payload, rec_index, fid);
	mlxsw_reg_sfd_uc_tunnel_protocol_set(payload, rec_index, proto);
}
static inline void
mlxsw_reg_sfd_uc_tunnel_pack4(char *payload, int rec_index,
			      enum mlxsw_reg_sfd_rec_policy policy,
			      const char *mac, u16 fid,
			      enum mlxsw_reg_sfd_rec_action action, u32 uip)
{
	mlxsw_reg_sfd_uc_tunnel_uip_msb_set(payload, rec_index, uip >> 24);
	mlxsw_reg_sfd_uc_tunnel_uip_lsb_set(payload, rec_index, uip);
	mlxsw_reg_sfd_uc_tunnel_pack(payload, rec_index, policy, mac, fid,
				     action,
				     MLXSW_REG_SFD_UC_TUNNEL_PROTOCOL_IPV4);
}
static inline void
mlxsw_reg_sfd_uc_tunnel_pack6(char *payload, int rec_index, const char *mac,
			      u16 fid, enum mlxsw_reg_sfd_rec_action action,
			      u32 uip_ptr)
{
	mlxsw_reg_sfd_uc_tunnel_uip_lsb_set(payload, rec_index, uip_ptr);
	mlxsw_reg_sfd_uc_tunnel_pack(payload, rec_index,
				     MLXSW_REG_SFD_REC_POLICY_STATIC_ENTRY,
				     mac, fid, action,
				     MLXSW_REG_SFD_UC_TUNNEL_PROTOCOL_IPV6);
}
enum mlxsw_reg_tunnel_port {
	MLXSW_REG_TUNNEL_PORT_NVE,
	MLXSW_REG_TUNNEL_PORT_VPLS,
	MLXSW_REG_TUNNEL_PORT_FLEX_TUNNEL0,
	MLXSW_REG_TUNNEL_PORT_FLEX_TUNNEL1,
};
#define MLXSW_REG_SFN_ID 0x200B
#define MLXSW_REG_SFN_BASE_LEN 0x10  
#define MLXSW_REG_SFN_REC_LEN 0x10  
#define MLXSW_REG_SFN_REC_MAX_COUNT 64
#define MLXSW_REG_SFN_LEN (MLXSW_REG_SFN_BASE_LEN +	\
			   MLXSW_REG_SFN_REC_LEN * MLXSW_REG_SFN_REC_MAX_COUNT)
MLXSW_REG_DEFINE(sfn, MLXSW_REG_SFN_ID, MLXSW_REG_SFN_LEN);
MLXSW_ITEM32(reg, sfn, swid, 0x00, 24, 8);
MLXSW_ITEM32(reg, sfn, end, 0x04, 20, 1);
MLXSW_ITEM32(reg, sfn, num_rec, 0x04, 0, 8);
static inline void mlxsw_reg_sfn_pack(char *payload)
{
	MLXSW_REG_ZERO(sfn, payload);
	mlxsw_reg_sfn_swid_set(payload, 0);
	mlxsw_reg_sfn_end_set(payload, 0);
	mlxsw_reg_sfn_num_rec_set(payload, MLXSW_REG_SFN_REC_MAX_COUNT);
}
MLXSW_ITEM32_INDEXED(reg, sfn, rec_swid, MLXSW_REG_SFN_BASE_LEN, 24, 8,
		     MLXSW_REG_SFN_REC_LEN, 0x00, false);
enum mlxsw_reg_sfn_rec_type {
	MLXSW_REG_SFN_REC_TYPE_LEARNED_MAC = 0x5,
	MLXSW_REG_SFN_REC_TYPE_LEARNED_MAC_LAG = 0x6,
	MLXSW_REG_SFN_REC_TYPE_AGED_OUT_MAC = 0x7,
	MLXSW_REG_SFN_REC_TYPE_AGED_OUT_MAC_LAG = 0x8,
	MLXSW_REG_SFN_REC_TYPE_LEARNED_UNICAST_TUNNEL = 0xD,
	MLXSW_REG_SFN_REC_TYPE_AGED_OUT_UNICAST_TUNNEL = 0xE,
};
MLXSW_ITEM32_INDEXED(reg, sfn, rec_type, MLXSW_REG_SFN_BASE_LEN, 20, 4,
		     MLXSW_REG_SFN_REC_LEN, 0x00, false);
MLXSW_ITEM_BUF_INDEXED(reg, sfn, rec_mac, MLXSW_REG_SFN_BASE_LEN, 6,
		       MLXSW_REG_SFN_REC_LEN, 0x02);
MLXSW_ITEM32_INDEXED(reg, sfn, mac_sub_port, MLXSW_REG_SFN_BASE_LEN, 16, 8,
		     MLXSW_REG_SFN_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfn, mac_fid, MLXSW_REG_SFN_BASE_LEN, 0, 16,
		     MLXSW_REG_SFN_REC_LEN, 0x08, false);
MLXSW_ITEM32_INDEXED(reg, sfn, mac_system_port, MLXSW_REG_SFN_BASE_LEN, 0, 16,
		     MLXSW_REG_SFN_REC_LEN, 0x0C, false);
static inline void mlxsw_reg_sfn_mac_unpack(char *payload, int rec_index,
					    char *mac, u16 *p_vid,
					    u16 *p_local_port)
{
	mlxsw_reg_sfn_rec_mac_memcpy_from(payload, rec_index, mac);
	*p_vid = mlxsw_reg_sfn_mac_fid_get(payload, rec_index);
	*p_local_port = mlxsw_reg_sfn_mac_system_port_get(payload, rec_index);
}
MLXSW_ITEM32_INDEXED(reg, sfn, mac_lag_lag_id, MLXSW_REG_SFN_BASE_LEN, 0, 10,
		     MLXSW_REG_SFN_REC_LEN, 0x0C, false);
static inline void mlxsw_reg_sfn_mac_lag_unpack(char *payload, int rec_index,
						char *mac, u16 *p_vid,
						u16 *p_lag_id)
{
	mlxsw_reg_sfn_rec_mac_memcpy_from(payload, rec_index, mac);
	*p_vid = mlxsw_reg_sfn_mac_fid_get(payload, rec_index);
	*p_lag_id = mlxsw_reg_sfn_mac_lag_lag_id_get(payload, rec_index);
}
MLXSW_ITEM32_INDEXED(reg, sfn, uc_tunnel_uip_msb, MLXSW_REG_SFN_BASE_LEN, 24,
		     8, MLXSW_REG_SFN_REC_LEN, 0x08, false);
enum mlxsw_reg_sfn_uc_tunnel_protocol {
	MLXSW_REG_SFN_UC_TUNNEL_PROTOCOL_IPV4,
	MLXSW_REG_SFN_UC_TUNNEL_PROTOCOL_IPV6,
};
MLXSW_ITEM32_INDEXED(reg, sfn, uc_tunnel_protocol, MLXSW_REG_SFN_BASE_LEN, 27,
		     1, MLXSW_REG_SFN_REC_LEN, 0x0C, false);
MLXSW_ITEM32_INDEXED(reg, sfn, uc_tunnel_uip_lsb, MLXSW_REG_SFN_BASE_LEN, 0,
		     24, MLXSW_REG_SFN_REC_LEN, 0x0C, false);
MLXSW_ITEM32_INDEXED(reg, sfn, tunnel_port, MLXSW_REG_SFN_BASE_LEN, 0, 4,
		     MLXSW_REG_SFN_REC_LEN, 0x10, false);
static inline void
mlxsw_reg_sfn_uc_tunnel_unpack(char *payload, int rec_index, char *mac,
			       u16 *p_fid, u32 *p_uip,
			       enum mlxsw_reg_sfn_uc_tunnel_protocol *p_proto)
{
	u32 uip_msb, uip_lsb;
	mlxsw_reg_sfn_rec_mac_memcpy_from(payload, rec_index, mac);
	*p_fid = mlxsw_reg_sfn_mac_fid_get(payload, rec_index);
	uip_msb = mlxsw_reg_sfn_uc_tunnel_uip_msb_get(payload, rec_index);
	uip_lsb = mlxsw_reg_sfn_uc_tunnel_uip_lsb_get(payload, rec_index);
	*p_uip = uip_msb << 24 | uip_lsb;
	*p_proto = mlxsw_reg_sfn_uc_tunnel_protocol_get(payload, rec_index);
}
#define MLXSW_REG_SPMS_ID 0x200D
#define MLXSW_REG_SPMS_LEN 0x404
MLXSW_REG_DEFINE(spms, MLXSW_REG_SPMS_ID, MLXSW_REG_SPMS_LEN);
MLXSW_ITEM32_LP(reg, spms, 0x00, 16, 0x00, 12);
enum mlxsw_reg_spms_state {
	MLXSW_REG_SPMS_STATE_NO_CHANGE,
	MLXSW_REG_SPMS_STATE_DISCARDING,
	MLXSW_REG_SPMS_STATE_LEARNING,
	MLXSW_REG_SPMS_STATE_FORWARDING,
};
MLXSW_ITEM_BIT_ARRAY(reg, spms, state, 0x04, 0x400, 2);
static inline void mlxsw_reg_spms_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(spms, payload);
	mlxsw_reg_spms_local_port_set(payload, local_port);
}
static inline void mlxsw_reg_spms_vid_pack(char *payload, u16 vid,
					   enum mlxsw_reg_spms_state state)
{
	mlxsw_reg_spms_state_set(payload, vid, state);
}
#define MLXSW_REG_SPVID_ID 0x200E
#define MLXSW_REG_SPVID_LEN 0x08
MLXSW_REG_DEFINE(spvid, MLXSW_REG_SPVID_ID, MLXSW_REG_SPVID_LEN);
MLXSW_ITEM32(reg, spvid, tport, 0x00, 24, 1);
MLXSW_ITEM32_LP(reg, spvid, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spvid, sub_port, 0x00, 8, 8);
MLXSW_ITEM32(reg, spvid, egr_et_set, 0x04, 24, 1);
MLXSW_ITEM32(reg, spvid, et_vlan, 0x04, 16, 2);
MLXSW_ITEM32(reg, spvid, pvid, 0x04, 0, 12);
static inline void mlxsw_reg_spvid_pack(char *payload, u16 local_port, u16 pvid,
					u8 et_vlan)
{
	MLXSW_REG_ZERO(spvid, payload);
	mlxsw_reg_spvid_local_port_set(payload, local_port);
	mlxsw_reg_spvid_pvid_set(payload, pvid);
	mlxsw_reg_spvid_et_vlan_set(payload, et_vlan);
}
#define MLXSW_REG_SPVM_ID 0x200F
#define MLXSW_REG_SPVM_BASE_LEN 0x04  
#define MLXSW_REG_SPVM_REC_LEN 0x04  
#define MLXSW_REG_SPVM_REC_MAX_COUNT 255
#define MLXSW_REG_SPVM_LEN (MLXSW_REG_SPVM_BASE_LEN +	\
		    MLXSW_REG_SPVM_REC_LEN * MLXSW_REG_SPVM_REC_MAX_COUNT)
MLXSW_REG_DEFINE(spvm, MLXSW_REG_SPVM_ID, MLXSW_REG_SPVM_LEN);
MLXSW_ITEM32(reg, spvm, pt, 0x00, 31, 1);
MLXSW_ITEM32(reg, spvm, pte, 0x00, 30, 1);
MLXSW_ITEM32_LP(reg, spvm, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spvm, sub_port, 0x00, 8, 8);
MLXSW_ITEM32(reg, spvm, num_rec, 0x00, 0, 8);
MLXSW_ITEM32_INDEXED(reg, spvm, rec_i,
		     MLXSW_REG_SPVM_BASE_LEN, 14, 1,
		     MLXSW_REG_SPVM_REC_LEN, 0, false);
MLXSW_ITEM32_INDEXED(reg, spvm, rec_e,
		     MLXSW_REG_SPVM_BASE_LEN, 13, 1,
		     MLXSW_REG_SPVM_REC_LEN, 0, false);
MLXSW_ITEM32_INDEXED(reg, spvm, rec_u,
		     MLXSW_REG_SPVM_BASE_LEN, 12, 1,
		     MLXSW_REG_SPVM_REC_LEN, 0, false);
MLXSW_ITEM32_INDEXED(reg, spvm, rec_vid,
		     MLXSW_REG_SPVM_BASE_LEN, 0, 12,
		     MLXSW_REG_SPVM_REC_LEN, 0, false);
static inline void mlxsw_reg_spvm_pack(char *payload, u16 local_port,
				       u16 vid_begin, u16 vid_end,
				       bool is_member, bool untagged)
{
	int size = vid_end - vid_begin + 1;
	int i;
	MLXSW_REG_ZERO(spvm, payload);
	mlxsw_reg_spvm_local_port_set(payload, local_port);
	mlxsw_reg_spvm_num_rec_set(payload, size);
	for (i = 0; i < size; i++) {
		mlxsw_reg_spvm_rec_i_set(payload, i, is_member);
		mlxsw_reg_spvm_rec_e_set(payload, i, is_member);
		mlxsw_reg_spvm_rec_u_set(payload, i, untagged);
		mlxsw_reg_spvm_rec_vid_set(payload, i, vid_begin + i);
	}
}
#define MLXSW_REG_SPAFT_ID 0x2010
#define MLXSW_REG_SPAFT_LEN 0x08
MLXSW_REG_DEFINE(spaft, MLXSW_REG_SPAFT_ID, MLXSW_REG_SPAFT_LEN);
MLXSW_ITEM32_LP(reg, spaft, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spaft, sub_port, 0x00, 8, 8);
MLXSW_ITEM32(reg, spaft, allow_untagged, 0x04, 31, 1);
MLXSW_ITEM32(reg, spaft, allow_prio_tagged, 0x04, 30, 1);
MLXSW_ITEM32(reg, spaft, allow_tagged, 0x04, 29, 1);
static inline void mlxsw_reg_spaft_pack(char *payload, u16 local_port,
					bool allow_untagged)
{
	MLXSW_REG_ZERO(spaft, payload);
	mlxsw_reg_spaft_local_port_set(payload, local_port);
	mlxsw_reg_spaft_allow_untagged_set(payload, allow_untagged);
	mlxsw_reg_spaft_allow_prio_tagged_set(payload, allow_untagged);
	mlxsw_reg_spaft_allow_tagged_set(payload, true);
}
#define MLXSW_REG_SFGC_ID 0x2011
#define MLXSW_REG_SFGC_LEN 0x14
MLXSW_REG_DEFINE(sfgc, MLXSW_REG_SFGC_ID, MLXSW_REG_SFGC_LEN);
enum mlxsw_reg_sfgc_type {
	MLXSW_REG_SFGC_TYPE_BROADCAST,
	MLXSW_REG_SFGC_TYPE_UNKNOWN_UNICAST,
	MLXSW_REG_SFGC_TYPE_UNREGISTERED_MULTICAST_IPV4,
	MLXSW_REG_SFGC_TYPE_UNREGISTERED_MULTICAST_IPV6,
	MLXSW_REG_SFGC_TYPE_RESERVED,
	MLXSW_REG_SFGC_TYPE_UNREGISTERED_MULTICAST_NON_IP,
	MLXSW_REG_SFGC_TYPE_IPV4_LINK_LOCAL,
	MLXSW_REG_SFGC_TYPE_IPV6_ALL_HOST,
	MLXSW_REG_SFGC_TYPE_MAX,
};
MLXSW_ITEM32(reg, sfgc, type, 0x00, 0, 4);
enum mlxsw_reg_bridge_type {
	MLXSW_REG_BRIDGE_TYPE_0 = 0,  
	MLXSW_REG_BRIDGE_TYPE_1 = 1,  
};
MLXSW_ITEM32(reg, sfgc, bridge_type, 0x04, 24, 3);
enum mlxsw_flood_table_type {
	MLXSW_REG_SFGC_TABLE_TYPE_VID = 1,
	MLXSW_REG_SFGC_TABLE_TYPE_SINGLE = 2,
	MLXSW_REG_SFGC_TABLE_TYPE_ANY = 0,
	MLXSW_REG_SFGC_TABLE_TYPE_FID_OFFSET = 3,
	MLXSW_REG_SFGC_TABLE_TYPE_FID = 4,
};
MLXSW_ITEM32(reg, sfgc, table_type, 0x04, 16, 3);
MLXSW_ITEM32(reg, sfgc, flood_table, 0x04, 0, 6);
MLXSW_ITEM32(reg, sfgc, counter_set_type, 0x0C, 24, 8);
MLXSW_ITEM32(reg, sfgc, counter_index, 0x0C, 0, 24);
MLXSW_ITEM32(reg, sfgc, mid_base, 0x10, 0, 16);
static inline void
mlxsw_reg_sfgc_pack(char *payload, enum mlxsw_reg_sfgc_type type,
		    enum mlxsw_reg_bridge_type bridge_type,
		    enum mlxsw_flood_table_type table_type,
		    unsigned int flood_table, u16 mid_base)
{
	MLXSW_REG_ZERO(sfgc, payload);
	mlxsw_reg_sfgc_type_set(payload, type);
	mlxsw_reg_sfgc_bridge_type_set(payload, bridge_type);
	mlxsw_reg_sfgc_table_type_set(payload, table_type);
	mlxsw_reg_sfgc_flood_table_set(payload, flood_table);
	mlxsw_reg_sfgc_mid_base_set(payload, mid_base);
}
#define MLXSW_REG_SFDF_ID 0x2013
#define MLXSW_REG_SFDF_LEN 0x14
MLXSW_REG_DEFINE(sfdf, MLXSW_REG_SFDF_ID, MLXSW_REG_SFDF_LEN);
MLXSW_ITEM32(reg, sfdf, swid, 0x00, 24, 8);
enum mlxsw_reg_sfdf_flush_type {
	MLXSW_REG_SFDF_FLUSH_PER_SWID,
	MLXSW_REG_SFDF_FLUSH_PER_FID,
	MLXSW_REG_SFDF_FLUSH_PER_PORT,
	MLXSW_REG_SFDF_FLUSH_PER_PORT_AND_FID,
	MLXSW_REG_SFDF_FLUSH_PER_LAG,
	MLXSW_REG_SFDF_FLUSH_PER_LAG_AND_FID,
	MLXSW_REG_SFDF_FLUSH_PER_NVE,
	MLXSW_REG_SFDF_FLUSH_PER_NVE_AND_FID,
};
MLXSW_ITEM32(reg, sfdf, flush_type, 0x04, 28, 4);
MLXSW_ITEM32(reg, sfdf, flush_static, 0x04, 24, 1);
static inline void mlxsw_reg_sfdf_pack(char *payload,
				       enum mlxsw_reg_sfdf_flush_type type)
{
	MLXSW_REG_ZERO(sfdf, payload);
	mlxsw_reg_sfdf_flush_type_set(payload, type);
	mlxsw_reg_sfdf_flush_static_set(payload, true);
}
MLXSW_ITEM32(reg, sfdf, fid, 0x0C, 0, 16);
MLXSW_ITEM32(reg, sfdf, system_port, 0x0C, 0, 16);
MLXSW_ITEM32(reg, sfdf, port_fid_system_port, 0x08, 0, 16);
MLXSW_ITEM32(reg, sfdf, lag_id, 0x0C, 0, 10);
MLXSW_ITEM32(reg, sfdf, lag_fid_lag_id, 0x08, 0, 10);
#define MLXSW_REG_SLDR_ID 0x2014
#define MLXSW_REG_SLDR_LEN 0x0C  
MLXSW_REG_DEFINE(sldr, MLXSW_REG_SLDR_ID, MLXSW_REG_SLDR_LEN);
enum mlxsw_reg_sldr_op {
	MLXSW_REG_SLDR_OP_LAG_CREATE,
	MLXSW_REG_SLDR_OP_LAG_DESTROY,
	MLXSW_REG_SLDR_OP_LAG_ADD_PORT_LIST,
	MLXSW_REG_SLDR_OP_LAG_REMOVE_PORT_LIST,
};
MLXSW_ITEM32(reg, sldr, op, 0x00, 29, 3);
MLXSW_ITEM32(reg, sldr, lag_id, 0x00, 0, 10);
static inline void mlxsw_reg_sldr_lag_create_pack(char *payload, u8 lag_id)
{
	MLXSW_REG_ZERO(sldr, payload);
	mlxsw_reg_sldr_op_set(payload, MLXSW_REG_SLDR_OP_LAG_CREATE);
	mlxsw_reg_sldr_lag_id_set(payload, lag_id);
}
static inline void mlxsw_reg_sldr_lag_destroy_pack(char *payload, u8 lag_id)
{
	MLXSW_REG_ZERO(sldr, payload);
	mlxsw_reg_sldr_op_set(payload, MLXSW_REG_SLDR_OP_LAG_DESTROY);
	mlxsw_reg_sldr_lag_id_set(payload, lag_id);
}
MLXSW_ITEM32(reg, sldr, num_ports, 0x04, 24, 8);
MLXSW_ITEM32_INDEXED(reg, sldr, system_port, 0x08, 0, 16, 4, 0, false);
static inline void mlxsw_reg_sldr_lag_add_port_pack(char *payload, u8 lag_id,
						    u16 local_port)
{
	MLXSW_REG_ZERO(sldr, payload);
	mlxsw_reg_sldr_op_set(payload, MLXSW_REG_SLDR_OP_LAG_ADD_PORT_LIST);
	mlxsw_reg_sldr_lag_id_set(payload, lag_id);
	mlxsw_reg_sldr_num_ports_set(payload, 1);
	mlxsw_reg_sldr_system_port_set(payload, 0, local_port);
}
static inline void mlxsw_reg_sldr_lag_remove_port_pack(char *payload, u8 lag_id,
						       u16 local_port)
{
	MLXSW_REG_ZERO(sldr, payload);
	mlxsw_reg_sldr_op_set(payload, MLXSW_REG_SLDR_OP_LAG_REMOVE_PORT_LIST);
	mlxsw_reg_sldr_lag_id_set(payload, lag_id);
	mlxsw_reg_sldr_num_ports_set(payload, 1);
	mlxsw_reg_sldr_system_port_set(payload, 0, local_port);
}
#define MLXSW_REG_SLCR_ID 0x2015
#define MLXSW_REG_SLCR_LEN 0x10
MLXSW_REG_DEFINE(slcr, MLXSW_REG_SLCR_ID, MLXSW_REG_SLCR_LEN);
enum mlxsw_reg_slcr_pp {
	MLXSW_REG_SLCR_PP_GLOBAL,
	MLXSW_REG_SLCR_PP_PER_PORT,
};
MLXSW_ITEM32(reg, slcr, pp, 0x00, 24, 1);
MLXSW_ITEM32_LP(reg, slcr, 0x00, 16, 0x00, 12);
enum mlxsw_reg_slcr_type {
	MLXSW_REG_SLCR_TYPE_CRC,  
	MLXSW_REG_SLCR_TYPE_XOR,
	MLXSW_REG_SLCR_TYPE_RANDOM,
};
MLXSW_ITEM32(reg, slcr, type, 0x00, 0, 4);
#define MLXSW_REG_SLCR_LAG_HASH_IN_PORT		BIT(0)
#define MLXSW_REG_SLCR_LAG_HASH_SMAC_IP		BIT(1)
#define MLXSW_REG_SLCR_LAG_HASH_SMAC_NONIP	BIT(2)
#define MLXSW_REG_SLCR_LAG_HASH_SMAC \
	(MLXSW_REG_SLCR_LAG_HASH_SMAC_IP | \
	 MLXSW_REG_SLCR_LAG_HASH_SMAC_NONIP)
#define MLXSW_REG_SLCR_LAG_HASH_DMAC_IP		BIT(3)
#define MLXSW_REG_SLCR_LAG_HASH_DMAC_NONIP	BIT(4)
#define MLXSW_REG_SLCR_LAG_HASH_DMAC \
	(MLXSW_REG_SLCR_LAG_HASH_DMAC_IP | \
	 MLXSW_REG_SLCR_LAG_HASH_DMAC_NONIP)
#define MLXSW_REG_SLCR_LAG_HASH_ETHERTYPE_IP	BIT(5)
#define MLXSW_REG_SLCR_LAG_HASH_ETHERTYPE_NONIP	BIT(6)
#define MLXSW_REG_SLCR_LAG_HASH_ETHERTYPE \
	(MLXSW_REG_SLCR_LAG_HASH_ETHERTYPE_IP | \
	 MLXSW_REG_SLCR_LAG_HASH_ETHERTYPE_NONIP)
#define MLXSW_REG_SLCR_LAG_HASH_VLANID_IP	BIT(7)
#define MLXSW_REG_SLCR_LAG_HASH_VLANID_NONIP	BIT(8)
#define MLXSW_REG_SLCR_LAG_HASH_VLANID \
	(MLXSW_REG_SLCR_LAG_HASH_VLANID_IP | \
	 MLXSW_REG_SLCR_LAG_HASH_VLANID_NONIP)
#define MLXSW_REG_SLCR_LAG_HASH_SIP		BIT(9)
#define MLXSW_REG_SLCR_LAG_HASH_DIP		BIT(10)
#define MLXSW_REG_SLCR_LAG_HASH_SPORT		BIT(11)
#define MLXSW_REG_SLCR_LAG_HASH_DPORT		BIT(12)
#define MLXSW_REG_SLCR_LAG_HASH_IPPROTO		BIT(13)
#define MLXSW_REG_SLCR_LAG_HASH_FLOWLABEL	BIT(14)
#define MLXSW_REG_SLCR_LAG_HASH_FCOE_SID	BIT(15)
#define MLXSW_REG_SLCR_LAG_HASH_FCOE_DID	BIT(16)
#define MLXSW_REG_SLCR_LAG_HASH_FCOE_OXID	BIT(17)
#define MLXSW_REG_SLCR_LAG_HASH_ROCE_DQP	BIT(19)
MLXSW_ITEM32(reg, slcr, lag_hash, 0x04, 0, 20);
MLXSW_ITEM32(reg, slcr, seed, 0x08, 0, 32);
static inline void mlxsw_reg_slcr_pack(char *payload, u16 lag_hash, u32 seed)
{
	MLXSW_REG_ZERO(slcr, payload);
	mlxsw_reg_slcr_pp_set(payload, MLXSW_REG_SLCR_PP_GLOBAL);
	mlxsw_reg_slcr_type_set(payload, MLXSW_REG_SLCR_TYPE_CRC);
	mlxsw_reg_slcr_lag_hash_set(payload, lag_hash);
	mlxsw_reg_slcr_seed_set(payload, seed);
}
#define MLXSW_REG_SLCOR_ID 0x2016
#define MLXSW_REG_SLCOR_LEN 0x10
MLXSW_REG_DEFINE(slcor, MLXSW_REG_SLCOR_ID, MLXSW_REG_SLCOR_LEN);
enum mlxsw_reg_slcor_col {
	MLXSW_REG_SLCOR_COL_LAG_ADD_PORT,
	MLXSW_REG_SLCOR_COL_LAG_COLLECTOR_ENABLED,
	MLXSW_REG_SLCOR_COL_LAG_COLLECTOR_DISABLED,
	MLXSW_REG_SLCOR_COL_LAG_REMOVE_PORT,
};
MLXSW_ITEM32(reg, slcor, col, 0x00, 30, 2);
MLXSW_ITEM32_LP(reg, slcor, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, slcor, lag_id, 0x00, 0, 10);
MLXSW_ITEM32(reg, slcor, port_index, 0x04, 0, 10);
static inline void mlxsw_reg_slcor_pack(char *payload,
					u16 local_port, u16 lag_id,
					enum mlxsw_reg_slcor_col col)
{
	MLXSW_REG_ZERO(slcor, payload);
	mlxsw_reg_slcor_col_set(payload, col);
	mlxsw_reg_slcor_local_port_set(payload, local_port);
	mlxsw_reg_slcor_lag_id_set(payload, lag_id);
}
static inline void mlxsw_reg_slcor_port_add_pack(char *payload,
						 u16 local_port, u16 lag_id,
						 u8 port_index)
{
	mlxsw_reg_slcor_pack(payload, local_port, lag_id,
			     MLXSW_REG_SLCOR_COL_LAG_ADD_PORT);
	mlxsw_reg_slcor_port_index_set(payload, port_index);
}
static inline void mlxsw_reg_slcor_port_remove_pack(char *payload,
						    u16 local_port, u16 lag_id)
{
	mlxsw_reg_slcor_pack(payload, local_port, lag_id,
			     MLXSW_REG_SLCOR_COL_LAG_REMOVE_PORT);
}
static inline void mlxsw_reg_slcor_col_enable_pack(char *payload,
						   u16 local_port, u16 lag_id)
{
	mlxsw_reg_slcor_pack(payload, local_port, lag_id,
			     MLXSW_REG_SLCOR_COL_LAG_COLLECTOR_ENABLED);
}
static inline void mlxsw_reg_slcor_col_disable_pack(char *payload,
						    u16 local_port, u16 lag_id)
{
	mlxsw_reg_slcor_pack(payload, local_port, lag_id,
			     MLXSW_REG_SLCOR_COL_LAG_COLLECTOR_ENABLED);
}
#define MLXSW_REG_SPMLR_ID 0x2018
#define MLXSW_REG_SPMLR_LEN 0x8
MLXSW_REG_DEFINE(spmlr, MLXSW_REG_SPMLR_ID, MLXSW_REG_SPMLR_LEN);
MLXSW_ITEM32_LP(reg, spmlr, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spmlr, sub_port, 0x00, 8, 8);
enum mlxsw_reg_spmlr_learn_mode {
	MLXSW_REG_SPMLR_LEARN_MODE_DISABLE = 0,
	MLXSW_REG_SPMLR_LEARN_MODE_ENABLE = 2,
	MLXSW_REG_SPMLR_LEARN_MODE_SEC = 3,
};
MLXSW_ITEM32(reg, spmlr, learn_mode, 0x04, 30, 2);
static inline void mlxsw_reg_spmlr_pack(char *payload, u16 local_port,
					enum mlxsw_reg_spmlr_learn_mode mode)
{
	MLXSW_REG_ZERO(spmlr, payload);
	mlxsw_reg_spmlr_local_port_set(payload, local_port);
	mlxsw_reg_spmlr_sub_port_set(payload, 0);
	mlxsw_reg_spmlr_learn_mode_set(payload, mode);
}
#define MLXSW_REG_SVFA_ID 0x201C
#define MLXSW_REG_SVFA_LEN 0x18
MLXSW_REG_DEFINE(svfa, MLXSW_REG_SVFA_ID, MLXSW_REG_SVFA_LEN);
MLXSW_ITEM32(reg, svfa, swid, 0x00, 24, 8);
MLXSW_ITEM32_LP(reg, svfa, 0x00, 16, 0x00, 12);
enum mlxsw_reg_svfa_mt {
	MLXSW_REG_SVFA_MT_VID_TO_FID,
	MLXSW_REG_SVFA_MT_PORT_VID_TO_FID,
	MLXSW_REG_SVFA_MT_VNI_TO_FID,
};
MLXSW_ITEM32(reg, svfa, mapping_table, 0x00, 8, 3);
MLXSW_ITEM32(reg, svfa, v, 0x00, 0, 1);
MLXSW_ITEM32(reg, svfa, fid, 0x04, 16, 16);
MLXSW_ITEM32(reg, svfa, vid, 0x04, 0, 12);
MLXSW_ITEM32(reg, svfa, counter_set_type, 0x08, 24, 8);
MLXSW_ITEM32(reg, svfa, counter_index, 0x08, 0, 24);
MLXSW_ITEM32(reg, svfa, vni, 0x10, 0, 24);
MLXSW_ITEM32(reg, svfa, irif_v, 0x14, 24, 1);
MLXSW_ITEM32(reg, svfa, irif, 0x14, 0, 16);
static inline void __mlxsw_reg_svfa_pack(char *payload,
					 enum mlxsw_reg_svfa_mt mt, bool valid,
					 u16 fid, bool irif_v, u16 irif)
{
	MLXSW_REG_ZERO(svfa, payload);
	mlxsw_reg_svfa_swid_set(payload, 0);
	mlxsw_reg_svfa_mapping_table_set(payload, mt);
	mlxsw_reg_svfa_v_set(payload, valid);
	mlxsw_reg_svfa_fid_set(payload, fid);
	mlxsw_reg_svfa_irif_v_set(payload, irif_v);
	mlxsw_reg_svfa_irif_set(payload, irif_v ? irif : 0);
}
static inline void mlxsw_reg_svfa_port_vid_pack(char *payload, u16 local_port,
						bool valid, u16 fid, u16 vid,
						bool irif_v, u16 irif)
{
	enum mlxsw_reg_svfa_mt mt = MLXSW_REG_SVFA_MT_PORT_VID_TO_FID;
	__mlxsw_reg_svfa_pack(payload, mt, valid, fid, irif_v, irif);
	mlxsw_reg_svfa_local_port_set(payload, local_port);
	mlxsw_reg_svfa_vid_set(payload, vid);
}
static inline void mlxsw_reg_svfa_vid_pack(char *payload, bool valid, u16 fid,
					   u16 vid, bool irif_v, u16 irif)
{
	enum mlxsw_reg_svfa_mt mt = MLXSW_REG_SVFA_MT_VID_TO_FID;
	__mlxsw_reg_svfa_pack(payload, mt, valid, fid, irif_v, irif);
	mlxsw_reg_svfa_vid_set(payload, vid);
}
static inline void mlxsw_reg_svfa_vni_pack(char *payload, bool valid, u16 fid,
					   u32 vni, bool irif_v, u16 irif)
{
	enum mlxsw_reg_svfa_mt mt = MLXSW_REG_SVFA_MT_VNI_TO_FID;
	__mlxsw_reg_svfa_pack(payload, mt, valid, fid, irif_v, irif);
	mlxsw_reg_svfa_vni_set(payload, vni);
}
#define MLXSW_REG_SPVTR_ID 0x201D
#define MLXSW_REG_SPVTR_LEN 0x10
MLXSW_REG_DEFINE(spvtr, MLXSW_REG_SPVTR_ID, MLXSW_REG_SPVTR_LEN);
MLXSW_ITEM32(reg, spvtr, tport, 0x00, 24, 1);
MLXSW_ITEM32_LP(reg, spvtr, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spvtr, ippe, 0x04, 31, 1);
MLXSW_ITEM32(reg, spvtr, ipve, 0x04, 30, 1);
MLXSW_ITEM32(reg, spvtr, epve, 0x04, 29, 1);
MLXSW_ITEM32(reg, spvtr, ipprio_mode, 0x04, 20, 4);
enum mlxsw_reg_spvtr_ipvid_mode {
	MLXSW_REG_SPVTR_IPVID_MODE_IEEE_COMPLIANT_PVID,
	MLXSW_REG_SPVTR_IPVID_MODE_PUSH_VLAN_FOR_UNTAGGED_PACKET,
	MLXSW_REG_SPVTR_IPVID_MODE_ALWAYS_PUSH_VLAN,
};
MLXSW_ITEM32(reg, spvtr, ipvid_mode, 0x04, 16, 4);
enum mlxsw_reg_spvtr_epvid_mode {
	MLXSW_REG_SPVTR_EPVID_MODE_IEEE_COMPLIANT_VLAN_MEMBERSHIP,
	MLXSW_REG_SPVTR_EPVID_MODE_POP_VLAN,
};
MLXSW_ITEM32(reg, spvtr, epvid_mode, 0x04, 0, 4);
static inline void mlxsw_reg_spvtr_pack(char *payload, bool tport,
					u16 local_port,
					enum mlxsw_reg_spvtr_ipvid_mode ipvid_mode)
{
	MLXSW_REG_ZERO(spvtr, payload);
	mlxsw_reg_spvtr_tport_set(payload, tport);
	mlxsw_reg_spvtr_local_port_set(payload, local_port);
	mlxsw_reg_spvtr_ipvid_mode_set(payload, ipvid_mode);
	mlxsw_reg_spvtr_ipve_set(payload, true);
}
#define MLXSW_REG_SVPE_ID 0x201E
#define MLXSW_REG_SVPE_LEN 0x4
MLXSW_REG_DEFINE(svpe, MLXSW_REG_SVPE_ID, MLXSW_REG_SVPE_LEN);
MLXSW_ITEM32_LP(reg, svpe, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, svpe, vp_en, 0x00, 8, 1);
static inline void mlxsw_reg_svpe_pack(char *payload, u16 local_port,
				       bool enable)
{
	MLXSW_REG_ZERO(svpe, payload);
	mlxsw_reg_svpe_local_port_set(payload, local_port);
	mlxsw_reg_svpe_vp_en_set(payload, enable);
}
#define MLXSW_REG_SFMR_ID 0x201F
#define MLXSW_REG_SFMR_LEN 0x30
MLXSW_REG_DEFINE(sfmr, MLXSW_REG_SFMR_ID, MLXSW_REG_SFMR_LEN);
enum mlxsw_reg_sfmr_op {
	MLXSW_REG_SFMR_OP_CREATE_FID,
	MLXSW_REG_SFMR_OP_DESTROY_FID,
};
MLXSW_ITEM32(reg, sfmr, op, 0x00, 24, 4);
MLXSW_ITEM32(reg, sfmr, fid, 0x00, 0, 16);
MLXSW_ITEM32(reg, sfmr, flood_rsp, 0x08, 31, 1);
MLXSW_ITEM32(reg, sfmr, flood_bridge_type, 0x08, 28, 1);
MLXSW_ITEM32(reg, sfmr, fid_offset, 0x08, 0, 16);
MLXSW_ITEM32(reg, sfmr, vtfp, 0x0C, 31, 1);
MLXSW_ITEM32(reg, sfmr, nve_tunnel_flood_ptr, 0x0C, 0, 24);
MLXSW_ITEM32(reg, sfmr, vv, 0x10, 31, 1);
MLXSW_ITEM32(reg, sfmr, vni, 0x10, 0, 24);
MLXSW_ITEM32(reg, sfmr, irif_v, 0x14, 24, 1);
MLXSW_ITEM32(reg, sfmr, irif, 0x14, 0, 16);
MLXSW_ITEM32(reg, sfmr, smpe_valid, 0x28, 20, 1);
MLXSW_ITEM32(reg, sfmr, smpe, 0x28, 0, 16);
static inline void mlxsw_reg_sfmr_pack(char *payload,
				       enum mlxsw_reg_sfmr_op op, u16 fid,
				       u16 fid_offset, bool flood_rsp,
				       enum mlxsw_reg_bridge_type bridge_type,
				       bool smpe_valid, u16 smpe)
{
	MLXSW_REG_ZERO(sfmr, payload);
	mlxsw_reg_sfmr_op_set(payload, op);
	mlxsw_reg_sfmr_fid_set(payload, fid);
	mlxsw_reg_sfmr_fid_offset_set(payload, fid_offset);
	mlxsw_reg_sfmr_vtfp_set(payload, false);
	mlxsw_reg_sfmr_vv_set(payload, false);
	mlxsw_reg_sfmr_flood_rsp_set(payload, flood_rsp);
	mlxsw_reg_sfmr_flood_bridge_type_set(payload, bridge_type);
	mlxsw_reg_sfmr_smpe_valid_set(payload, smpe_valid);
	mlxsw_reg_sfmr_smpe_set(payload, smpe);
}
#define MLXSW_REG_SPVMLR_ID 0x2020
#define MLXSW_REG_SPVMLR_BASE_LEN 0x04  
#define MLXSW_REG_SPVMLR_REC_LEN 0x04  
#define MLXSW_REG_SPVMLR_REC_MAX_COUNT 255
#define MLXSW_REG_SPVMLR_LEN (MLXSW_REG_SPVMLR_BASE_LEN + \
			      MLXSW_REG_SPVMLR_REC_LEN * \
			      MLXSW_REG_SPVMLR_REC_MAX_COUNT)
MLXSW_REG_DEFINE(spvmlr, MLXSW_REG_SPVMLR_ID, MLXSW_REG_SPVMLR_LEN);
MLXSW_ITEM32_LP(reg, spvmlr, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spvmlr, num_rec, 0x00, 0, 8);
MLXSW_ITEM32_INDEXED(reg, spvmlr, rec_learn_enable, MLXSW_REG_SPVMLR_BASE_LEN,
		     31, 1, MLXSW_REG_SPVMLR_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, spvmlr, rec_vid, MLXSW_REG_SPVMLR_BASE_LEN, 0, 12,
		     MLXSW_REG_SPVMLR_REC_LEN, 0x00, false);
static inline void mlxsw_reg_spvmlr_pack(char *payload, u16 local_port,
					 u16 vid_begin, u16 vid_end,
					 bool learn_enable)
{
	int num_rec = vid_end - vid_begin + 1;
	int i;
	WARN_ON(num_rec < 1 || num_rec > MLXSW_REG_SPVMLR_REC_MAX_COUNT);
	MLXSW_REG_ZERO(spvmlr, payload);
	mlxsw_reg_spvmlr_local_port_set(payload, local_port);
	mlxsw_reg_spvmlr_num_rec_set(payload, num_rec);
	for (i = 0; i < num_rec; i++) {
		mlxsw_reg_spvmlr_rec_learn_enable_set(payload, i, learn_enable);
		mlxsw_reg_spvmlr_rec_vid_set(payload, i, vid_begin + i);
	}
}
#define MLXSW_REG_SPFSR_ID 0x2023
#define MLXSW_REG_SPFSR_LEN 0x08
MLXSW_REG_DEFINE(spfsr, MLXSW_REG_SPFSR_ID, MLXSW_REG_SPFSR_LEN);
MLXSW_ITEM32_LP(reg, spfsr, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spfsr, security, 0x04, 31, 1);
static inline void mlxsw_reg_spfsr_pack(char *payload, u16 local_port,
					bool security)
{
	MLXSW_REG_ZERO(spfsr, payload);
	mlxsw_reg_spfsr_local_port_set(payload, local_port);
	mlxsw_reg_spfsr_security_set(payload, security);
}
#define MLXSW_REG_SPVC_ID 0x2026
#define MLXSW_REG_SPVC_LEN 0x0C
MLXSW_REG_DEFINE(spvc, MLXSW_REG_SPVC_ID, MLXSW_REG_SPVC_LEN);
MLXSW_ITEM32_LP(reg, spvc, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spvc, inner_et2, 0x08, 17, 1);
MLXSW_ITEM32(reg, spvc, et2, 0x08, 16, 1);
MLXSW_ITEM32(reg, spvc, inner_et1, 0x08, 9, 1);
MLXSW_ITEM32(reg, spvc, et1, 0x08, 8, 1);
MLXSW_ITEM32(reg, spvc, inner_et0, 0x08, 1, 1);
MLXSW_ITEM32(reg, spvc, et0, 0x08, 0, 1);
static inline void mlxsw_reg_spvc_pack(char *payload, u16 local_port, bool et1,
				       bool et0)
{
	MLXSW_REG_ZERO(spvc, payload);
	mlxsw_reg_spvc_local_port_set(payload, local_port);
	mlxsw_reg_spvc_inner_et1_set(payload, 1);
	mlxsw_reg_spvc_inner_et0_set(payload, 1);
	mlxsw_reg_spvc_et1_set(payload, et1);
	mlxsw_reg_spvc_et0_set(payload, et0);
}
#define MLXSW_REG_SPEVET_ID 0x202A
#define MLXSW_REG_SPEVET_LEN 0x08
MLXSW_REG_DEFINE(spevet, MLXSW_REG_SPEVET_ID, MLXSW_REG_SPEVET_LEN);
MLXSW_ITEM32_LP(reg, spevet, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, spevet, et_vlan, 0x04, 16, 2);
static inline void mlxsw_reg_spevet_pack(char *payload, u16 local_port,
					 u8 et_vlan)
{
	MLXSW_REG_ZERO(spevet, payload);
	mlxsw_reg_spevet_local_port_set(payload, local_port);
	mlxsw_reg_spevet_et_vlan_set(payload, et_vlan);
}
#define MLXSW_REG_SMPE_ID 0x202B
#define MLXSW_REG_SMPE_LEN 0x0C
MLXSW_REG_DEFINE(smpe, MLXSW_REG_SMPE_ID, MLXSW_REG_SMPE_LEN);
MLXSW_ITEM32_LP(reg, smpe, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, smpe, smpe_index, 0x04, 0, 16);
MLXSW_ITEM32(reg, smpe, evid, 0x08, 0, 12);
static inline void mlxsw_reg_smpe_pack(char *payload, u16 local_port,
				       u16 smpe_index, u16 evid)
{
	MLXSW_REG_ZERO(smpe, payload);
	mlxsw_reg_smpe_local_port_set(payload, local_port);
	mlxsw_reg_smpe_smpe_index_set(payload, smpe_index);
	mlxsw_reg_smpe_evid_set(payload, evid);
}
#define MLXSW_REG_SMID2_ID 0x2034
#define MLXSW_REG_SMID2_LEN 0x120
MLXSW_REG_DEFINE(smid2, MLXSW_REG_SMID2_ID, MLXSW_REG_SMID2_LEN);
MLXSW_ITEM32(reg, smid2, swid, 0x00, 24, 8);
MLXSW_ITEM32(reg, smid2, mid, 0x00, 0, 16);
MLXSW_ITEM32(reg, smid2, smpe_valid, 0x08, 20, 1);
MLXSW_ITEM32(reg, smid2, smpe, 0x08, 0, 16);
MLXSW_ITEM_BIT_ARRAY(reg, smid2, port, 0x20, 0x80, 1);
MLXSW_ITEM_BIT_ARRAY(reg, smid2, port_mask, 0xA0, 0x80, 1);
static inline void mlxsw_reg_smid2_pack(char *payload, u16 mid, u16 port,
					bool set, bool smpe_valid, u16 smpe)
{
	MLXSW_REG_ZERO(smid2, payload);
	mlxsw_reg_smid2_swid_set(payload, 0);
	mlxsw_reg_smid2_mid_set(payload, mid);
	mlxsw_reg_smid2_port_set(payload, port, set);
	mlxsw_reg_smid2_port_mask_set(payload, port, 1);
	mlxsw_reg_smid2_smpe_valid_set(payload, smpe_valid);
	mlxsw_reg_smid2_smpe_set(payload, smpe_valid ? smpe : 0);
}
#define MLXSW_REG_CWTP_ID 0x2802
#define MLXSW_REG_CWTP_BASE_LEN 0x28
#define MLXSW_REG_CWTP_PROFILE_DATA_REC_LEN 0x08
#define MLXSW_REG_CWTP_LEN 0x40
MLXSW_REG_DEFINE(cwtp, MLXSW_REG_CWTP_ID, MLXSW_REG_CWTP_LEN);
MLXSW_ITEM32_LP(reg, cwtp, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, cwtp, traffic_class, 32, 0, 8);
MLXSW_ITEM32_INDEXED(reg, cwtp, profile_min, MLXSW_REG_CWTP_BASE_LEN,
		     0, 20, MLXSW_REG_CWTP_PROFILE_DATA_REC_LEN, 0, false);
MLXSW_ITEM32_INDEXED(reg, cwtp, profile_percent, MLXSW_REG_CWTP_BASE_LEN,
		     24, 7, MLXSW_REG_CWTP_PROFILE_DATA_REC_LEN, 4, false);
MLXSW_ITEM32_INDEXED(reg, cwtp, profile_max, MLXSW_REG_CWTP_BASE_LEN,
		     0, 20, MLXSW_REG_CWTP_PROFILE_DATA_REC_LEN, 4, false);
#define MLXSW_REG_CWTP_MIN_VALUE 64
#define MLXSW_REG_CWTP_MAX_PROFILE 2
#define MLXSW_REG_CWTP_DEFAULT_PROFILE 1
static inline void mlxsw_reg_cwtp_pack(char *payload, u16 local_port,
				       u8 traffic_class)
{
	int i;
	MLXSW_REG_ZERO(cwtp, payload);
	mlxsw_reg_cwtp_local_port_set(payload, local_port);
	mlxsw_reg_cwtp_traffic_class_set(payload, traffic_class);
	for (i = 0; i <= MLXSW_REG_CWTP_MAX_PROFILE; i++) {
		mlxsw_reg_cwtp_profile_min_set(payload, i,
					       MLXSW_REG_CWTP_MIN_VALUE);
		mlxsw_reg_cwtp_profile_max_set(payload, i,
					       MLXSW_REG_CWTP_MIN_VALUE);
	}
}
#define MLXSW_REG_CWTP_PROFILE_TO_INDEX(profile) (profile - 1)
static inline void
mlxsw_reg_cwtp_profile_pack(char *payload, u8 profile, u32 min, u32 max,
			    u32 probability)
{
	u8 index = MLXSW_REG_CWTP_PROFILE_TO_INDEX(profile);
	mlxsw_reg_cwtp_profile_min_set(payload, index, min);
	mlxsw_reg_cwtp_profile_max_set(payload, index, max);
	mlxsw_reg_cwtp_profile_percent_set(payload, index, probability);
}
#define MLXSW_REG_CWTPM_ID 0x2803
#define MLXSW_REG_CWTPM_LEN 0x44
MLXSW_REG_DEFINE(cwtpm, MLXSW_REG_CWTPM_ID, MLXSW_REG_CWTPM_LEN);
MLXSW_ITEM32_LP(reg, cwtpm, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, cwtpm, traffic_class, 32, 0, 8);
MLXSW_ITEM32(reg, cwtpm, ew, 36, 1, 1);
MLXSW_ITEM32(reg, cwtpm, ee, 36, 0, 1);
MLXSW_ITEM32(reg, cwtpm, tcp_g, 52, 0, 2);
MLXSW_ITEM32(reg, cwtpm, tcp_y, 56, 16, 2);
MLXSW_ITEM32(reg, cwtpm, tcp_r, 56, 0, 2);
MLXSW_ITEM32(reg, cwtpm, ntcp_g, 60, 0, 2);
MLXSW_ITEM32(reg, cwtpm, ntcp_y, 64, 16, 2);
MLXSW_ITEM32(reg, cwtpm, ntcp_r, 64, 0, 2);
#define MLXSW_REG_CWTPM_RESET_PROFILE 0
static inline void mlxsw_reg_cwtpm_pack(char *payload, u16 local_port,
					u8 traffic_class, u8 profile,
					bool wred, bool ecn)
{
	MLXSW_REG_ZERO(cwtpm, payload);
	mlxsw_reg_cwtpm_local_port_set(payload, local_port);
	mlxsw_reg_cwtpm_traffic_class_set(payload, traffic_class);
	mlxsw_reg_cwtpm_ew_set(payload, wred);
	mlxsw_reg_cwtpm_ee_set(payload, ecn);
	mlxsw_reg_cwtpm_tcp_g_set(payload, profile);
	mlxsw_reg_cwtpm_tcp_y_set(payload, profile);
	mlxsw_reg_cwtpm_tcp_r_set(payload, profile);
	mlxsw_reg_cwtpm_ntcp_g_set(payload, profile);
	mlxsw_reg_cwtpm_ntcp_y_set(payload, profile);
	mlxsw_reg_cwtpm_ntcp_r_set(payload, profile);
}
#define MLXSW_REG_PGCR_ID 0x3001
#define MLXSW_REG_PGCR_LEN 0x20
MLXSW_REG_DEFINE(pgcr, MLXSW_REG_PGCR_ID, MLXSW_REG_PGCR_LEN);
MLXSW_ITEM32(reg, pgcr, default_action_pointer_base, 0x1C, 0, 24);
static inline void mlxsw_reg_pgcr_pack(char *payload, u32 pointer_base)
{
	MLXSW_REG_ZERO(pgcr, payload);
	mlxsw_reg_pgcr_default_action_pointer_base_set(payload, pointer_base);
}
#define MLXSW_REG_PPBT_ID 0x3002
#define MLXSW_REG_PPBT_LEN 0x14
MLXSW_REG_DEFINE(ppbt, MLXSW_REG_PPBT_ID, MLXSW_REG_PPBT_LEN);
enum mlxsw_reg_pxbt_e {
	MLXSW_REG_PXBT_E_IACL,
	MLXSW_REG_PXBT_E_EACL,
};
MLXSW_ITEM32(reg, ppbt, e, 0x00, 31, 1);
enum mlxsw_reg_pxbt_op {
	MLXSW_REG_PXBT_OP_BIND,
	MLXSW_REG_PXBT_OP_UNBIND,
};
MLXSW_ITEM32(reg, ppbt, op, 0x00, 28, 3);
MLXSW_ITEM32_LP(reg, ppbt, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, ppbt, g, 0x10, 31, 1);
MLXSW_ITEM32(reg, ppbt, acl_info, 0x10, 0, 16);
static inline void mlxsw_reg_ppbt_pack(char *payload, enum mlxsw_reg_pxbt_e e,
				       enum mlxsw_reg_pxbt_op op,
				       u16 local_port, u16 acl_info)
{
	MLXSW_REG_ZERO(ppbt, payload);
	mlxsw_reg_ppbt_e_set(payload, e);
	mlxsw_reg_ppbt_op_set(payload, op);
	mlxsw_reg_ppbt_local_port_set(payload, local_port);
	mlxsw_reg_ppbt_g_set(payload, true);
	mlxsw_reg_ppbt_acl_info_set(payload, acl_info);
}
#define MLXSW_REG_PACL_ID 0x3004
#define MLXSW_REG_PACL_LEN 0x70
MLXSW_REG_DEFINE(pacl, MLXSW_REG_PACL_ID, MLXSW_REG_PACL_LEN);
MLXSW_ITEM32(reg, pacl, v, 0x00, 24, 1);
MLXSW_ITEM32(reg, pacl, acl_id, 0x08, 0, 16);
#define MLXSW_REG_PXXX_TCAM_REGION_INFO_LEN 16
MLXSW_ITEM_BUF(reg, pacl, tcam_region_info, 0x30,
	       MLXSW_REG_PXXX_TCAM_REGION_INFO_LEN);
static inline void mlxsw_reg_pacl_pack(char *payload, u16 acl_id,
				       bool valid, const char *tcam_region_info)
{
	MLXSW_REG_ZERO(pacl, payload);
	mlxsw_reg_pacl_acl_id_set(payload, acl_id);
	mlxsw_reg_pacl_v_set(payload, valid);
	mlxsw_reg_pacl_tcam_region_info_memcpy_to(payload, tcam_region_info);
}
#define MLXSW_REG_PAGT_ID 0x3005
#define MLXSW_REG_PAGT_BASE_LEN 0x30
#define MLXSW_REG_PAGT_ACL_LEN 4
#define MLXSW_REG_PAGT_ACL_MAX_NUM 16
#define MLXSW_REG_PAGT_LEN (MLXSW_REG_PAGT_BASE_LEN + \
		MLXSW_REG_PAGT_ACL_MAX_NUM * MLXSW_REG_PAGT_ACL_LEN)
MLXSW_REG_DEFINE(pagt, MLXSW_REG_PAGT_ID, MLXSW_REG_PAGT_LEN);
MLXSW_ITEM32(reg, pagt, size, 0x00, 0, 8);
MLXSW_ITEM32(reg, pagt, acl_group_id, 0x08, 0, 16);
MLXSW_ITEM32_INDEXED(reg, pagt, multi, 0x30, 31, 1, 0x04, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, pagt, acl_id, 0x30, 0, 16, 0x04, 0x00, false);
static inline void mlxsw_reg_pagt_pack(char *payload, u16 acl_group_id)
{
	MLXSW_REG_ZERO(pagt, payload);
	mlxsw_reg_pagt_acl_group_id_set(payload, acl_group_id);
}
static inline void mlxsw_reg_pagt_acl_id_pack(char *payload, int index,
					      u16 acl_id, bool multi)
{
	u8 size = mlxsw_reg_pagt_size_get(payload);
	if (index >= size)
		mlxsw_reg_pagt_size_set(payload, index + 1);
	mlxsw_reg_pagt_multi_set(payload, index, multi);
	mlxsw_reg_pagt_acl_id_set(payload, index, acl_id);
}
#define MLXSW_REG_PTAR_ID 0x3006
#define MLXSW_REG_PTAR_BASE_LEN 0x20
#define MLXSW_REG_PTAR_KEY_ID_LEN 1
#define MLXSW_REG_PTAR_KEY_ID_MAX_NUM 16
#define MLXSW_REG_PTAR_LEN (MLXSW_REG_PTAR_BASE_LEN + \
		MLXSW_REG_PTAR_KEY_ID_MAX_NUM * MLXSW_REG_PTAR_KEY_ID_LEN)
MLXSW_REG_DEFINE(ptar, MLXSW_REG_PTAR_ID, MLXSW_REG_PTAR_LEN);
enum mlxsw_reg_ptar_op {
	MLXSW_REG_PTAR_OP_ALLOC,
	MLXSW_REG_PTAR_OP_RESIZE,
	MLXSW_REG_PTAR_OP_FREE,
	MLXSW_REG_PTAR_OP_TEST,
};
MLXSW_ITEM32(reg, ptar, op, 0x00, 28, 4);
MLXSW_ITEM32(reg, ptar, action_set_type, 0x00, 16, 8);
enum mlxsw_reg_ptar_key_type {
	MLXSW_REG_PTAR_KEY_TYPE_FLEX = 0x50,  
	MLXSW_REG_PTAR_KEY_TYPE_FLEX2 = 0x51,  
};
MLXSW_ITEM32(reg, ptar, key_type, 0x00, 0, 8);
MLXSW_ITEM32(reg, ptar, region_size, 0x04, 0, 16);
MLXSW_ITEM32(reg, ptar, region_id, 0x08, 0, 16);
MLXSW_ITEM_BUF(reg, ptar, tcam_region_info, 0x10,
	       MLXSW_REG_PXXX_TCAM_REGION_INFO_LEN);
MLXSW_ITEM8_INDEXED(reg, ptar, flexible_key_id, 0x20, 0, 8,
		    MLXSW_REG_PTAR_KEY_ID_LEN, 0x00, false);
static inline void mlxsw_reg_ptar_pack(char *payload, enum mlxsw_reg_ptar_op op,
				       enum mlxsw_reg_ptar_key_type key_type,
				       u16 region_size, u16 region_id,
				       const char *tcam_region_info)
{
	MLXSW_REG_ZERO(ptar, payload);
	mlxsw_reg_ptar_op_set(payload, op);
	mlxsw_reg_ptar_action_set_type_set(payload, 2);  
	mlxsw_reg_ptar_key_type_set(payload, key_type);
	mlxsw_reg_ptar_region_size_set(payload, region_size);
	mlxsw_reg_ptar_region_id_set(payload, region_id);
	mlxsw_reg_ptar_tcam_region_info_memcpy_to(payload, tcam_region_info);
}
static inline void mlxsw_reg_ptar_key_id_pack(char *payload, int index,
					      u16 key_id)
{
	mlxsw_reg_ptar_flexible_key_id_set(payload, index, key_id);
}
static inline void mlxsw_reg_ptar_unpack(char *payload, char *tcam_region_info)
{
	mlxsw_reg_ptar_tcam_region_info_memcpy_from(payload, tcam_region_info);
}
#define MLXSW_REG_PPRR_ID 0x3008
#define MLXSW_REG_PPRR_LEN 0x14
MLXSW_REG_DEFINE(pprr, MLXSW_REG_PPRR_ID, MLXSW_REG_PPRR_LEN);
MLXSW_ITEM32(reg, pprr, ipv4, 0x00, 31, 1);
MLXSW_ITEM32(reg, pprr, ipv6, 0x00, 30, 1);
MLXSW_ITEM32(reg, pprr, src, 0x00, 29, 1);
MLXSW_ITEM32(reg, pprr, dst, 0x00, 28, 1);
MLXSW_ITEM32(reg, pprr, tcp, 0x00, 27, 1);
MLXSW_ITEM32(reg, pprr, udp, 0x00, 26, 1);
MLXSW_ITEM32(reg, pprr, register_index, 0x00, 0, 8);
MLXSW_ITEM32(reg, pprr, port_range_min, 0x04, 16, 16);
MLXSW_ITEM32(reg, pprr, port_range_max, 0x04, 0, 16);
static inline void mlxsw_reg_pprr_pack(char *payload, u8 register_index)
{
	MLXSW_REG_ZERO(pprr, payload);
	mlxsw_reg_pprr_register_index_set(payload, register_index);
}
#define MLXSW_REG_PPBS_ID 0x300C
#define MLXSW_REG_PPBS_LEN 0x14
MLXSW_REG_DEFINE(ppbs, MLXSW_REG_PPBS_ID, MLXSW_REG_PPBS_LEN);
MLXSW_ITEM32(reg, ppbs, pbs_ptr, 0x08, 0, 24);
MLXSW_ITEM32(reg, ppbs, system_port, 0x10, 0, 16);
static inline void mlxsw_reg_ppbs_pack(char *payload, u32 pbs_ptr,
				       u16 system_port)
{
	MLXSW_REG_ZERO(ppbs, payload);
	mlxsw_reg_ppbs_pbs_ptr_set(payload, pbs_ptr);
	mlxsw_reg_ppbs_system_port_set(payload, system_port);
}
#define MLXSW_REG_PRCR_ID 0x300D
#define MLXSW_REG_PRCR_LEN 0x40
MLXSW_REG_DEFINE(prcr, MLXSW_REG_PRCR_ID, MLXSW_REG_PRCR_LEN);
enum mlxsw_reg_prcr_op {
	MLXSW_REG_PRCR_OP_MOVE,
	MLXSW_REG_PRCR_OP_COPY,
};
MLXSW_ITEM32(reg, prcr, op, 0x00, 28, 4);
MLXSW_ITEM32(reg, prcr, offset, 0x00, 0, 16);
MLXSW_ITEM32(reg, prcr, size, 0x04, 0, 16);
MLXSW_ITEM_BUF(reg, prcr, tcam_region_info, 0x10,
	       MLXSW_REG_PXXX_TCAM_REGION_INFO_LEN);
MLXSW_ITEM32(reg, prcr, dest_offset, 0x20, 0, 16);
MLXSW_ITEM_BUF(reg, prcr, dest_tcam_region_info, 0x30,
	       MLXSW_REG_PXXX_TCAM_REGION_INFO_LEN);
static inline void mlxsw_reg_prcr_pack(char *payload, enum mlxsw_reg_prcr_op op,
				       const char *src_tcam_region_info,
				       u16 src_offset,
				       const char *dest_tcam_region_info,
				       u16 dest_offset, u16 size)
{
	MLXSW_REG_ZERO(prcr, payload);
	mlxsw_reg_prcr_op_set(payload, op);
	mlxsw_reg_prcr_offset_set(payload, src_offset);
	mlxsw_reg_prcr_size_set(payload, size);
	mlxsw_reg_prcr_tcam_region_info_memcpy_to(payload,
						  src_tcam_region_info);
	mlxsw_reg_prcr_dest_offset_set(payload, dest_offset);
	mlxsw_reg_prcr_dest_tcam_region_info_memcpy_to(payload,
						       dest_tcam_region_info);
}
#define MLXSW_REG_PEFA_ID 0x300F
#define MLXSW_REG_PEFA_LEN 0xB0
MLXSW_REG_DEFINE(pefa, MLXSW_REG_PEFA_ID, MLXSW_REG_PEFA_LEN);
MLXSW_ITEM32(reg, pefa, index, 0x00, 0, 24);
MLXSW_ITEM32(reg, pefa, a, 0x04, 29, 1);
MLXSW_ITEM32(reg, pefa, ca, 0x04, 24, 1);
#define MLXSW_REG_FLEX_ACTION_SET_LEN 0xA8
MLXSW_ITEM_BUF(reg, pefa, flex_action_set, 0x08, MLXSW_REG_FLEX_ACTION_SET_LEN);
static inline void mlxsw_reg_pefa_pack(char *payload, u32 index, bool ca,
				       const char *flex_action_set)
{
	MLXSW_REG_ZERO(pefa, payload);
	mlxsw_reg_pefa_index_set(payload, index);
	mlxsw_reg_pefa_ca_set(payload, ca);
	if (flex_action_set)
		mlxsw_reg_pefa_flex_action_set_memcpy_to(payload,
							 flex_action_set);
}
static inline void mlxsw_reg_pefa_unpack(char *payload, bool *p_a)
{
	*p_a = mlxsw_reg_pefa_a_get(payload);
}
#define MLXSW_REG_PEMRBT_ID 0x3014
#define MLXSW_REG_PEMRBT_LEN 0x14
MLXSW_REG_DEFINE(pemrbt, MLXSW_REG_PEMRBT_ID, MLXSW_REG_PEMRBT_LEN);
enum mlxsw_reg_pemrbt_protocol {
	MLXSW_REG_PEMRBT_PROTO_IPV4,
	MLXSW_REG_PEMRBT_PROTO_IPV6,
};
MLXSW_ITEM32(reg, pemrbt, protocol, 0x00, 0, 1);
MLXSW_ITEM32(reg, pemrbt, group_id, 0x10, 0, 16);
static inline void
mlxsw_reg_pemrbt_pack(char *payload, enum mlxsw_reg_pemrbt_protocol protocol,
		      u16 group_id)
{
	MLXSW_REG_ZERO(pemrbt, payload);
	mlxsw_reg_pemrbt_protocol_set(payload, protocol);
	mlxsw_reg_pemrbt_group_id_set(payload, group_id);
}
#define MLXSW_REG_PTCE2_ID 0x3017
#define MLXSW_REG_PTCE2_LEN 0x1D8
MLXSW_REG_DEFINE(ptce2, MLXSW_REG_PTCE2_ID, MLXSW_REG_PTCE2_LEN);
MLXSW_ITEM32(reg, ptce2, v, 0x00, 31, 1);
MLXSW_ITEM32(reg, ptce2, a, 0x00, 30, 1);
enum mlxsw_reg_ptce2_op {
	MLXSW_REG_PTCE2_OP_QUERY_READ = 0,
	MLXSW_REG_PTCE2_OP_QUERY_CLEAR_ON_READ = 1,
	MLXSW_REG_PTCE2_OP_WRITE_WRITE = 0,
	MLXSW_REG_PTCE2_OP_WRITE_UPDATE = 1,
	MLXSW_REG_PTCE2_OP_WRITE_CLEAR_ACTIVITY = 2,
};
MLXSW_ITEM32(reg, ptce2, op, 0x00, 20, 3);
MLXSW_ITEM32(reg, ptce2, offset, 0x00, 0, 16);
MLXSW_ITEM32(reg, ptce2, priority, 0x04, 0, 24);
MLXSW_ITEM_BUF(reg, ptce2, tcam_region_info, 0x10,
	       MLXSW_REG_PXXX_TCAM_REGION_INFO_LEN);
#define MLXSW_REG_PTCEX_FLEX_KEY_BLOCKS_LEN 96
MLXSW_ITEM_BUF(reg, ptce2, flex_key_blocks, 0x20,
	       MLXSW_REG_PTCEX_FLEX_KEY_BLOCKS_LEN);
MLXSW_ITEM_BUF(reg, ptce2, mask, 0x80,
	       MLXSW_REG_PTCEX_FLEX_KEY_BLOCKS_LEN);
MLXSW_ITEM_BUF(reg, ptce2, flex_action_set, 0xE0,
	       MLXSW_REG_FLEX_ACTION_SET_LEN);
static inline void mlxsw_reg_ptce2_pack(char *payload, bool valid,
					enum mlxsw_reg_ptce2_op op,
					const char *tcam_region_info,
					u16 offset, u32 priority)
{
	MLXSW_REG_ZERO(ptce2, payload);
	mlxsw_reg_ptce2_v_set(payload, valid);
	mlxsw_reg_ptce2_op_set(payload, op);
	mlxsw_reg_ptce2_offset_set(payload, offset);
	mlxsw_reg_ptce2_priority_set(payload, priority);
	mlxsw_reg_ptce2_tcam_region_info_memcpy_to(payload, tcam_region_info);
}
#define MLXSW_REG_PERPT_ID 0x3021
#define MLXSW_REG_PERPT_LEN 0x80
MLXSW_REG_DEFINE(perpt, MLXSW_REG_PERPT_ID, MLXSW_REG_PERPT_LEN);
MLXSW_ITEM32(reg, perpt, erpt_bank, 0x00, 16, 4);
MLXSW_ITEM32(reg, perpt, erpt_index, 0x00, 0, 8);
enum mlxsw_reg_perpt_key_size {
	MLXSW_REG_PERPT_KEY_SIZE_2KB,
	MLXSW_REG_PERPT_KEY_SIZE_4KB,
	MLXSW_REG_PERPT_KEY_SIZE_8KB,
	MLXSW_REG_PERPT_KEY_SIZE_12KB,
};
MLXSW_ITEM32(reg, perpt, key_size, 0x04, 0, 4);
MLXSW_ITEM32(reg, perpt, bf_bypass, 0x08, 8, 1);
MLXSW_ITEM32(reg, perpt, erp_id, 0x08, 0, 4);
MLXSW_ITEM32(reg, perpt, erpt_base_bank, 0x0C, 16, 4);
MLXSW_ITEM32(reg, perpt, erpt_base_index, 0x0C, 0, 8);
MLXSW_ITEM32(reg, perpt, erp_index_in_vector, 0x10, 0, 4);
MLXSW_ITEM_BIT_ARRAY(reg, perpt, erp_vector, 0x14, 4, 1);
MLXSW_ITEM_BUF(reg, perpt, mask, 0x20, MLXSW_REG_PTCEX_FLEX_KEY_BLOCKS_LEN);
static inline void mlxsw_reg_perpt_erp_vector_pack(char *payload,
						   unsigned long *erp_vector,
						   unsigned long size)
{
	unsigned long bit;
	for_each_set_bit(bit, erp_vector, size)
		mlxsw_reg_perpt_erp_vector_set(payload, bit, true);
}
static inline void
mlxsw_reg_perpt_pack(char *payload, u8 erpt_bank, u8 erpt_index,
		     enum mlxsw_reg_perpt_key_size key_size, u8 erp_id,
		     u8 erpt_base_bank, u8 erpt_base_index, u8 erp_index,
		     char *mask)
{
	MLXSW_REG_ZERO(perpt, payload);
	mlxsw_reg_perpt_erpt_bank_set(payload, erpt_bank);
	mlxsw_reg_perpt_erpt_index_set(payload, erpt_index);
	mlxsw_reg_perpt_key_size_set(payload, key_size);
	mlxsw_reg_perpt_bf_bypass_set(payload, false);
	mlxsw_reg_perpt_erp_id_set(payload, erp_id);
	mlxsw_reg_perpt_erpt_base_bank_set(payload, erpt_base_bank);
	mlxsw_reg_perpt_erpt_base_index_set(payload, erpt_base_index);
	mlxsw_reg_perpt_erp_index_in_vector_set(payload, erp_index);
	mlxsw_reg_perpt_mask_memcpy_to(payload, mask);
}
#define MLXSW_REG_PERAR_ID 0x3026
#define MLXSW_REG_PERAR_LEN 0x08
MLXSW_REG_DEFINE(perar, MLXSW_REG_PERAR_ID, MLXSW_REG_PERAR_LEN);
MLXSW_ITEM32(reg, perar, region_id, 0x00, 0, 16);
static inline unsigned int
mlxsw_reg_perar_hw_regions_needed(unsigned int block_num)
{
	return DIV_ROUND_UP(block_num, 4);
}
MLXSW_ITEM32(reg, perar, hw_region, 0x04, 0, 16);
static inline void mlxsw_reg_perar_pack(char *payload, u16 region_id,
					u16 hw_region)
{
	MLXSW_REG_ZERO(perar, payload);
	mlxsw_reg_perar_region_id_set(payload, region_id);
	mlxsw_reg_perar_hw_region_set(payload, hw_region);
}
#define MLXSW_REG_PTCE3_ID 0x3027
#define MLXSW_REG_PTCE3_LEN 0xF0
MLXSW_REG_DEFINE(ptce3, MLXSW_REG_PTCE3_ID, MLXSW_REG_PTCE3_LEN);
MLXSW_ITEM32(reg, ptce3, v, 0x00, 31, 1);
enum mlxsw_reg_ptce3_op {
	 MLXSW_REG_PTCE3_OP_WRITE_WRITE = 0,
	 MLXSW_REG_PTCE3_OP_WRITE_UPDATE = 1,
	 MLXSW_REG_PTCE3_OP_QUERY_READ = 0,
};
MLXSW_ITEM32(reg, ptce3, op, 0x00, 20, 3);
MLXSW_ITEM32(reg, ptce3, priority, 0x04, 0, 24);
MLXSW_ITEM_BUF(reg, ptce3, tcam_region_info, 0x10,
	       MLXSW_REG_PXXX_TCAM_REGION_INFO_LEN);
MLXSW_ITEM_BUF(reg, ptce3, flex2_key_blocks, 0x20,
	       MLXSW_REG_PTCEX_FLEX_KEY_BLOCKS_LEN);
MLXSW_ITEM32(reg, ptce3, erp_id, 0x80, 0, 4);
MLXSW_ITEM32(reg, ptce3, delta_start, 0x84, 0, 10);
MLXSW_ITEM32(reg, ptce3, delta_mask, 0x88, 16, 8);
MLXSW_ITEM32(reg, ptce3, delta_value, 0x88, 0, 8);
MLXSW_ITEM_BIT_ARRAY(reg, ptce3, prune_vector, 0x90, 4, 1);
MLXSW_ITEM32(reg, ptce3, prune_ctcam, 0x94, 31, 1);
MLXSW_ITEM32(reg, ptce3, large_exists, 0x98, 31, 1);
MLXSW_ITEM32(reg, ptce3, large_entry_key_id, 0x98, 0, 24);
MLXSW_ITEM32(reg, ptce3, action_pointer, 0xA0, 0, 24);
static inline void mlxsw_reg_ptce3_pack(char *payload, bool valid,
					enum mlxsw_reg_ptce3_op op,
					u32 priority,
					const char *tcam_region_info,
					const char *key, u8 erp_id,
					u16 delta_start, u8 delta_mask,
					u8 delta_value, bool large_exists,
					u32 lkey_id, u32 action_pointer)
{
	MLXSW_REG_ZERO(ptce3, payload);
	mlxsw_reg_ptce3_v_set(payload, valid);
	mlxsw_reg_ptce3_op_set(payload, op);
	mlxsw_reg_ptce3_priority_set(payload, priority);
	mlxsw_reg_ptce3_tcam_region_info_memcpy_to(payload, tcam_region_info);
	mlxsw_reg_ptce3_flex2_key_blocks_memcpy_to(payload, key);
	mlxsw_reg_ptce3_erp_id_set(payload, erp_id);
	mlxsw_reg_ptce3_delta_start_set(payload, delta_start);
	mlxsw_reg_ptce3_delta_mask_set(payload, delta_mask);
	mlxsw_reg_ptce3_delta_value_set(payload, delta_value);
	mlxsw_reg_ptce3_large_exists_set(payload, large_exists);
	mlxsw_reg_ptce3_large_entry_key_id_set(payload, lkey_id);
	mlxsw_reg_ptce3_action_pointer_set(payload, action_pointer);
}
#define MLXSW_REG_PERCR_ID 0x302A
#define MLXSW_REG_PERCR_LEN 0x80
MLXSW_REG_DEFINE(percr, MLXSW_REG_PERCR_ID, MLXSW_REG_PERCR_LEN);
MLXSW_ITEM32(reg, percr, region_id, 0x00, 0, 16);
MLXSW_ITEM32(reg, percr, atcam_ignore_prune, 0x04, 25, 1);
MLXSW_ITEM32(reg, percr, ctcam_ignore_prune, 0x04, 24, 1);
MLXSW_ITEM32(reg, percr, bf_bypass, 0x04, 16, 1);
MLXSW_ITEM_BUF(reg, percr, master_mask, 0x20, 96);
static inline void mlxsw_reg_percr_pack(char *payload, u16 region_id)
{
	MLXSW_REG_ZERO(percr, payload);
	mlxsw_reg_percr_region_id_set(payload, region_id);
	mlxsw_reg_percr_atcam_ignore_prune_set(payload, false);
	mlxsw_reg_percr_ctcam_ignore_prune_set(payload, false);
	mlxsw_reg_percr_bf_bypass_set(payload, false);
}
#define MLXSW_REG_PERERP_ID 0x302B
#define MLXSW_REG_PERERP_LEN 0x1C
MLXSW_REG_DEFINE(pererp, MLXSW_REG_PERERP_ID, MLXSW_REG_PERERP_LEN);
MLXSW_ITEM32(reg, pererp, region_id, 0x00, 0, 16);
MLXSW_ITEM32(reg, pererp, ctcam_le, 0x04, 28, 1);
MLXSW_ITEM32(reg, pererp, erpt_pointer_valid, 0x10, 31, 1);
MLXSW_ITEM32(reg, pererp, erpt_bank_pointer, 0x10, 16, 4);
MLXSW_ITEM32(reg, pererp, erpt_pointer, 0x10, 0, 8);
MLXSW_ITEM_BIT_ARRAY(reg, pererp, erpt_vector, 0x14, 4, 1);
MLXSW_ITEM32(reg, pererp, master_rp_id, 0x18, 0, 4);
static inline void mlxsw_reg_pererp_erp_vector_pack(char *payload,
						    unsigned long *erp_vector,
						    unsigned long size)
{
	unsigned long bit;
	for_each_set_bit(bit, erp_vector, size)
		mlxsw_reg_pererp_erpt_vector_set(payload, bit, true);
}
static inline void mlxsw_reg_pererp_pack(char *payload, u16 region_id,
					 bool ctcam_le, bool erpt_pointer_valid,
					 u8 erpt_bank_pointer, u8 erpt_pointer,
					 u8 master_rp_id)
{
	MLXSW_REG_ZERO(pererp, payload);
	mlxsw_reg_pererp_region_id_set(payload, region_id);
	mlxsw_reg_pererp_ctcam_le_set(payload, ctcam_le);
	mlxsw_reg_pererp_erpt_pointer_valid_set(payload, erpt_pointer_valid);
	mlxsw_reg_pererp_erpt_bank_pointer_set(payload, erpt_bank_pointer);
	mlxsw_reg_pererp_erpt_pointer_set(payload, erpt_pointer);
	mlxsw_reg_pererp_master_rp_id_set(payload, master_rp_id);
}
#define MLXSW_REG_PEABFE_ID 0x3022
#define MLXSW_REG_PEABFE_BASE_LEN 0x10
#define MLXSW_REG_PEABFE_BF_REC_LEN 0x4
#define MLXSW_REG_PEABFE_BF_REC_MAX_COUNT 256
#define MLXSW_REG_PEABFE_LEN (MLXSW_REG_PEABFE_BASE_LEN + \
			      MLXSW_REG_PEABFE_BF_REC_LEN * \
			      MLXSW_REG_PEABFE_BF_REC_MAX_COUNT)
MLXSW_REG_DEFINE(peabfe, MLXSW_REG_PEABFE_ID, MLXSW_REG_PEABFE_LEN);
MLXSW_ITEM32(reg, peabfe, size, 0x00, 0, 9);
MLXSW_ITEM32_INDEXED(reg, peabfe, bf_entry_state,
		     MLXSW_REG_PEABFE_BASE_LEN,	31, 1,
		     MLXSW_REG_PEABFE_BF_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, peabfe, bf_entry_bank,
		     MLXSW_REG_PEABFE_BASE_LEN,	24, 4,
		     MLXSW_REG_PEABFE_BF_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, peabfe, bf_entry_index,
		     MLXSW_REG_PEABFE_BASE_LEN,	0, 24,
		     MLXSW_REG_PEABFE_BF_REC_LEN, 0x00, false);
static inline void mlxsw_reg_peabfe_pack(char *payload)
{
	MLXSW_REG_ZERO(peabfe, payload);
}
static inline void mlxsw_reg_peabfe_rec_pack(char *payload, int rec_index,
					     u8 state, u8 bank, u32 bf_index)
{
	u8 num_rec = mlxsw_reg_peabfe_size_get(payload);
	if (rec_index >= num_rec)
		mlxsw_reg_peabfe_size_set(payload, rec_index + 1);
	mlxsw_reg_peabfe_bf_entry_state_set(payload, rec_index, state);
	mlxsw_reg_peabfe_bf_entry_bank_set(payload, rec_index, bank);
	mlxsw_reg_peabfe_bf_entry_index_set(payload, rec_index, bf_index);
}
#define MLXSW_REG_IEDR_ID 0x3804
#define MLXSW_REG_IEDR_BASE_LEN 0x10  
#define MLXSW_REG_IEDR_REC_LEN 0x8  
#define MLXSW_REG_IEDR_REC_MAX_COUNT 64
#define MLXSW_REG_IEDR_LEN (MLXSW_REG_IEDR_BASE_LEN +	\
			    MLXSW_REG_IEDR_REC_LEN *	\
			    MLXSW_REG_IEDR_REC_MAX_COUNT)
MLXSW_REG_DEFINE(iedr, MLXSW_REG_IEDR_ID, MLXSW_REG_IEDR_LEN);
MLXSW_ITEM32(reg, iedr, num_rec, 0x00, 0, 8);
MLXSW_ITEM32_INDEXED(reg, iedr, rec_type, MLXSW_REG_IEDR_BASE_LEN, 24, 8,
		     MLXSW_REG_IEDR_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, iedr, rec_size, MLXSW_REG_IEDR_BASE_LEN, 0, 13,
		     MLXSW_REG_IEDR_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, iedr, rec_index_start, MLXSW_REG_IEDR_BASE_LEN, 0, 24,
		     MLXSW_REG_IEDR_REC_LEN, 0x04, false);
static inline void mlxsw_reg_iedr_pack(char *payload)
{
	MLXSW_REG_ZERO(iedr, payload);
}
static inline void mlxsw_reg_iedr_rec_pack(char *payload, int rec_index,
					   u8 rec_type, u16 rec_size,
					   u32 rec_index_start)
{
	u8 num_rec = mlxsw_reg_iedr_num_rec_get(payload);
	if (rec_index >= num_rec)
		mlxsw_reg_iedr_num_rec_set(payload, rec_index + 1);
	mlxsw_reg_iedr_rec_type_set(payload, rec_index, rec_type);
	mlxsw_reg_iedr_rec_size_set(payload, rec_index, rec_size);
	mlxsw_reg_iedr_rec_index_start_set(payload, rec_index, rec_index_start);
}
#define MLXSW_REG_QPTS_ID 0x4002
#define MLXSW_REG_QPTS_LEN 0x8
MLXSW_REG_DEFINE(qpts, MLXSW_REG_QPTS_ID, MLXSW_REG_QPTS_LEN);
MLXSW_ITEM32_LP(reg, qpts, 0x00, 16, 0x00, 12);
enum mlxsw_reg_qpts_trust_state {
	MLXSW_REG_QPTS_TRUST_STATE_PCP = 1,
	MLXSW_REG_QPTS_TRUST_STATE_DSCP = 2,  
};
MLXSW_ITEM32(reg, qpts, trust_state, 0x04, 0, 3);
static inline void mlxsw_reg_qpts_pack(char *payload, u16 local_port,
				       enum mlxsw_reg_qpts_trust_state ts)
{
	MLXSW_REG_ZERO(qpts, payload);
	mlxsw_reg_qpts_local_port_set(payload, local_port);
	mlxsw_reg_qpts_trust_state_set(payload, ts);
}
#define MLXSW_REG_QPCR_ID 0x4004
#define MLXSW_REG_QPCR_LEN 0x28
MLXSW_REG_DEFINE(qpcr, MLXSW_REG_QPCR_ID, MLXSW_REG_QPCR_LEN);
enum mlxsw_reg_qpcr_g {
	MLXSW_REG_QPCR_G_GLOBAL = 2,
	MLXSW_REG_QPCR_G_STORM_CONTROL = 3,
};
MLXSW_ITEM32(reg, qpcr, g, 0x00, 14, 2);
MLXSW_ITEM32(reg, qpcr, pid, 0x00, 0, 14);
MLXSW_ITEM32(reg, qpcr, clear_counter, 0x04, 31, 1);
MLXSW_ITEM32(reg, qpcr, color_aware, 0x04, 15, 1);
MLXSW_ITEM32(reg, qpcr, bytes, 0x04, 14, 1);
enum mlxsw_reg_qpcr_ir_units {
	MLXSW_REG_QPCR_IR_UNITS_M,
	MLXSW_REG_QPCR_IR_UNITS_K,
};
MLXSW_ITEM32(reg, qpcr, ir_units, 0x04, 12, 1);
enum mlxsw_reg_qpcr_rate_type {
	MLXSW_REG_QPCR_RATE_TYPE_SINGLE = 1,
	MLXSW_REG_QPCR_RATE_TYPE_DOUBLE = 2,
};
MLXSW_ITEM32(reg, qpcr, rate_type, 0x04, 8, 2);
MLXSW_ITEM32(reg, qpcr, cbs, 0x08, 24, 6);
MLXSW_ITEM32(reg, qpcr, cir, 0x0C, 0, 32);
MLXSW_ITEM32(reg, qpcr, eir, 0x10, 0, 32);
#define MLXSW_REG_QPCR_DOUBLE_RATE_ACTION 2
MLXSW_ITEM32(reg, qpcr, exceed_action, 0x14, 0, 4);
enum mlxsw_reg_qpcr_action {
	MLXSW_REG_QPCR_ACTION_DISCARD = 1,
	MLXSW_REG_QPCR_ACTION_FORWARD = 2,
};
MLXSW_ITEM32(reg, qpcr, violate_action, 0x18, 0, 4);
MLXSW_ITEM64(reg, qpcr, violate_count, 0x20, 0, 64);
#define MLXSW_REG_QPCR_LOWEST_CIR	1
#define MLXSW_REG_QPCR_HIGHEST_CIR	(2 * 1000 * 1000 * 1000)  
#define MLXSW_REG_QPCR_LOWEST_CBS	4
#define MLXSW_REG_QPCR_HIGHEST_CBS	24
#define MLXSW_REG_QPCR_LOWEST_CIR_BITS		1024  
#define MLXSW_REG_QPCR_HIGHEST_CIR_BITS		2000000000000ULL  
#define MLXSW_REG_QPCR_LOWEST_CBS_BITS_SP1	4
#define MLXSW_REG_QPCR_LOWEST_CBS_BITS_SP2	4
#define MLXSW_REG_QPCR_HIGHEST_CBS_BITS_SP1	25
#define MLXSW_REG_QPCR_HIGHEST_CBS_BITS_SP2	31
static inline void mlxsw_reg_qpcr_pack(char *payload, u16 pid,
				       enum mlxsw_reg_qpcr_ir_units ir_units,
				       bool bytes, u32 cir, u16 cbs)
{
	MLXSW_REG_ZERO(qpcr, payload);
	mlxsw_reg_qpcr_pid_set(payload, pid);
	mlxsw_reg_qpcr_g_set(payload, MLXSW_REG_QPCR_G_GLOBAL);
	mlxsw_reg_qpcr_rate_type_set(payload, MLXSW_REG_QPCR_RATE_TYPE_SINGLE);
	mlxsw_reg_qpcr_violate_action_set(payload,
					  MLXSW_REG_QPCR_ACTION_DISCARD);
	mlxsw_reg_qpcr_cir_set(payload, cir);
	mlxsw_reg_qpcr_ir_units_set(payload, ir_units);
	mlxsw_reg_qpcr_bytes_set(payload, bytes);
	mlxsw_reg_qpcr_cbs_set(payload, cbs);
}
#define MLXSW_REG_QTCT_ID 0x400A
#define MLXSW_REG_QTCT_LEN 0x08
MLXSW_REG_DEFINE(qtct, MLXSW_REG_QTCT_ID, MLXSW_REG_QTCT_LEN);
MLXSW_ITEM32_LP(reg, qtct, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, qtct, sub_port, 0x00, 8, 8);
MLXSW_ITEM32(reg, qtct, switch_prio, 0x00, 0, 4);
MLXSW_ITEM32(reg, qtct, tclass, 0x04, 0, 4);
static inline void mlxsw_reg_qtct_pack(char *payload, u16 local_port,
				       u8 switch_prio, u8 tclass)
{
	MLXSW_REG_ZERO(qtct, payload);
	mlxsw_reg_qtct_local_port_set(payload, local_port);
	mlxsw_reg_qtct_switch_prio_set(payload, switch_prio);
	mlxsw_reg_qtct_tclass_set(payload, tclass);
}
#define MLXSW_REG_QEEC_ID 0x400D
#define MLXSW_REG_QEEC_LEN 0x20
MLXSW_REG_DEFINE(qeec, MLXSW_REG_QEEC_ID, MLXSW_REG_QEEC_LEN);
MLXSW_ITEM32_LP(reg, qeec, 0x00, 16, 0x00, 12);
enum mlxsw_reg_qeec_hr {
	MLXSW_REG_QEEC_HR_PORT,
	MLXSW_REG_QEEC_HR_GROUP,
	MLXSW_REG_QEEC_HR_SUBGROUP,
	MLXSW_REG_QEEC_HR_TC,
};
MLXSW_ITEM32(reg, qeec, element_hierarchy, 0x04, 16, 4);
MLXSW_ITEM32(reg, qeec, element_index, 0x04, 0, 8);
MLXSW_ITEM32(reg, qeec, next_element_index, 0x08, 0, 8);
MLXSW_ITEM32(reg, qeec, mise, 0x0C, 31, 1);
MLXSW_ITEM32(reg, qeec, ptps, 0x0C, 29, 1);
enum {
	MLXSW_REG_QEEC_BYTES_MODE,
	MLXSW_REG_QEEC_PACKETS_MODE,
};
MLXSW_ITEM32(reg, qeec, pb, 0x0C, 28, 1);
#define MLXSW_REG_QEEC_MIS_MIN	200000		 
MLXSW_ITEM32(reg, qeec, min_shaper_rate, 0x0C, 0, 28);
MLXSW_ITEM32(reg, qeec, mase, 0x10, 31, 1);
#define MLXSW_REG_QEEC_MAS_DIS	((1u << 31) - 1)	 
MLXSW_ITEM32(reg, qeec, max_shaper_rate, 0x10, 0, 31);
MLXSW_ITEM32(reg, qeec, de, 0x18, 31, 1);
MLXSW_ITEM32(reg, qeec, dwrr, 0x18, 15, 1);
MLXSW_ITEM32(reg, qeec, dwrr_weight, 0x18, 0, 8);
MLXSW_ITEM32(reg, qeec, max_shaper_bs, 0x1C, 0, 6);
#define MLXSW_REG_QEEC_HIGHEST_SHAPER_BS	25
#define MLXSW_REG_QEEC_LOWEST_SHAPER_BS_SP1	5
#define MLXSW_REG_QEEC_LOWEST_SHAPER_BS_SP2	11
#define MLXSW_REG_QEEC_LOWEST_SHAPER_BS_SP3	11
#define MLXSW_REG_QEEC_LOWEST_SHAPER_BS_SP4	11
static inline void mlxsw_reg_qeec_pack(char *payload, u16 local_port,
				       enum mlxsw_reg_qeec_hr hr, u8 index,
				       u8 next_index)
{
	MLXSW_REG_ZERO(qeec, payload);
	mlxsw_reg_qeec_local_port_set(payload, local_port);
	mlxsw_reg_qeec_element_hierarchy_set(payload, hr);
	mlxsw_reg_qeec_element_index_set(payload, index);
	mlxsw_reg_qeec_next_element_index_set(payload, next_index);
}
static inline void mlxsw_reg_qeec_ptps_pack(char *payload, u16 local_port,
					    bool ptps)
{
	MLXSW_REG_ZERO(qeec, payload);
	mlxsw_reg_qeec_local_port_set(payload, local_port);
	mlxsw_reg_qeec_element_hierarchy_set(payload, MLXSW_REG_QEEC_HR_PORT);
	mlxsw_reg_qeec_ptps_set(payload, ptps);
}
#define MLXSW_REG_QRWE_ID 0x400F
#define MLXSW_REG_QRWE_LEN 0x08
MLXSW_REG_DEFINE(qrwe, MLXSW_REG_QRWE_ID, MLXSW_REG_QRWE_LEN);
MLXSW_ITEM32_LP(reg, qrwe, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, qrwe, dscp, 0x04, 1, 1);
MLXSW_ITEM32(reg, qrwe, pcp, 0x04, 0, 1);
static inline void mlxsw_reg_qrwe_pack(char *payload, u16 local_port,
				       bool rewrite_pcp, bool rewrite_dscp)
{
	MLXSW_REG_ZERO(qrwe, payload);
	mlxsw_reg_qrwe_local_port_set(payload, local_port);
	mlxsw_reg_qrwe_pcp_set(payload, rewrite_pcp);
	mlxsw_reg_qrwe_dscp_set(payload, rewrite_dscp);
}
#define MLXSW_REG_QPDSM_ID 0x4011
#define MLXSW_REG_QPDSM_BASE_LEN 0x04  
#define MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN 0x4  
#define MLXSW_REG_QPDSM_PRIO_ENTRY_REC_MAX_COUNT 16
#define MLXSW_REG_QPDSM_LEN (MLXSW_REG_QPDSM_BASE_LEN +			\
			     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN *	\
			     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_MAX_COUNT)
MLXSW_REG_DEFINE(qpdsm, MLXSW_REG_QPDSM_ID, MLXSW_REG_QPDSM_LEN);
MLXSW_ITEM32_LP(reg, qpdsm, 0x00, 16, 0x00, 12);
MLXSW_ITEM32_INDEXED(reg, qpdsm, prio_entry_color0_e,
		     MLXSW_REG_QPDSM_BASE_LEN, 31, 1,
		     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, qpdsm, prio_entry_color0_dscp,
		     MLXSW_REG_QPDSM_BASE_LEN, 24, 6,
		     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, qpdsm, prio_entry_color1_e,
		     MLXSW_REG_QPDSM_BASE_LEN, 23, 1,
		     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, qpdsm, prio_entry_color1_dscp,
		     MLXSW_REG_QPDSM_BASE_LEN, 16, 6,
		     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, qpdsm, prio_entry_color2_e,
		     MLXSW_REG_QPDSM_BASE_LEN, 15, 1,
		     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, qpdsm, prio_entry_color2_dscp,
		     MLXSW_REG_QPDSM_BASE_LEN, 8, 6,
		     MLXSW_REG_QPDSM_PRIO_ENTRY_REC_LEN, 0x00, false);
static inline void mlxsw_reg_qpdsm_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(qpdsm, payload);
	mlxsw_reg_qpdsm_local_port_set(payload, local_port);
}
static inline void
mlxsw_reg_qpdsm_prio_pack(char *payload, unsigned short prio, u8 dscp)
{
	mlxsw_reg_qpdsm_prio_entry_color0_e_set(payload, prio, 1);
	mlxsw_reg_qpdsm_prio_entry_color0_dscp_set(payload, prio, dscp);
	mlxsw_reg_qpdsm_prio_entry_color1_e_set(payload, prio, 1);
	mlxsw_reg_qpdsm_prio_entry_color1_dscp_set(payload, prio, dscp);
	mlxsw_reg_qpdsm_prio_entry_color2_e_set(payload, prio, 1);
	mlxsw_reg_qpdsm_prio_entry_color2_dscp_set(payload, prio, dscp);
}
#define MLXSW_REG_QPDP_ID 0x4007
#define MLXSW_REG_QPDP_LEN 0x8
MLXSW_REG_DEFINE(qpdp, MLXSW_REG_QPDP_ID, MLXSW_REG_QPDP_LEN);
MLXSW_ITEM32_LP(reg, qpdp, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, qpdp, switch_prio, 0x04, 0, 4);
static inline void mlxsw_reg_qpdp_pack(char *payload, u16 local_port,
				       u8 switch_prio)
{
	MLXSW_REG_ZERO(qpdp, payload);
	mlxsw_reg_qpdp_local_port_set(payload, local_port);
	mlxsw_reg_qpdp_switch_prio_set(payload, switch_prio);
}
#define MLXSW_REG_QPDPM_ID 0x4013
#define MLXSW_REG_QPDPM_BASE_LEN 0x4  
#define MLXSW_REG_QPDPM_DSCP_ENTRY_REC_LEN 0x2  
#define MLXSW_REG_QPDPM_DSCP_ENTRY_REC_MAX_COUNT 64
#define MLXSW_REG_QPDPM_LEN (MLXSW_REG_QPDPM_BASE_LEN +			\
			     MLXSW_REG_QPDPM_DSCP_ENTRY_REC_LEN *	\
			     MLXSW_REG_QPDPM_DSCP_ENTRY_REC_MAX_COUNT)
MLXSW_REG_DEFINE(qpdpm, MLXSW_REG_QPDPM_ID, MLXSW_REG_QPDPM_LEN);
MLXSW_ITEM32_LP(reg, qpdpm, 0x00, 16, 0x00, 12);
MLXSW_ITEM16_INDEXED(reg, qpdpm, dscp_entry_e, MLXSW_REG_QPDPM_BASE_LEN, 15, 1,
		     MLXSW_REG_QPDPM_DSCP_ENTRY_REC_LEN, 0x00, false);
MLXSW_ITEM16_INDEXED(reg, qpdpm, dscp_entry_prio,
		     MLXSW_REG_QPDPM_BASE_LEN, 0, 4,
		     MLXSW_REG_QPDPM_DSCP_ENTRY_REC_LEN, 0x00, false);
static inline void mlxsw_reg_qpdpm_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(qpdpm, payload);
	mlxsw_reg_qpdpm_local_port_set(payload, local_port);
}
static inline void
mlxsw_reg_qpdpm_dscp_pack(char *payload, unsigned short dscp, u8 prio)
{
	mlxsw_reg_qpdpm_dscp_entry_e_set(payload, dscp, 1);
	mlxsw_reg_qpdpm_dscp_entry_prio_set(payload, dscp, prio);
}
#define MLXSW_REG_QTCTM_ID 0x401A
#define MLXSW_REG_QTCTM_LEN 0x08
MLXSW_REG_DEFINE(qtctm, MLXSW_REG_QTCTM_ID, MLXSW_REG_QTCTM_LEN);
MLXSW_ITEM32_LP(reg, qtctm, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, qtctm, mc, 0x04, 0, 1);
static inline void
mlxsw_reg_qtctm_pack(char *payload, u16 local_port, bool mc)
{
	MLXSW_REG_ZERO(qtctm, payload);
	mlxsw_reg_qtctm_local_port_set(payload, local_port);
	mlxsw_reg_qtctm_mc_set(payload, mc);
}
#define MLXSW_REG_QPSC_ID 0x401B
#define MLXSW_REG_QPSC_LEN 0x28
MLXSW_REG_DEFINE(qpsc, MLXSW_REG_QPSC_ID, MLXSW_REG_QPSC_LEN);
enum mlxsw_reg_qpsc_port_speed {
	MLXSW_REG_QPSC_PORT_SPEED_100M,
	MLXSW_REG_QPSC_PORT_SPEED_1G,
	MLXSW_REG_QPSC_PORT_SPEED_10G,
	MLXSW_REG_QPSC_PORT_SPEED_25G,
};
MLXSW_ITEM32(reg, qpsc, port_speed, 0x00, 0, 4);
MLXSW_ITEM32(reg, qpsc, shaper_time_exp, 0x04, 16, 4);
MLXSW_ITEM32(reg, qpsc, shaper_time_mantissa, 0x04, 0, 5);
MLXSW_ITEM32(reg, qpsc, shaper_inc, 0x08, 0, 5);
MLXSW_ITEM32(reg, qpsc, shaper_bs, 0x0C, 0, 6);
MLXSW_ITEM32(reg, qpsc, ptsc_we, 0x10, 31, 1);
MLXSW_ITEM32(reg, qpsc, port_to_shaper_credits, 0x10, 0, 8);
MLXSW_ITEM32(reg, qpsc, ing_timestamp_inc, 0x20, 0, 32);
MLXSW_ITEM32(reg, qpsc, egr_timestamp_inc, 0x24, 0, 32);
static inline void
mlxsw_reg_qpsc_pack(char *payload, enum mlxsw_reg_qpsc_port_speed port_speed,
		    u8 shaper_time_exp, u8 shaper_time_mantissa, u8 shaper_inc,
		    u8 shaper_bs, u8 port_to_shaper_credits,
		    int ing_timestamp_inc, int egr_timestamp_inc)
{
	MLXSW_REG_ZERO(qpsc, payload);
	mlxsw_reg_qpsc_port_speed_set(payload, port_speed);
	mlxsw_reg_qpsc_shaper_time_exp_set(payload, shaper_time_exp);
	mlxsw_reg_qpsc_shaper_time_mantissa_set(payload, shaper_time_mantissa);
	mlxsw_reg_qpsc_shaper_inc_set(payload, shaper_inc);
	mlxsw_reg_qpsc_shaper_bs_set(payload, shaper_bs);
	mlxsw_reg_qpsc_ptsc_we_set(payload, true);
	mlxsw_reg_qpsc_port_to_shaper_credits_set(payload, port_to_shaper_credits);
	mlxsw_reg_qpsc_ing_timestamp_inc_set(payload, ing_timestamp_inc);
	mlxsw_reg_qpsc_egr_timestamp_inc_set(payload, egr_timestamp_inc);
}
#define MLXSW_REG_PMLP_ID 0x5002
#define MLXSW_REG_PMLP_LEN 0x40
MLXSW_REG_DEFINE(pmlp, MLXSW_REG_PMLP_ID, MLXSW_REG_PMLP_LEN);
MLXSW_ITEM32(reg, pmlp, rxtx, 0x00, 31, 1);
MLXSW_ITEM32_LP(reg, pmlp, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pmlp, width, 0x00, 0, 8);
MLXSW_ITEM32_INDEXED(reg, pmlp, module, 0x04, 0, 8, 0x04, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, pmlp, slot_index, 0x04, 8, 4, 0x04, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, pmlp, tx_lane, 0x04, 16, 4, 0x04, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, pmlp, rx_lane, 0x04, 24, 4, 0x04, 0x00, false);
static inline void mlxsw_reg_pmlp_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(pmlp, payload);
	mlxsw_reg_pmlp_local_port_set(payload, local_port);
}
#define MLXSW_REG_PMTU_ID 0x5003
#define MLXSW_REG_PMTU_LEN 0x10
MLXSW_REG_DEFINE(pmtu, MLXSW_REG_PMTU_ID, MLXSW_REG_PMTU_LEN);
MLXSW_ITEM32_LP(reg, pmtu, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pmtu, max_mtu, 0x04, 16, 16);
MLXSW_ITEM32(reg, pmtu, admin_mtu, 0x08, 16, 16);
MLXSW_ITEM32(reg, pmtu, oper_mtu, 0x0C, 16, 16);
static inline void mlxsw_reg_pmtu_pack(char *payload, u16 local_port,
				       u16 new_mtu)
{
	MLXSW_REG_ZERO(pmtu, payload);
	mlxsw_reg_pmtu_local_port_set(payload, local_port);
	mlxsw_reg_pmtu_max_mtu_set(payload, 0);
	mlxsw_reg_pmtu_admin_mtu_set(payload, new_mtu);
	mlxsw_reg_pmtu_oper_mtu_set(payload, 0);
}
#define MLXSW_REG_PTYS_ID 0x5004
#define MLXSW_REG_PTYS_LEN 0x40
MLXSW_REG_DEFINE(ptys, MLXSW_REG_PTYS_ID, MLXSW_REG_PTYS_LEN);
MLXSW_ITEM32(reg, ptys, an_disable_admin, 0x00, 30, 1);
MLXSW_ITEM32_LP(reg, ptys, 0x00, 16, 0x00, 12);
#define MLXSW_REG_PTYS_PROTO_MASK_IB	BIT(0)
#define MLXSW_REG_PTYS_PROTO_MASK_ETH	BIT(2)
MLXSW_ITEM32(reg, ptys, proto_mask, 0x00, 0, 3);
enum {
	MLXSW_REG_PTYS_AN_STATUS_NA,
	MLXSW_REG_PTYS_AN_STATUS_OK,
	MLXSW_REG_PTYS_AN_STATUS_FAIL,
};
MLXSW_ITEM32(reg, ptys, an_status, 0x04, 28, 4);
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_SGMII_100M				BIT(0)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_1000BASE_X_SGMII			BIT(1)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_5GBASE_R				BIT(3)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_XFI_XAUI_1_10G			BIT(4)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_XLAUI_4_XLPPI_4_40G		BIT(5)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_25GAUI_1_25GBASE_CR_KR		BIT(6)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_50GAUI_2_LAUI_2_50GBASE_CR2_KR2	BIT(7)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_50GAUI_1_LAUI_1_50GBASE_CR_KR	BIT(8)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_CAUI_4_100GBASE_CR4_KR4		BIT(9)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_100GAUI_2_100GBASE_CR2_KR2		BIT(10)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_200GAUI_4_200GBASE_CR4_KR4		BIT(12)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_400GAUI_8				BIT(15)
#define MLXSW_REG_PTYS_EXT_ETH_SPEED_800GAUI_8				BIT(19)
MLXSW_ITEM32(reg, ptys, ext_eth_proto_cap, 0x08, 0, 32);
#define MLXSW_REG_PTYS_ETH_SPEED_SGMII			BIT(0)
#define MLXSW_REG_PTYS_ETH_SPEED_1000BASE_KX		BIT(1)
#define MLXSW_REG_PTYS_ETH_SPEED_10GBASE_CX4		BIT(2)
#define MLXSW_REG_PTYS_ETH_SPEED_10GBASE_KX4		BIT(3)
#define MLXSW_REG_PTYS_ETH_SPEED_10GBASE_KR		BIT(4)
#define MLXSW_REG_PTYS_ETH_SPEED_40GBASE_CR4		BIT(6)
#define MLXSW_REG_PTYS_ETH_SPEED_40GBASE_KR4		BIT(7)
#define MLXSW_REG_PTYS_ETH_SPEED_10GBASE_CR		BIT(12)
#define MLXSW_REG_PTYS_ETH_SPEED_10GBASE_SR		BIT(13)
#define MLXSW_REG_PTYS_ETH_SPEED_10GBASE_ER_LR		BIT(14)
#define MLXSW_REG_PTYS_ETH_SPEED_40GBASE_SR4		BIT(15)
#define MLXSW_REG_PTYS_ETH_SPEED_40GBASE_LR4_ER4	BIT(16)
#define MLXSW_REG_PTYS_ETH_SPEED_50GBASE_SR2		BIT(18)
#define MLXSW_REG_PTYS_ETH_SPEED_50GBASE_KR4		BIT(19)
#define MLXSW_REG_PTYS_ETH_SPEED_100GBASE_CR4		BIT(20)
#define MLXSW_REG_PTYS_ETH_SPEED_100GBASE_SR4		BIT(21)
#define MLXSW_REG_PTYS_ETH_SPEED_100GBASE_KR4		BIT(22)
#define MLXSW_REG_PTYS_ETH_SPEED_100GBASE_LR4_ER4	BIT(23)
#define MLXSW_REG_PTYS_ETH_SPEED_100BASE_T		BIT(24)
#define MLXSW_REG_PTYS_ETH_SPEED_1000BASE_T		BIT(25)
#define MLXSW_REG_PTYS_ETH_SPEED_25GBASE_CR		BIT(27)
#define MLXSW_REG_PTYS_ETH_SPEED_25GBASE_KR		BIT(28)
#define MLXSW_REG_PTYS_ETH_SPEED_25GBASE_SR		BIT(29)
#define MLXSW_REG_PTYS_ETH_SPEED_50GBASE_CR2		BIT(30)
#define MLXSW_REG_PTYS_ETH_SPEED_50GBASE_KR2		BIT(31)
MLXSW_ITEM32(reg, ptys, eth_proto_cap, 0x0C, 0, 32);
MLXSW_ITEM32(reg, ptys, ext_eth_proto_admin, 0x14, 0, 32);
MLXSW_ITEM32(reg, ptys, eth_proto_admin, 0x18, 0, 32);
MLXSW_ITEM32(reg, ptys, ext_eth_proto_oper, 0x20, 0, 32);
MLXSW_ITEM32(reg, ptys, eth_proto_oper, 0x24, 0, 32);
enum mlxsw_reg_ptys_connector_type {
	MLXSW_REG_PTYS_CONNECTOR_TYPE_UNKNOWN_OR_NO_CONNECTOR,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_NONE,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_TP,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_AUI,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_BNC,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_MII,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_FIBRE,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_DA,
	MLXSW_REG_PTYS_CONNECTOR_TYPE_PORT_OTHER,
};
MLXSW_ITEM32(reg, ptys, connector_type, 0x2C, 0, 4);
static inline void mlxsw_reg_ptys_eth_pack(char *payload, u16 local_port,
					   u32 proto_admin, bool autoneg)
{
	MLXSW_REG_ZERO(ptys, payload);
	mlxsw_reg_ptys_local_port_set(payload, local_port);
	mlxsw_reg_ptys_proto_mask_set(payload, MLXSW_REG_PTYS_PROTO_MASK_ETH);
	mlxsw_reg_ptys_eth_proto_admin_set(payload, proto_admin);
	mlxsw_reg_ptys_an_disable_admin_set(payload, !autoneg);
}
static inline void mlxsw_reg_ptys_ext_eth_pack(char *payload, u16 local_port,
					       u32 proto_admin, bool autoneg)
{
	MLXSW_REG_ZERO(ptys, payload);
	mlxsw_reg_ptys_local_port_set(payload, local_port);
	mlxsw_reg_ptys_proto_mask_set(payload, MLXSW_REG_PTYS_PROTO_MASK_ETH);
	mlxsw_reg_ptys_ext_eth_proto_admin_set(payload, proto_admin);
	mlxsw_reg_ptys_an_disable_admin_set(payload, !autoneg);
}
static inline void mlxsw_reg_ptys_eth_unpack(char *payload,
					     u32 *p_eth_proto_cap,
					     u32 *p_eth_proto_admin,
					     u32 *p_eth_proto_oper)
{
	if (p_eth_proto_cap)
		*p_eth_proto_cap =
			mlxsw_reg_ptys_eth_proto_cap_get(payload);
	if (p_eth_proto_admin)
		*p_eth_proto_admin =
			mlxsw_reg_ptys_eth_proto_admin_get(payload);
	if (p_eth_proto_oper)
		*p_eth_proto_oper =
			mlxsw_reg_ptys_eth_proto_oper_get(payload);
}
static inline void mlxsw_reg_ptys_ext_eth_unpack(char *payload,
						 u32 *p_eth_proto_cap,
						 u32 *p_eth_proto_admin,
						 u32 *p_eth_proto_oper)
{
	if (p_eth_proto_cap)
		*p_eth_proto_cap =
			mlxsw_reg_ptys_ext_eth_proto_cap_get(payload);
	if (p_eth_proto_admin)
		*p_eth_proto_admin =
			mlxsw_reg_ptys_ext_eth_proto_admin_get(payload);
	if (p_eth_proto_oper)
		*p_eth_proto_oper =
			mlxsw_reg_ptys_ext_eth_proto_oper_get(payload);
}
#define MLXSW_REG_PPAD_ID 0x5005
#define MLXSW_REG_PPAD_LEN 0x10
MLXSW_REG_DEFINE(ppad, MLXSW_REG_PPAD_ID, MLXSW_REG_PPAD_LEN);
MLXSW_ITEM32(reg, ppad, single_base_mac, 0x00, 28, 1);
MLXSW_ITEM32_LP(reg, ppad, 0x00, 16, 0x00, 24);
MLXSW_ITEM_BUF(reg, ppad, mac, 0x02, 6);
static inline void mlxsw_reg_ppad_pack(char *payload, bool single_base_mac,
				       u16 local_port)
{
	MLXSW_REG_ZERO(ppad, payload);
	mlxsw_reg_ppad_single_base_mac_set(payload, !!single_base_mac);
	mlxsw_reg_ppad_local_port_set(payload, local_port);
}
#define MLXSW_REG_PAOS_ID 0x5006
#define MLXSW_REG_PAOS_LEN 0x10
MLXSW_REG_DEFINE(paos, MLXSW_REG_PAOS_ID, MLXSW_REG_PAOS_LEN);
MLXSW_ITEM32(reg, paos, swid, 0x00, 24, 8);
MLXSW_ITEM32_LP(reg, paos, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, paos, admin_status, 0x00, 8, 4);
MLXSW_ITEM32(reg, paos, oper_status, 0x00, 0, 4);
MLXSW_ITEM32(reg, paos, ase, 0x04, 31, 1);
MLXSW_ITEM32(reg, paos, ee, 0x04, 30, 1);
MLXSW_ITEM32(reg, paos, e, 0x04, 0, 2);
static inline void mlxsw_reg_paos_pack(char *payload, u16 local_port,
				       enum mlxsw_port_admin_status status)
{
	MLXSW_REG_ZERO(paos, payload);
	mlxsw_reg_paos_swid_set(payload, 0);
	mlxsw_reg_paos_local_port_set(payload, local_port);
	mlxsw_reg_paos_admin_status_set(payload, status);
	mlxsw_reg_paos_oper_status_set(payload, 0);
	mlxsw_reg_paos_ase_set(payload, 1);
	mlxsw_reg_paos_ee_set(payload, 1);
	mlxsw_reg_paos_e_set(payload, 1);
}
#define MLXSW_REG_PFCC_ID 0x5007
#define MLXSW_REG_PFCC_LEN 0x20
MLXSW_REG_DEFINE(pfcc, MLXSW_REG_PFCC_ID, MLXSW_REG_PFCC_LEN);
MLXSW_ITEM32_LP(reg, pfcc, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pfcc, pnat, 0x00, 14, 2);
MLXSW_ITEM32(reg, pfcc, shl_cap, 0x00, 1, 1);
MLXSW_ITEM32(reg, pfcc, shl_opr, 0x00, 0, 1);
MLXSW_ITEM32(reg, pfcc, ppan, 0x04, 28, 4);
MLXSW_ITEM32(reg, pfcc, prio_mask_tx, 0x04, 16, 8);
MLXSW_ITEM32(reg, pfcc, prio_mask_rx, 0x04, 0, 8);
MLXSW_ITEM32(reg, pfcc, pptx, 0x08, 31, 1);
MLXSW_ITEM32(reg, pfcc, aptx, 0x08, 30, 1);
MLXSW_ITEM32(reg, pfcc, pfctx, 0x08, 16, 8);
MLXSW_ITEM32(reg, pfcc, pprx, 0x0C, 31, 1);
MLXSW_ITEM32(reg, pfcc, aprx, 0x0C, 30, 1);
MLXSW_ITEM32(reg, pfcc, pfcrx, 0x0C, 16, 8);
#define MLXSW_REG_PFCC_ALL_PRIO 0xFF
static inline void mlxsw_reg_pfcc_prio_pack(char *payload, u8 pfc_en)
{
	mlxsw_reg_pfcc_prio_mask_tx_set(payload, MLXSW_REG_PFCC_ALL_PRIO);
	mlxsw_reg_pfcc_prio_mask_rx_set(payload, MLXSW_REG_PFCC_ALL_PRIO);
	mlxsw_reg_pfcc_pfctx_set(payload, pfc_en);
	mlxsw_reg_pfcc_pfcrx_set(payload, pfc_en);
}
static inline void mlxsw_reg_pfcc_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(pfcc, payload);
	mlxsw_reg_pfcc_local_port_set(payload, local_port);
}
#define MLXSW_REG_PPCNT_ID 0x5008
#define MLXSW_REG_PPCNT_LEN 0x100
#define MLXSW_REG_PPCNT_COUNTERS_OFFSET 0x08
MLXSW_REG_DEFINE(ppcnt, MLXSW_REG_PPCNT_ID, MLXSW_REG_PPCNT_LEN);
MLXSW_ITEM32(reg, ppcnt, swid, 0x00, 24, 8);
MLXSW_ITEM32_LP(reg, ppcnt, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, ppcnt, pnat, 0x00, 14, 2);
enum mlxsw_reg_ppcnt_grp {
	MLXSW_REG_PPCNT_IEEE_8023_CNT = 0x0,
	MLXSW_REG_PPCNT_RFC_2863_CNT = 0x1,
	MLXSW_REG_PPCNT_RFC_2819_CNT = 0x2,
	MLXSW_REG_PPCNT_RFC_3635_CNT = 0x3,
	MLXSW_REG_PPCNT_EXT_CNT = 0x5,
	MLXSW_REG_PPCNT_DISCARD_CNT = 0x6,
	MLXSW_REG_PPCNT_PRIO_CNT = 0x10,
	MLXSW_REG_PPCNT_TC_CNT = 0x11,
	MLXSW_REG_PPCNT_TC_CONG_CNT = 0x13,
};
MLXSW_ITEM32(reg, ppcnt, grp, 0x00, 0, 6);
MLXSW_ITEM32(reg, ppcnt, clr, 0x04, 31, 1);
MLXSW_ITEM32(reg, ppcnt, lp_gl, 0x04, 30, 1);
MLXSW_ITEM32(reg, ppcnt, prio_tc, 0x04, 0, 5);
MLXSW_ITEM64(reg, ppcnt, a_frames_transmitted_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x00, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_frames_received_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x08, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_frame_check_sequence_errors,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x10, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_alignment_errors,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x18, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_octets_transmitted_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x20, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_octets_received_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x28, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_multicast_frames_xmitted_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x30, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_broadcast_frames_xmitted_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x38, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_multicast_frames_received_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x40, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_broadcast_frames_received_ok,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x48, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_in_range_length_errors,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x50, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_out_of_range_length_field,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x58, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_frame_too_long_errors,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x60, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_symbol_error_during_carrier,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x68, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_mac_control_frames_transmitted,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x70, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_mac_control_frames_received,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x78, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_unsupported_opcodes_received,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x80, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_pause_mac_ctrl_frames_received,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x88, 0, 64);
MLXSW_ITEM64(reg, ppcnt, a_pause_mac_ctrl_frames_transmitted,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x90, 0, 64);
MLXSW_ITEM64(reg, ppcnt, if_in_discards,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x10, 0, 64);
MLXSW_ITEM64(reg, ppcnt, if_out_discards,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x38, 0, 64);
MLXSW_ITEM64(reg, ppcnt, if_out_errors,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x40, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_undersize_pkts,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x30, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_oversize_pkts,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x38, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_fragments,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x40, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts64octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x58, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts65to127octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x60, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts128to255octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x68, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts256to511octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x70, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts512to1023octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x78, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts1024to1518octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x80, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts1519to2047octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x88, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts2048to4095octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x90, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts4096to8191octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x98, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ether_stats_pkts8192to10239octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0xA0, 0, 64);
MLXSW_ITEM64(reg, ppcnt, dot3stats_fcs_errors,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x08, 0, 64);
MLXSW_ITEM64(reg, ppcnt, dot3stats_symbol_errors,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x60, 0, 64);
MLXSW_ITEM64(reg, ppcnt, dot3control_in_unknown_opcodes,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x68, 0, 64);
MLXSW_ITEM64(reg, ppcnt, dot3in_pause_frames,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x70, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ecn_marked,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x08, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ingress_general,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x00, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ingress_policy_engine,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x08, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ingress_vlan_membership,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x10, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ingress_tag_frame_type,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x18, 0, 64);
MLXSW_ITEM64(reg, ppcnt, egress_vlan_membership,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x20, 0, 64);
MLXSW_ITEM64(reg, ppcnt, loopback_filter,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x28, 0, 64);
MLXSW_ITEM64(reg, ppcnt, egress_general,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x30, 0, 64);
MLXSW_ITEM64(reg, ppcnt, egress_hoq,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x40, 0, 64);
MLXSW_ITEM64(reg, ppcnt, egress_policy_engine,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x50, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ingress_tx_link_down,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x58, 0, 64);
MLXSW_ITEM64(reg, ppcnt, egress_stp_filter,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x60, 0, 64);
MLXSW_ITEM64(reg, ppcnt, egress_sll,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x70, 0, 64);
MLXSW_ITEM64(reg, ppcnt, rx_octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x00, 0, 64);
MLXSW_ITEM64(reg, ppcnt, rx_frames,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x20, 0, 64);
MLXSW_ITEM64(reg, ppcnt, tx_octets,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x28, 0, 64);
MLXSW_ITEM64(reg, ppcnt, tx_frames,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x48, 0, 64);
MLXSW_ITEM64(reg, ppcnt, rx_pause,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x50, 0, 64);
MLXSW_ITEM64(reg, ppcnt, rx_pause_duration,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x58, 0, 64);
MLXSW_ITEM64(reg, ppcnt, tx_pause,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x60, 0, 64);
MLXSW_ITEM64(reg, ppcnt, tx_pause_duration,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x68, 0, 64);
MLXSW_ITEM64(reg, ppcnt, tx_pause_transition,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x70, 0, 64);
MLXSW_ITEM64(reg, ppcnt, tc_transmit_queue,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x00, 0, 64);
MLXSW_ITEM64(reg, ppcnt, tc_no_buffer_discard_uc,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x08, 0, 64);
MLXSW_ITEM64(reg, ppcnt, wred_discard,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x00, 0, 64);
MLXSW_ITEM64(reg, ppcnt, ecn_marked_tc,
	     MLXSW_REG_PPCNT_COUNTERS_OFFSET + 0x08, 0, 64);
static inline void mlxsw_reg_ppcnt_pack(char *payload, u16 local_port,
					enum mlxsw_reg_ppcnt_grp grp,
					u8 prio_tc)
{
	MLXSW_REG_ZERO(ppcnt, payload);
	mlxsw_reg_ppcnt_swid_set(payload, 0);
	mlxsw_reg_ppcnt_local_port_set(payload, local_port);
	mlxsw_reg_ppcnt_pnat_set(payload, 0);
	mlxsw_reg_ppcnt_grp_set(payload, grp);
	mlxsw_reg_ppcnt_clr_set(payload, 0);
	mlxsw_reg_ppcnt_lp_gl_set(payload, 1);
	mlxsw_reg_ppcnt_prio_tc_set(payload, prio_tc);
}
#define MLXSW_REG_PPTB_ID 0x500B
#define MLXSW_REG_PPTB_LEN 0x10
MLXSW_REG_DEFINE(pptb, MLXSW_REG_PPTB_ID, MLXSW_REG_PPTB_LEN);
enum {
	MLXSW_REG_PPTB_MM_UM,
	MLXSW_REG_PPTB_MM_UNICAST,
	MLXSW_REG_PPTB_MM_MULTICAST,
};
MLXSW_ITEM32(reg, pptb, mm, 0x00, 28, 2);
MLXSW_ITEM32_LP(reg, pptb, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pptb, um, 0x00, 8, 1);
MLXSW_ITEM32(reg, pptb, pm, 0x00, 0, 8);
MLXSW_ITEM_BIT_ARRAY(reg, pptb, prio_to_buff, 0x04, 0x04, 4);
MLXSW_ITEM32(reg, pptb, pm_msb, 0x08, 24, 8);
MLXSW_ITEM32(reg, pptb, untagged_buff, 0x08, 0, 4);
MLXSW_ITEM_BIT_ARRAY(reg, pptb, prio_to_buff_msb, 0x0C, 0x04, 4);
#define MLXSW_REG_PPTB_ALL_PRIO 0xFF
static inline void mlxsw_reg_pptb_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(pptb, payload);
	mlxsw_reg_pptb_mm_set(payload, MLXSW_REG_PPTB_MM_UM);
	mlxsw_reg_pptb_local_port_set(payload, local_port);
	mlxsw_reg_pptb_pm_set(payload, MLXSW_REG_PPTB_ALL_PRIO);
	mlxsw_reg_pptb_pm_msb_set(payload, MLXSW_REG_PPTB_ALL_PRIO);
}
static inline void mlxsw_reg_pptb_prio_to_buff_pack(char *payload, u8 prio,
						    u8 buff)
{
	mlxsw_reg_pptb_prio_to_buff_set(payload, prio, buff);
	mlxsw_reg_pptb_prio_to_buff_msb_set(payload, prio, buff);
}
#define MLXSW_REG_PBMC_ID 0x500C
#define MLXSW_REG_PBMC_LEN 0x6C
MLXSW_REG_DEFINE(pbmc, MLXSW_REG_PBMC_ID, MLXSW_REG_PBMC_LEN);
MLXSW_ITEM32_LP(reg, pbmc, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pbmc, xoff_timer_value, 0x04, 16, 16);
MLXSW_ITEM32(reg, pbmc, xoff_refresh, 0x04, 0, 16);
#define MLXSW_REG_PBMC_PORT_SHARED_BUF_IDX 11
MLXSW_ITEM32_INDEXED(reg, pbmc, buf_lossy, 0x0C, 25, 1, 0x08, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, pbmc, buf_epsb, 0x0C, 24, 1, 0x08, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, pbmc, buf_size, 0x0C, 0, 16, 0x08, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, pbmc, buf_xoff_threshold, 0x0C, 16, 16,
		     0x08, 0x04, false);
MLXSW_ITEM32_INDEXED(reg, pbmc, buf_xon_threshold, 0x0C, 0, 16,
		     0x08, 0x04, false);
static inline void mlxsw_reg_pbmc_pack(char *payload, u16 local_port,
				       u16 xoff_timer_value, u16 xoff_refresh)
{
	MLXSW_REG_ZERO(pbmc, payload);
	mlxsw_reg_pbmc_local_port_set(payload, local_port);
	mlxsw_reg_pbmc_xoff_timer_value_set(payload, xoff_timer_value);
	mlxsw_reg_pbmc_xoff_refresh_set(payload, xoff_refresh);
}
static inline void mlxsw_reg_pbmc_lossy_buffer_pack(char *payload,
						    int buf_index,
						    u16 size)
{
	mlxsw_reg_pbmc_buf_lossy_set(payload, buf_index, 1);
	mlxsw_reg_pbmc_buf_epsb_set(payload, buf_index, 0);
	mlxsw_reg_pbmc_buf_size_set(payload, buf_index, size);
}
static inline void mlxsw_reg_pbmc_lossless_buffer_pack(char *payload,
						       int buf_index, u16 size,
						       u16 threshold)
{
	mlxsw_reg_pbmc_buf_lossy_set(payload, buf_index, 0);
	mlxsw_reg_pbmc_buf_epsb_set(payload, buf_index, 0);
	mlxsw_reg_pbmc_buf_size_set(payload, buf_index, size);
	mlxsw_reg_pbmc_buf_xoff_threshold_set(payload, buf_index, threshold);
	mlxsw_reg_pbmc_buf_xon_threshold_set(payload, buf_index, threshold);
}
#define MLXSW_REG_PSPA_ID 0x500D
#define MLXSW_REG_PSPA_LEN 0x8
MLXSW_REG_DEFINE(pspa, MLXSW_REG_PSPA_ID, MLXSW_REG_PSPA_LEN);
MLXSW_ITEM32(reg, pspa, swid, 0x00, 24, 8);
MLXSW_ITEM32_LP(reg, pspa, 0x00, 16, 0x00, 0);
MLXSW_ITEM32(reg, pspa, sub_port, 0x00, 8, 8);
static inline void mlxsw_reg_pspa_pack(char *payload, u8 swid, u16 local_port)
{
	MLXSW_REG_ZERO(pspa, payload);
	mlxsw_reg_pspa_swid_set(payload, swid);
	mlxsw_reg_pspa_local_port_set(payload, local_port);
	mlxsw_reg_pspa_sub_port_set(payload, 0);
}
#define MLXSW_REG_PMAOS_ID 0x5012
#define MLXSW_REG_PMAOS_LEN 0x10
MLXSW_REG_DEFINE(pmaos, MLXSW_REG_PMAOS_ID, MLXSW_REG_PMAOS_LEN);
MLXSW_ITEM32(reg, pmaos, rst, 0x00, 31, 1);
MLXSW_ITEM32(reg, pmaos, slot_index, 0x00, 24, 4);
MLXSW_ITEM32(reg, pmaos, module, 0x00, 16, 8);
enum mlxsw_reg_pmaos_admin_status {
	MLXSW_REG_PMAOS_ADMIN_STATUS_ENABLED = 1,
	MLXSW_REG_PMAOS_ADMIN_STATUS_DISABLED = 2,
	MLXSW_REG_PMAOS_ADMIN_STATUS_ENABLED_ONCE = 3,
};
MLXSW_ITEM32(reg, pmaos, admin_status, 0x00, 8, 4);
MLXSW_ITEM32(reg, pmaos, ase, 0x04, 31, 1);
MLXSW_ITEM32(reg, pmaos, ee, 0x04, 30, 1);
enum mlxsw_reg_pmaos_e {
	MLXSW_REG_PMAOS_E_DO_NOT_GENERATE_EVENT,
	MLXSW_REG_PMAOS_E_GENERATE_EVENT,
	MLXSW_REG_PMAOS_E_GENERATE_SINGLE_EVENT,
};
MLXSW_ITEM32(reg, pmaos, e, 0x04, 0, 2);
static inline void mlxsw_reg_pmaos_pack(char *payload, u8 slot_index, u8 module)
{
	MLXSW_REG_ZERO(pmaos, payload);
	mlxsw_reg_pmaos_slot_index_set(payload, slot_index);
	mlxsw_reg_pmaos_module_set(payload, module);
}
#define MLXSW_REG_PPLR_ID 0x5018
#define MLXSW_REG_PPLR_LEN 0x8
MLXSW_REG_DEFINE(pplr, MLXSW_REG_PPLR_ID, MLXSW_REG_PPLR_LEN);
MLXSW_ITEM32_LP(reg, pplr, 0x00, 16, 0x00, 12);
#define MLXSW_REG_PPLR_LB_TYPE_BIT_PHY_LOCAL BIT(1)
MLXSW_ITEM32(reg, pplr, lb_en, 0x04, 0, 8);
static inline void mlxsw_reg_pplr_pack(char *payload, u16 local_port,
				       bool phy_local)
{
	MLXSW_REG_ZERO(pplr, payload);
	mlxsw_reg_pplr_local_port_set(payload, local_port);
	mlxsw_reg_pplr_lb_en_set(payload,
				 phy_local ?
				 MLXSW_REG_PPLR_LB_TYPE_BIT_PHY_LOCAL : 0);
}
#define MLXSW_REG_PMTDB_ID 0x501A
#define MLXSW_REG_PMTDB_LEN 0x40
MLXSW_REG_DEFINE(pmtdb, MLXSW_REG_PMTDB_ID, MLXSW_REG_PMTDB_LEN);
MLXSW_ITEM32(reg, pmtdb, slot_index, 0x00, 24, 4);
MLXSW_ITEM32(reg, pmtdb, module, 0x00, 16, 8);
MLXSW_ITEM32(reg, pmtdb, ports_width, 0x00, 12, 4);
MLXSW_ITEM32(reg, pmtdb, num_ports, 0x00, 8, 4);
enum mlxsw_reg_pmtdb_status {
	MLXSW_REG_PMTDB_STATUS_SUCCESS,
};
MLXSW_ITEM32(reg, pmtdb, status, 0x00, 0, 4);
MLXSW_ITEM16_INDEXED(reg, pmtdb, port_num, 0x04, 0, 10, 0x02, 0x00, false);
static inline void mlxsw_reg_pmtdb_pack(char *payload, u8 slot_index, u8 module,
					u8 ports_width, u8 num_ports)
{
	MLXSW_REG_ZERO(pmtdb, payload);
	mlxsw_reg_pmtdb_slot_index_set(payload, slot_index);
	mlxsw_reg_pmtdb_module_set(payload, module);
	mlxsw_reg_pmtdb_ports_width_set(payload, ports_width);
	mlxsw_reg_pmtdb_num_ports_set(payload, num_ports);
}
#define MLXSW_REG_PMECR_ID 0x501B
#define MLXSW_REG_PMECR_LEN 0x20
MLXSW_REG_DEFINE(pmecr, MLXSW_REG_PMECR_ID, MLXSW_REG_PMECR_LEN);
MLXSW_ITEM32_LP(reg, pmecr, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pmecr, ee, 0x04, 30, 1);
MLXSW_ITEM32(reg, pmecr, eswi, 0x04, 24, 1);
MLXSW_ITEM32(reg, pmecr, swi, 0x04, 8, 1);
enum mlxsw_reg_pmecr_e {
	MLXSW_REG_PMECR_E_DO_NOT_GENERATE_EVENT,
	MLXSW_REG_PMECR_E_GENERATE_EVENT,
	MLXSW_REG_PMECR_E_GENERATE_SINGLE_EVENT,
};
MLXSW_ITEM32(reg, pmecr, e, 0x04, 0, 2);
static inline void mlxsw_reg_pmecr_pack(char *payload, u16 local_port,
					enum mlxsw_reg_pmecr_e e)
{
	MLXSW_REG_ZERO(pmecr, payload);
	mlxsw_reg_pmecr_local_port_set(payload, local_port);
	mlxsw_reg_pmecr_e_set(payload, e);
	mlxsw_reg_pmecr_ee_set(payload, true);
	mlxsw_reg_pmecr_swi_set(payload, true);
	mlxsw_reg_pmecr_eswi_set(payload, true);
}
#define MLXSW_REG_PMPE_ID 0x5024
#define MLXSW_REG_PMPE_LEN 0x10
MLXSW_REG_DEFINE(pmpe, MLXSW_REG_PMPE_ID, MLXSW_REG_PMPE_LEN);
MLXSW_ITEM32(reg, pmpe, slot_index, 0x00, 24, 4);
MLXSW_ITEM32(reg, pmpe, module, 0x00, 16, 8);
enum mlxsw_reg_pmpe_module_status {
	MLXSW_REG_PMPE_MODULE_STATUS_PLUGGED_ENABLED = 1,
	MLXSW_REG_PMPE_MODULE_STATUS_UNPLUGGED,
	MLXSW_REG_PMPE_MODULE_STATUS_PLUGGED_ERROR,
	MLXSW_REG_PMPE_MODULE_STATUS_PLUGGED_DISABLED,
};
MLXSW_ITEM32(reg, pmpe, module_status, 0x00, 0, 4);
MLXSW_ITEM32(reg, pmpe, error_type, 0x04, 8, 4);
#define MLXSW_REG_PDDR_ID 0x5031
#define MLXSW_REG_PDDR_LEN 0x100
MLXSW_REG_DEFINE(pddr, MLXSW_REG_PDDR_ID, MLXSW_REG_PDDR_LEN);
MLXSW_ITEM32_LP(reg, pddr, 0x00, 16, 0x00, 12);
enum mlxsw_reg_pddr_page_select {
	MLXSW_REG_PDDR_PAGE_SELECT_TROUBLESHOOTING_INFO = 1,
};
MLXSW_ITEM32(reg, pddr, page_select, 0x04, 0, 8);
enum mlxsw_reg_pddr_trblsh_group_opcode {
	MLXSW_REG_PDDR_TRBLSH_GROUP_OPCODE_MONITOR,
};
MLXSW_ITEM32(reg, pddr, trblsh_group_opcode, 0x08, 0, 16);
MLXSW_ITEM32(reg, pddr, trblsh_status_opcode, 0x0C, 0, 16);
static inline void mlxsw_reg_pddr_pack(char *payload, u16 local_port,
				       u8 page_select)
{
	MLXSW_REG_ZERO(pddr, payload);
	mlxsw_reg_pddr_local_port_set(payload, local_port);
	mlxsw_reg_pddr_page_select_set(payload, page_select);
}
#define MLXSW_REG_PMMP_ID 0x5044
#define MLXSW_REG_PMMP_LEN 0x2C
MLXSW_REG_DEFINE(pmmp, MLXSW_REG_PMMP_ID, MLXSW_REG_PMMP_LEN);
MLXSW_ITEM32(reg, pmmp, module, 0x00, 16, 8);
MLXSW_ITEM32(reg, pmmp, slot_index, 0x00, 24, 4);
MLXSW_ITEM32(reg, pmmp, sticky, 0x00, 0, 1);
MLXSW_ITEM32(reg, pmmp, eeprom_override_mask, 0x04, 16, 16);
enum {
	MLXSW_REG_PMMP_EEPROM_OVERRIDE_LOW_POWER_MASK = BIT(8),
};
MLXSW_ITEM32(reg, pmmp, eeprom_override, 0x04, 0, 16);
static inline void mlxsw_reg_pmmp_pack(char *payload, u8 slot_index, u8 module)
{
	MLXSW_REG_ZERO(pmmp, payload);
	mlxsw_reg_pmmp_slot_index_set(payload, slot_index);
	mlxsw_reg_pmmp_module_set(payload, module);
}
#define MLXSW_REG_PLLP_ID 0x504A
#define MLXSW_REG_PLLP_LEN 0x10
MLXSW_REG_DEFINE(pllp, MLXSW_REG_PLLP_ID, MLXSW_REG_PLLP_LEN);
MLXSW_ITEM32_LP(reg, pllp, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pllp, label_port, 0x00, 0, 8);
MLXSW_ITEM32(reg, pllp, split_num, 0x04, 0, 4);
MLXSW_ITEM32(reg, pllp, slot_index, 0x08, 0, 4);
static inline void mlxsw_reg_pllp_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(pllp, payload);
	mlxsw_reg_pllp_local_port_set(payload, local_port);
}
static inline void mlxsw_reg_pllp_unpack(char *payload, u8 *label_port,
					 u8 *split_num, u8 *slot_index)
{
	*label_port = mlxsw_reg_pllp_label_port_get(payload);
	*split_num = mlxsw_reg_pllp_split_num_get(payload);
	*slot_index = mlxsw_reg_pllp_slot_index_get(payload);
}
#define MLXSW_REG_PMTM_ID 0x5067
#define MLXSW_REG_PMTM_LEN 0x10
MLXSW_REG_DEFINE(pmtm, MLXSW_REG_PMTM_ID, MLXSW_REG_PMTM_LEN);
MLXSW_ITEM32(reg, pmtm, slot_index, 0x00, 24, 4);
MLXSW_ITEM32(reg, pmtm, module, 0x00, 16, 8);
enum mlxsw_reg_pmtm_module_type {
	MLXSW_REG_PMTM_MODULE_TYPE_BACKPLANE_4_LANES = 0,
	MLXSW_REG_PMTM_MODULE_TYPE_QSFP = 1,
	MLXSW_REG_PMTM_MODULE_TYPE_SFP = 2,
	MLXSW_REG_PMTM_MODULE_TYPE_BACKPLANE_SINGLE_LANE = 4,
	MLXSW_REG_PMTM_MODULE_TYPE_BACKPLANE_2_LANES = 8,
	MLXSW_REG_PMTM_MODULE_TYPE_CHIP2CHIP4X = 10,
	MLXSW_REG_PMTM_MODULE_TYPE_CHIP2CHIP2X = 11,
	MLXSW_REG_PMTM_MODULE_TYPE_CHIP2CHIP1X = 12,
	MLXSW_REG_PMTM_MODULE_TYPE_QSFP_DD = 14,
	MLXSW_REG_PMTM_MODULE_TYPE_OSFP = 15,
	MLXSW_REG_PMTM_MODULE_TYPE_SFP_DD = 16,
	MLXSW_REG_PMTM_MODULE_TYPE_DSFP = 17,
	MLXSW_REG_PMTM_MODULE_TYPE_CHIP2CHIP8X = 18,
	MLXSW_REG_PMTM_MODULE_TYPE_TWISTED_PAIR = 19,
};
MLXSW_ITEM32(reg, pmtm, module_type, 0x04, 0, 5);
static inline void mlxsw_reg_pmtm_pack(char *payload, u8 slot_index, u8 module)
{
	MLXSW_REG_ZERO(pmtm, payload);
	mlxsw_reg_pmtm_slot_index_set(payload, slot_index);
	mlxsw_reg_pmtm_module_set(payload, module);
}
#define MLXSW_REG_HTGT_ID 0x7002
#define MLXSW_REG_HTGT_LEN 0x20
MLXSW_REG_DEFINE(htgt, MLXSW_REG_HTGT_ID, MLXSW_REG_HTGT_LEN);
MLXSW_ITEM32(reg, htgt, swid, 0x00, 24, 8);
#define MLXSW_REG_HTGT_PATH_TYPE_LOCAL 0x0	 
MLXSW_ITEM32(reg, htgt, type, 0x00, 8, 4);
enum mlxsw_reg_htgt_trap_group {
	MLXSW_REG_HTGT_TRAP_GROUP_EMAD,
	MLXSW_REG_HTGT_TRAP_GROUP_CORE_EVENT,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_STP,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_LACP,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_LLDP,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_MC_SNOOPING,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_BGP,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_OSPF,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_PIM,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_MULTICAST,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_NEIGH_DISCOVERY,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_ROUTER_EXP,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_EXTERNAL_ROUTE,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_IP2ME,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_DHCP,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_EVENT,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_IPV6,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_LBERROR,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_PTP0,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_PTP1,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_VRRP,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_PKT_SAMPLE,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_FLOW_LOGGING,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_FID_MISS,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_BFD,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_DUMMY,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_L2_DISCARDS,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_L3_DISCARDS,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_L3_EXCEPTIONS,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_TUNNEL_DISCARDS,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_ACL_DISCARDS,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_BUFFER_DISCARDS,
	MLXSW_REG_HTGT_TRAP_GROUP_SP_EAPOL,
	__MLXSW_REG_HTGT_TRAP_GROUP_MAX,
	MLXSW_REG_HTGT_TRAP_GROUP_MAX = __MLXSW_REG_HTGT_TRAP_GROUP_MAX - 1
};
MLXSW_ITEM32(reg, htgt, trap_group, 0x00, 0, 8);
enum {
	MLXSW_REG_HTGT_POLICER_DISABLE,
	MLXSW_REG_HTGT_POLICER_ENABLE,
};
MLXSW_ITEM32(reg, htgt, pide, 0x04, 15, 1);
#define MLXSW_REG_HTGT_INVALID_POLICER 0xff
MLXSW_ITEM32(reg, htgt, pid, 0x04, 0, 8);
#define MLXSW_REG_HTGT_TRAP_TO_CPU 0x0
MLXSW_ITEM32(reg, htgt, mirror_action, 0x08, 8, 2);
MLXSW_ITEM32(reg, htgt, mirroring_agent, 0x08, 0, 3);
#define MLXSW_REG_HTGT_DEFAULT_PRIORITY 0
MLXSW_ITEM32(reg, htgt, priority, 0x0C, 0, 4);
#define MLXSW_REG_HTGT_DEFAULT_TC 7
MLXSW_ITEM32(reg, htgt, local_path_cpu_tclass, 0x10, 16, 6);
enum mlxsw_reg_htgt_local_path_rdq {
	MLXSW_REG_HTGT_LOCAL_PATH_RDQ_SX2_CTRL = 0x13,
	MLXSW_REG_HTGT_LOCAL_PATH_RDQ_SX2_RX = 0x14,
	MLXSW_REG_HTGT_LOCAL_PATH_RDQ_SX2_EMAD = 0x15,
	MLXSW_REG_HTGT_LOCAL_PATH_RDQ_SIB_EMAD = 0x15,
};
MLXSW_ITEM32(reg, htgt, local_path_rdq, 0x10, 0, 6);
static inline void mlxsw_reg_htgt_pack(char *payload, u8 group, u8 policer_id,
				       u8 priority, u8 tc)
{
	MLXSW_REG_ZERO(htgt, payload);
	if (policer_id == MLXSW_REG_HTGT_INVALID_POLICER) {
		mlxsw_reg_htgt_pide_set(payload,
					MLXSW_REG_HTGT_POLICER_DISABLE);
	} else {
		mlxsw_reg_htgt_pide_set(payload,
					MLXSW_REG_HTGT_POLICER_ENABLE);
		mlxsw_reg_htgt_pid_set(payload, policer_id);
	}
	mlxsw_reg_htgt_type_set(payload, MLXSW_REG_HTGT_PATH_TYPE_LOCAL);
	mlxsw_reg_htgt_trap_group_set(payload, group);
	mlxsw_reg_htgt_mirror_action_set(payload, MLXSW_REG_HTGT_TRAP_TO_CPU);
	mlxsw_reg_htgt_mirroring_agent_set(payload, 0);
	mlxsw_reg_htgt_priority_set(payload, priority);
	mlxsw_reg_htgt_local_path_cpu_tclass_set(payload, tc);
	mlxsw_reg_htgt_local_path_rdq_set(payload, group);
}
#define MLXSW_REG_HPKT_ID 0x7003
#define MLXSW_REG_HPKT_LEN 0x10
MLXSW_REG_DEFINE(hpkt, MLXSW_REG_HPKT_ID, MLXSW_REG_HPKT_LEN);
enum {
	MLXSW_REG_HPKT_ACK_NOT_REQUIRED,
	MLXSW_REG_HPKT_ACK_REQUIRED,
};
MLXSW_ITEM32(reg, hpkt, ack, 0x00, 24, 1);
enum mlxsw_reg_hpkt_action {
	MLXSW_REG_HPKT_ACTION_FORWARD,
	MLXSW_REG_HPKT_ACTION_TRAP_TO_CPU,
	MLXSW_REG_HPKT_ACTION_MIRROR_TO_CPU,
	MLXSW_REG_HPKT_ACTION_DISCARD,
	MLXSW_REG_HPKT_ACTION_SOFT_DISCARD,
	MLXSW_REG_HPKT_ACTION_TRAP_AND_SOFT_DISCARD,
	MLXSW_REG_HPKT_ACTION_TRAP_EXCEPTION_TO_CPU,
	MLXSW_REG_HPKT_ACTION_SET_FW_DEFAULT = 15,
};
MLXSW_ITEM32(reg, hpkt, action, 0x00, 20, 3);
MLXSW_ITEM32(reg, hpkt, trap_group, 0x00, 12, 6);
MLXSW_ITEM32(reg, hpkt, trap_id, 0x00, 0, 10);
enum {
	MLXSW_REG_HPKT_CTRL_PACKET_DEFAULT,
	MLXSW_REG_HPKT_CTRL_PACKET_NO_BUFFER,
	MLXSW_REG_HPKT_CTRL_PACKET_USE_BUFFER,
};
MLXSW_ITEM32(reg, hpkt, ctrl, 0x04, 16, 2);
static inline void mlxsw_reg_hpkt_pack(char *payload, u8 action, u16 trap_id,
				       enum mlxsw_reg_htgt_trap_group trap_group,
				       bool is_ctrl)
{
	MLXSW_REG_ZERO(hpkt, payload);
	mlxsw_reg_hpkt_ack_set(payload, MLXSW_REG_HPKT_ACK_NOT_REQUIRED);
	mlxsw_reg_hpkt_action_set(payload, action);
	mlxsw_reg_hpkt_trap_group_set(payload, trap_group);
	mlxsw_reg_hpkt_trap_id_set(payload, trap_id);
	mlxsw_reg_hpkt_ctrl_set(payload, is_ctrl ?
				MLXSW_REG_HPKT_CTRL_PACKET_USE_BUFFER :
				MLXSW_REG_HPKT_CTRL_PACKET_NO_BUFFER);
}
#define MLXSW_REG_RGCR_ID 0x8001
#define MLXSW_REG_RGCR_LEN 0x28
MLXSW_REG_DEFINE(rgcr, MLXSW_REG_RGCR_ID, MLXSW_REG_RGCR_LEN);
MLXSW_ITEM32(reg, rgcr, ipv4_en, 0x00, 31, 1);
MLXSW_ITEM32(reg, rgcr, ipv6_en, 0x00, 30, 1);
MLXSW_ITEM32(reg, rgcr, max_router_interfaces, 0x10, 0, 16);
MLXSW_ITEM32(reg, rgcr, usp, 0x18, 20, 1);
MLXSW_ITEM32(reg, rgcr, pcp_rw, 0x18, 16, 2);
MLXSW_ITEM32(reg, rgcr, activity_dis, 0x20, 0, 8);
static inline void mlxsw_reg_rgcr_pack(char *payload, bool ipv4_en,
				       bool ipv6_en)
{
	MLXSW_REG_ZERO(rgcr, payload);
	mlxsw_reg_rgcr_ipv4_en_set(payload, ipv4_en);
	mlxsw_reg_rgcr_ipv6_en_set(payload, ipv6_en);
}
#define MLXSW_REG_RITR_ID 0x8002
#define MLXSW_REG_RITR_LEN 0x40
MLXSW_REG_DEFINE(ritr, MLXSW_REG_RITR_ID, MLXSW_REG_RITR_LEN);
MLXSW_ITEM32(reg, ritr, enable, 0x00, 31, 1);
MLXSW_ITEM32(reg, ritr, ipv4, 0x00, 29, 1);
MLXSW_ITEM32(reg, ritr, ipv6, 0x00, 28, 1);
MLXSW_ITEM32(reg, ritr, ipv4_mc, 0x00, 27, 1);
MLXSW_ITEM32(reg, ritr, ipv6_mc, 0x00, 26, 1);
enum mlxsw_reg_ritr_if_type {
	MLXSW_REG_RITR_VLAN_IF,
	MLXSW_REG_RITR_FID_IF,
	MLXSW_REG_RITR_SP_IF,
	MLXSW_REG_RITR_LOOPBACK_IF,
};
MLXSW_ITEM32(reg, ritr, type, 0x00, 23, 3);
enum {
	MLXSW_REG_RITR_RIF_CREATE,
	MLXSW_REG_RITR_RIF_DEL,
};
MLXSW_ITEM32(reg, ritr, op, 0x00, 20, 2);
MLXSW_ITEM32(reg, ritr, rif, 0x00, 0, 16);
MLXSW_ITEM32(reg, ritr, ipv4_fe, 0x04, 29, 1);
MLXSW_ITEM32(reg, ritr, ipv6_fe, 0x04, 28, 1);
MLXSW_ITEM32(reg, ritr, ipv4_mc_fe, 0x04, 27, 1);
MLXSW_ITEM32(reg, ritr, ipv6_mc_fe, 0x04, 26, 1);
MLXSW_ITEM32(reg, ritr, lb_en, 0x04, 24, 1);
MLXSW_ITEM32(reg, ritr, virtual_router, 0x04, 0, 16);
MLXSW_ITEM32(reg, ritr, mtu, 0x34, 0, 16);
MLXSW_ITEM32(reg, ritr, if_swid, 0x08, 24, 8);
MLXSW_ITEM32(reg, ritr, if_mac_profile_id, 0x10, 16, 4);
MLXSW_ITEM_BUF(reg, ritr, if_mac, 0x12, 6);
MLXSW_ITEM32(reg, ritr, if_vrrp_id_ipv6, 0x1C, 8, 8);
MLXSW_ITEM32(reg, ritr, if_vrrp_id_ipv4, 0x1C, 0, 8);
MLXSW_ITEM32(reg, ritr, vlan_if_vlan_id, 0x08, 0, 12);
MLXSW_ITEM32(reg, ritr, vlan_if_efid, 0x0C, 0, 16);
MLXSW_ITEM32(reg, ritr, fid_if_fid, 0x08, 0, 16);
MLXSW_ITEM32(reg, ritr, sp_if_lag, 0x08, 24, 1);
MLXSW_ITEM32(reg, ritr, sp_if_system_port, 0x08, 0, 16);
MLXSW_ITEM32(reg, ritr, sp_if_efid, 0x0C, 0, 16);
MLXSW_ITEM32(reg, ritr, sp_if_vid, 0x18, 0, 12);
enum mlxsw_reg_ritr_loopback_protocol {
	MLXSW_REG_RITR_LOOPBACK_PROTOCOL_IPIP_IPV4,
	MLXSW_REG_RITR_LOOPBACK_PROTOCOL_IPIP_IPV6,
	MLXSW_REG_RITR_LOOPBACK_GENERIC,
};
MLXSW_ITEM32(reg, ritr, loopback_protocol, 0x08, 28, 4);
enum mlxsw_reg_ritr_loopback_ipip_type {
	MLXSW_REG_RITR_LOOPBACK_IPIP_TYPE_IP_IN_IP,
	MLXSW_REG_RITR_LOOPBACK_IPIP_TYPE_IP_IN_GRE_IN_IP,
	MLXSW_REG_RITR_LOOPBACK_IPIP_TYPE_IP_IN_GRE_KEY_IN_IP,
};
MLXSW_ITEM32(reg, ritr, loopback_ipip_type, 0x10, 24, 4);
enum mlxsw_reg_ritr_loopback_ipip_options {
	MLXSW_REG_RITR_LOOPBACK_IPIP_OPTIONS_GRE_KEY_PRESET,
};
MLXSW_ITEM32(reg, ritr, loopback_ipip_options, 0x10, 20, 4);
MLXSW_ITEM32(reg, ritr, loopback_ipip_uvr, 0x10, 0, 16);
MLXSW_ITEM32(reg, ritr, loopback_ipip_underlay_rif, 0x14, 0, 16);
MLXSW_ITEM_BUF(reg, ritr, loopback_ipip_usip6, 0x18, 16);
MLXSW_ITEM32(reg, ritr, loopback_ipip_usip4, 0x24, 0, 32);
MLXSW_ITEM32(reg, ritr, loopback_ipip_gre_key, 0x28, 0, 32);
enum mlxsw_reg_ritr_counter_set_type {
	MLXSW_REG_RITR_COUNTER_SET_TYPE_NO_COUNT = 0x0,
	MLXSW_REG_RITR_COUNTER_SET_TYPE_BASIC = 0x9,
};
MLXSW_ITEM32(reg, ritr, ingress_counter_index, 0x38, 0, 24);
MLXSW_ITEM32(reg, ritr, ingress_counter_set_type, 0x38, 24, 8);
MLXSW_ITEM32(reg, ritr, egress_counter_index, 0x3C, 0, 24);
MLXSW_ITEM32(reg, ritr, egress_counter_set_type, 0x3C, 24, 8);
static inline void mlxsw_reg_ritr_counter_pack(char *payload, u32 index,
					       bool enable, bool egress)
{
	enum mlxsw_reg_ritr_counter_set_type set_type;
	if (enable)
		set_type = MLXSW_REG_RITR_COUNTER_SET_TYPE_BASIC;
	else
		set_type = MLXSW_REG_RITR_COUNTER_SET_TYPE_NO_COUNT;
	if (egress) {
		mlxsw_reg_ritr_egress_counter_set_type_set(payload, set_type);
		mlxsw_reg_ritr_egress_counter_index_set(payload, index);
	} else {
		mlxsw_reg_ritr_ingress_counter_set_type_set(payload, set_type);
		mlxsw_reg_ritr_ingress_counter_index_set(payload, index);
	}
}
static inline void mlxsw_reg_ritr_rif_pack(char *payload, u16 rif)
{
	MLXSW_REG_ZERO(ritr, payload);
	mlxsw_reg_ritr_rif_set(payload, rif);
}
static inline void mlxsw_reg_ritr_sp_if_pack(char *payload, bool lag,
					     u16 system_port, u16 efid, u16 vid)
{
	mlxsw_reg_ritr_sp_if_lag_set(payload, lag);
	mlxsw_reg_ritr_sp_if_system_port_set(payload, system_port);
	mlxsw_reg_ritr_sp_if_efid_set(payload, efid);
	mlxsw_reg_ritr_sp_if_vid_set(payload, vid);
}
static inline void mlxsw_reg_ritr_pack(char *payload, bool enable,
				       enum mlxsw_reg_ritr_if_type type,
				       u16 rif, u16 vr_id, u16 mtu)
{
	bool op = enable ? MLXSW_REG_RITR_RIF_CREATE : MLXSW_REG_RITR_RIF_DEL;
	MLXSW_REG_ZERO(ritr, payload);
	mlxsw_reg_ritr_enable_set(payload, enable);
	mlxsw_reg_ritr_ipv4_set(payload, 1);
	mlxsw_reg_ritr_ipv6_set(payload, 1);
	mlxsw_reg_ritr_ipv4_mc_set(payload, 1);
	mlxsw_reg_ritr_ipv6_mc_set(payload, 1);
	mlxsw_reg_ritr_type_set(payload, type);
	mlxsw_reg_ritr_op_set(payload, op);
	mlxsw_reg_ritr_rif_set(payload, rif);
	mlxsw_reg_ritr_ipv4_fe_set(payload, 1);
	mlxsw_reg_ritr_ipv6_fe_set(payload, 1);
	mlxsw_reg_ritr_ipv4_mc_fe_set(payload, 1);
	mlxsw_reg_ritr_ipv6_mc_fe_set(payload, 1);
	mlxsw_reg_ritr_lb_en_set(payload, 1);
	mlxsw_reg_ritr_virtual_router_set(payload, vr_id);
	mlxsw_reg_ritr_mtu_set(payload, mtu);
}
static inline void mlxsw_reg_ritr_mac_pack(char *payload, const char *mac)
{
	mlxsw_reg_ritr_if_mac_memcpy_to(payload, mac);
}
static inline void
mlxsw_reg_ritr_vlan_if_pack(char *payload, bool enable, u16 rif, u16 vr_id,
			    u16 mtu, const char *mac, u8 mac_profile_id,
			    u16 vlan_id, u16 efid)
{
	enum mlxsw_reg_ritr_if_type type = MLXSW_REG_RITR_VLAN_IF;
	mlxsw_reg_ritr_pack(payload, enable, type, rif, vr_id, mtu);
	mlxsw_reg_ritr_if_mac_memcpy_to(payload, mac);
	mlxsw_reg_ritr_if_mac_profile_id_set(payload, mac_profile_id);
	mlxsw_reg_ritr_vlan_if_vlan_id_set(payload, vlan_id);
	mlxsw_reg_ritr_vlan_if_efid_set(payload, efid);
}
static inline void
mlxsw_reg_ritr_loopback_ipip_common_pack(char *payload,
			    enum mlxsw_reg_ritr_loopback_ipip_type ipip_type,
			    enum mlxsw_reg_ritr_loopback_ipip_options options,
			    u16 uvr_id, u16 underlay_rif, u32 gre_key)
{
	mlxsw_reg_ritr_loopback_ipip_type_set(payload, ipip_type);
	mlxsw_reg_ritr_loopback_ipip_options_set(payload, options);
	mlxsw_reg_ritr_loopback_ipip_uvr_set(payload, uvr_id);
	mlxsw_reg_ritr_loopback_ipip_underlay_rif_set(payload, underlay_rif);
	mlxsw_reg_ritr_loopback_ipip_gre_key_set(payload, gre_key);
}
static inline void
mlxsw_reg_ritr_loopback_ipip4_pack(char *payload,
			    enum mlxsw_reg_ritr_loopback_ipip_type ipip_type,
			    enum mlxsw_reg_ritr_loopback_ipip_options options,
			    u16 uvr_id, u16 underlay_rif, u32 usip, u32 gre_key)
{
	mlxsw_reg_ritr_loopback_protocol_set(payload,
				    MLXSW_REG_RITR_LOOPBACK_PROTOCOL_IPIP_IPV4);
	mlxsw_reg_ritr_loopback_ipip_common_pack(payload, ipip_type, options,
						 uvr_id, underlay_rif, gre_key);
	mlxsw_reg_ritr_loopback_ipip_usip4_set(payload, usip);
}
static inline void
mlxsw_reg_ritr_loopback_ipip6_pack(char *payload,
				   enum mlxsw_reg_ritr_loopback_ipip_type ipip_type,
				   enum mlxsw_reg_ritr_loopback_ipip_options options,
				   u16 uvr_id, u16 underlay_rif,
				   const struct in6_addr *usip, u32 gre_key)
{
	enum mlxsw_reg_ritr_loopback_protocol protocol =
		MLXSW_REG_RITR_LOOPBACK_PROTOCOL_IPIP_IPV6;
	mlxsw_reg_ritr_loopback_protocol_set(payload, protocol);
	mlxsw_reg_ritr_loopback_ipip_common_pack(payload, ipip_type, options,
						 uvr_id, underlay_rif, gre_key);
	mlxsw_reg_ritr_loopback_ipip_usip6_memcpy_to(payload,
						     (const char *)usip);
}
#define MLXSW_REG_RTAR_ID 0x8004
#define MLXSW_REG_RTAR_LEN 0x20
MLXSW_REG_DEFINE(rtar, MLXSW_REG_RTAR_ID, MLXSW_REG_RTAR_LEN);
enum mlxsw_reg_rtar_op {
	MLXSW_REG_RTAR_OP_ALLOCATE,
	MLXSW_REG_RTAR_OP_RESIZE,
	MLXSW_REG_RTAR_OP_DEALLOCATE,
};
MLXSW_ITEM32(reg, rtar, op, 0x00, 28, 4);
enum mlxsw_reg_rtar_key_type {
	MLXSW_REG_RTAR_KEY_TYPE_IPV4_MULTICAST = 1,
	MLXSW_REG_RTAR_KEY_TYPE_IPV6_MULTICAST = 3
};
MLXSW_ITEM32(reg, rtar, key_type, 0x00, 0, 8);
MLXSW_ITEM32(reg, rtar, region_size, 0x04, 0, 16);
static inline void mlxsw_reg_rtar_pack(char *payload,
				       enum mlxsw_reg_rtar_op op,
				       enum mlxsw_reg_rtar_key_type key_type,
				       u16 region_size)
{
	MLXSW_REG_ZERO(rtar, payload);
	mlxsw_reg_rtar_op_set(payload, op);
	mlxsw_reg_rtar_key_type_set(payload, key_type);
	mlxsw_reg_rtar_region_size_set(payload, region_size);
}
#define MLXSW_REG_RATR_ID 0x8008
#define MLXSW_REG_RATR_LEN 0x2C
MLXSW_REG_DEFINE(ratr, MLXSW_REG_RATR_ID, MLXSW_REG_RATR_LEN);
enum mlxsw_reg_ratr_op {
	MLXSW_REG_RATR_OP_QUERY_READ = 0,
	MLXSW_REG_RATR_OP_QUERY_READ_CLEAR = 2,
	MLXSW_REG_RATR_OP_WRITE_WRITE_ENTRY = 1,
	MLXSW_REG_RATR_OP_WRITE_WRITE_ENTRY_ON_ACTIVITY = 3,
};
MLXSW_ITEM32(reg, ratr, op, 0x00, 28, 4);
MLXSW_ITEM32(reg, ratr, v, 0x00, 24, 1);
MLXSW_ITEM32(reg, ratr, a, 0x00, 16, 1);
enum mlxsw_reg_ratr_type {
	MLXSW_REG_RATR_TYPE_ETHERNET,
	MLXSW_REG_RATR_TYPE_IPOIB_UC,
	MLXSW_REG_RATR_TYPE_IPOIB_UC_W_GRH,
	MLXSW_REG_RATR_TYPE_IPOIB_MC,
	MLXSW_REG_RATR_TYPE_MPLS,
	MLXSW_REG_RATR_TYPE_IPIP,
};
MLXSW_ITEM32(reg, ratr, type, 0x04, 28, 4);
MLXSW_ITEM32(reg, ratr, adjacency_index_low, 0x04, 0, 16);
MLXSW_ITEM32(reg, ratr, egress_router_interface, 0x08, 0, 16);
enum mlxsw_reg_ratr_trap_action {
	MLXSW_REG_RATR_TRAP_ACTION_NOP,
	MLXSW_REG_RATR_TRAP_ACTION_TRAP,
	MLXSW_REG_RATR_TRAP_ACTION_MIRROR_TO_CPU,
	MLXSW_REG_RATR_TRAP_ACTION_MIRROR,
	MLXSW_REG_RATR_TRAP_ACTION_DISCARD_ERRORS,
};
MLXSW_ITEM32(reg, ratr, trap_action, 0x0C, 28, 4);
MLXSW_ITEM32(reg, ratr, adjacency_index_high, 0x0C, 16, 8);
enum mlxsw_reg_ratr_trap_id {
	MLXSW_REG_RATR_TRAP_ID_RTR_EGRESS0,
	MLXSW_REG_RATR_TRAP_ID_RTR_EGRESS1,
};
MLXSW_ITEM32(reg, ratr, trap_id, 0x0C, 0, 8);
MLXSW_ITEM_BUF(reg, ratr, eth_destination_mac, 0x12, 6);
enum mlxsw_reg_ratr_ipip_type {
	MLXSW_REG_RATR_IPIP_TYPE_IPV4,
	MLXSW_REG_RATR_IPIP_TYPE_IPV6,
};
MLXSW_ITEM32(reg, ratr, ipip_type, 0x10, 16, 4);
MLXSW_ITEM32(reg, ratr, ipip_ipv4_udip, 0x18, 0, 32);
MLXSW_ITEM32(reg, ratr, ipip_ipv6_ptr, 0x1C, 0, 24);
enum mlxsw_reg_flow_counter_set_type {
	MLXSW_REG_FLOW_COUNTER_SET_TYPE_NO_COUNT = 0x00,
	MLXSW_REG_FLOW_COUNTER_SET_TYPE_PACKETS_BYTES = 0x03,
	MLXSW_REG_FLOW_COUNTER_SET_TYPE_PACKETS = 0x05,
};
MLXSW_ITEM32(reg, ratr, counter_set_type, 0x28, 24, 8);
MLXSW_ITEM32(reg, ratr, counter_index, 0x28, 0, 24);
static inline void
mlxsw_reg_ratr_pack(char *payload,
		    enum mlxsw_reg_ratr_op op, bool valid,
		    enum mlxsw_reg_ratr_type type,
		    u32 adjacency_index, u16 egress_rif)
{
	MLXSW_REG_ZERO(ratr, payload);
	mlxsw_reg_ratr_op_set(payload, op);
	mlxsw_reg_ratr_v_set(payload, valid);
	mlxsw_reg_ratr_type_set(payload, type);
	mlxsw_reg_ratr_adjacency_index_low_set(payload, adjacency_index);
	mlxsw_reg_ratr_adjacency_index_high_set(payload, adjacency_index >> 16);
	mlxsw_reg_ratr_egress_router_interface_set(payload, egress_rif);
}
static inline void mlxsw_reg_ratr_eth_entry_pack(char *payload,
						 const char *dest_mac)
{
	mlxsw_reg_ratr_eth_destination_mac_memcpy_to(payload, dest_mac);
}
static inline void mlxsw_reg_ratr_ipip4_entry_pack(char *payload, u32 ipv4_udip)
{
	mlxsw_reg_ratr_ipip_type_set(payload, MLXSW_REG_RATR_IPIP_TYPE_IPV4);
	mlxsw_reg_ratr_ipip_ipv4_udip_set(payload, ipv4_udip);
}
static inline void mlxsw_reg_ratr_ipip6_entry_pack(char *payload, u32 ipv6_ptr)
{
	mlxsw_reg_ratr_ipip_type_set(payload, MLXSW_REG_RATR_IPIP_TYPE_IPV6);
	mlxsw_reg_ratr_ipip_ipv6_ptr_set(payload, ipv6_ptr);
}
static inline void mlxsw_reg_ratr_counter_pack(char *payload, u64 counter_index,
					       bool counter_enable)
{
	enum mlxsw_reg_flow_counter_set_type set_type;
	if (counter_enable)
		set_type = MLXSW_REG_FLOW_COUNTER_SET_TYPE_PACKETS_BYTES;
	else
		set_type = MLXSW_REG_FLOW_COUNTER_SET_TYPE_NO_COUNT;
	mlxsw_reg_ratr_counter_index_set(payload, counter_index);
	mlxsw_reg_ratr_counter_set_type_set(payload, set_type);
}
#define MLXSW_REG_RDPM_ID 0x8009
#define MLXSW_REG_RDPM_BASE_LEN 0x00
#define MLXSW_REG_RDPM_DSCP_ENTRY_REC_LEN 0x01
#define MLXSW_REG_RDPM_DSCP_ENTRY_REC_MAX_COUNT 64
#define MLXSW_REG_RDPM_LEN 0x40
#define MLXSW_REG_RDPM_LAST_ENTRY (MLXSW_REG_RDPM_BASE_LEN + \
				   MLXSW_REG_RDPM_LEN - \
				   MLXSW_REG_RDPM_DSCP_ENTRY_REC_LEN)
MLXSW_REG_DEFINE(rdpm, MLXSW_REG_RDPM_ID, MLXSW_REG_RDPM_LEN);
MLXSW_ITEM8_INDEXED(reg, rdpm, dscp_entry_e, MLXSW_REG_RDPM_LAST_ENTRY, 7, 1,
		    -MLXSW_REG_RDPM_DSCP_ENTRY_REC_LEN, 0x00, false);
MLXSW_ITEM8_INDEXED(reg, rdpm, dscp_entry_prio, MLXSW_REG_RDPM_LAST_ENTRY, 0, 4,
		    -MLXSW_REG_RDPM_DSCP_ENTRY_REC_LEN, 0x00, false);
static inline void mlxsw_reg_rdpm_pack(char *payload, unsigned short index,
				       u8 prio)
{
	mlxsw_reg_rdpm_dscp_entry_e_set(payload, index, 1);
	mlxsw_reg_rdpm_dscp_entry_prio_set(payload, index, prio);
}
#define MLXSW_REG_RICNT_ID 0x800B
#define MLXSW_REG_RICNT_LEN 0x100
MLXSW_REG_DEFINE(ricnt, MLXSW_REG_RICNT_ID, MLXSW_REG_RICNT_LEN);
MLXSW_ITEM32(reg, ricnt, counter_index, 0x04, 0, 24);
enum mlxsw_reg_ricnt_counter_set_type {
	MLXSW_REG_RICNT_COUNTER_SET_TYPE_NO_COUNT = 0x00,
	MLXSW_REG_RICNT_COUNTER_SET_TYPE_BASIC = 0x09,
};
MLXSW_ITEM32(reg, ricnt, counter_set_type, 0x04, 24, 8);
enum mlxsw_reg_ricnt_opcode {
	MLXSW_REG_RICNT_OPCODE_NOP = 0x00,
	MLXSW_REG_RICNT_OPCODE_CLEAR = 0x08,
};
MLXSW_ITEM32(reg, ricnt, op, 0x00, 28, 4);
MLXSW_ITEM64(reg, ricnt, good_unicast_packets, 0x08, 0, 64);
MLXSW_ITEM64(reg, ricnt, good_multicast_packets, 0x10, 0, 64);
MLXSW_ITEM64(reg, ricnt, good_broadcast_packets, 0x18, 0, 64);
MLXSW_ITEM64(reg, ricnt, good_unicast_bytes, 0x20, 0, 64);
MLXSW_ITEM64(reg, ricnt, good_multicast_bytes, 0x28, 0, 64);
MLXSW_ITEM64(reg, ricnt, good_broadcast_bytes, 0x30, 0, 64);
MLXSW_ITEM64(reg, ricnt, error_packets, 0x38, 0, 64);
MLXSW_ITEM64(reg, ricnt, discard_packets, 0x40, 0, 64);
MLXSW_ITEM64(reg, ricnt, error_bytes, 0x48, 0, 64);
MLXSW_ITEM64(reg, ricnt, discard_bytes, 0x50, 0, 64);
static inline void mlxsw_reg_ricnt_pack(char *payload, u32 index,
					enum mlxsw_reg_ricnt_opcode op)
{
	MLXSW_REG_ZERO(ricnt, payload);
	mlxsw_reg_ricnt_op_set(payload, op);
	mlxsw_reg_ricnt_counter_index_set(payload, index);
	mlxsw_reg_ricnt_counter_set_type_set(payload,
					     MLXSW_REG_RICNT_COUNTER_SET_TYPE_BASIC);
}
#define MLXSW_REG_RRCR_ID 0x800F
#define MLXSW_REG_RRCR_LEN 0x24
MLXSW_REG_DEFINE(rrcr, MLXSW_REG_RRCR_ID, MLXSW_REG_RRCR_LEN);
enum mlxsw_reg_rrcr_op {
	MLXSW_REG_RRCR_OP_MOVE,
	MLXSW_REG_RRCR_OP_COPY,
};
MLXSW_ITEM32(reg, rrcr, op, 0x00, 28, 4);
MLXSW_ITEM32(reg, rrcr, offset, 0x00, 0, 16);
MLXSW_ITEM32(reg, rrcr, size, 0x04, 0, 16);
MLXSW_ITEM32(reg, rrcr, table_id, 0x10, 0, 4);
MLXSW_ITEM32(reg, rrcr, dest_offset, 0x20, 0, 16);
static inline void mlxsw_reg_rrcr_pack(char *payload, enum mlxsw_reg_rrcr_op op,
				       u16 offset, u16 size,
				       enum mlxsw_reg_rtar_key_type table_id,
				       u16 dest_offset)
{
	MLXSW_REG_ZERO(rrcr, payload);
	mlxsw_reg_rrcr_op_set(payload, op);
	mlxsw_reg_rrcr_offset_set(payload, offset);
	mlxsw_reg_rrcr_size_set(payload, size);
	mlxsw_reg_rrcr_table_id_set(payload, table_id);
	mlxsw_reg_rrcr_dest_offset_set(payload, dest_offset);
}
#define MLXSW_REG_RALTA_ID 0x8010
#define MLXSW_REG_RALTA_LEN 0x04
MLXSW_REG_DEFINE(ralta, MLXSW_REG_RALTA_ID, MLXSW_REG_RALTA_LEN);
MLXSW_ITEM32(reg, ralta, op, 0x00, 28, 2);
enum mlxsw_reg_ralxx_protocol {
	MLXSW_REG_RALXX_PROTOCOL_IPV4,
	MLXSW_REG_RALXX_PROTOCOL_IPV6,
};
MLXSW_ITEM32(reg, ralta, protocol, 0x00, 24, 4);
MLXSW_ITEM32(reg, ralta, tree_id, 0x00, 0, 8);
static inline void mlxsw_reg_ralta_pack(char *payload, bool alloc,
					enum mlxsw_reg_ralxx_protocol protocol,
					u8 tree_id)
{
	MLXSW_REG_ZERO(ralta, payload);
	mlxsw_reg_ralta_op_set(payload, !alloc);
	mlxsw_reg_ralta_protocol_set(payload, protocol);
	mlxsw_reg_ralta_tree_id_set(payload, tree_id);
}
#define MLXSW_REG_RALST_ID 0x8011
#define MLXSW_REG_RALST_LEN 0x104
MLXSW_REG_DEFINE(ralst, MLXSW_REG_RALST_ID, MLXSW_REG_RALST_LEN);
MLXSW_ITEM32(reg, ralst, root_bin, 0x00, 16, 8);
MLXSW_ITEM32(reg, ralst, tree_id, 0x00, 0, 8);
#define MLXSW_REG_RALST_BIN_NO_CHILD 0xff
#define MLXSW_REG_RALST_BIN_OFFSET 0x04
#define MLXSW_REG_RALST_BIN_COUNT 128
MLXSW_ITEM16_INDEXED(reg, ralst, left_child_bin, 0x04, 8, 8, 0x02, 0x00, false);
MLXSW_ITEM16_INDEXED(reg, ralst, right_child_bin, 0x04, 0, 8, 0x02, 0x00,
		     false);
static inline void mlxsw_reg_ralst_pack(char *payload, u8 root_bin, u8 tree_id)
{
	MLXSW_REG_ZERO(ralst, payload);
	memset(payload + MLXSW_REG_RALST_BIN_OFFSET,
	       MLXSW_REG_RALST_BIN_NO_CHILD, MLXSW_REG_RALST_BIN_COUNT * 2);
	mlxsw_reg_ralst_root_bin_set(payload, root_bin);
	mlxsw_reg_ralst_tree_id_set(payload, tree_id);
}
static inline void mlxsw_reg_ralst_bin_pack(char *payload, u8 bin_number,
					    u8 left_child_bin,
					    u8 right_child_bin)
{
	int bin_index = bin_number - 1;
	mlxsw_reg_ralst_left_child_bin_set(payload, bin_index, left_child_bin);
	mlxsw_reg_ralst_right_child_bin_set(payload, bin_index,
					    right_child_bin);
}
#define MLXSW_REG_RALTB_ID 0x8012
#define MLXSW_REG_RALTB_LEN 0x04
MLXSW_REG_DEFINE(raltb, MLXSW_REG_RALTB_ID, MLXSW_REG_RALTB_LEN);
MLXSW_ITEM32(reg, raltb, virtual_router, 0x00, 16, 16);
MLXSW_ITEM32(reg, raltb, protocol, 0x00, 12, 4);
MLXSW_ITEM32(reg, raltb, tree_id, 0x00, 0, 8);
static inline void mlxsw_reg_raltb_pack(char *payload, u16 virtual_router,
					enum mlxsw_reg_ralxx_protocol protocol,
					u8 tree_id)
{
	MLXSW_REG_ZERO(raltb, payload);
	mlxsw_reg_raltb_virtual_router_set(payload, virtual_router);
	mlxsw_reg_raltb_protocol_set(payload, protocol);
	mlxsw_reg_raltb_tree_id_set(payload, tree_id);
}
#define MLXSW_REG_RALUE_ID 0x8013
#define MLXSW_REG_RALUE_LEN 0x38
MLXSW_REG_DEFINE(ralue, MLXSW_REG_RALUE_ID, MLXSW_REG_RALUE_LEN);
MLXSW_ITEM32(reg, ralue, protocol, 0x00, 24, 4);
enum mlxsw_reg_ralue_op {
	MLXSW_REG_RALUE_OP_QUERY_READ = 0,
	MLXSW_REG_RALUE_OP_QUERY_CLEAR = 1,
	MLXSW_REG_RALUE_OP_WRITE_WRITE = 0,
	MLXSW_REG_RALUE_OP_WRITE_UPDATE = 1,
	MLXSW_REG_RALUE_OP_WRITE_CLEAR = 2,
	MLXSW_REG_RALUE_OP_WRITE_DELETE = 3,
};
MLXSW_ITEM32(reg, ralue, op, 0x00, 20, 3);
MLXSW_ITEM32(reg, ralue, a, 0x00, 16, 1);
MLXSW_ITEM32(reg, ralue, virtual_router, 0x04, 16, 16);
#define MLXSW_REG_RALUE_OP_U_MASK_ENTRY_TYPE	BIT(0)
#define MLXSW_REG_RALUE_OP_U_MASK_BMP_LEN	BIT(1)
#define MLXSW_REG_RALUE_OP_U_MASK_ACTION	BIT(2)
MLXSW_ITEM32(reg, ralue, op_u_mask, 0x04, 8, 3);
MLXSW_ITEM32(reg, ralue, prefix_len, 0x08, 0, 8);
MLXSW_ITEM32(reg, ralue, dip4, 0x18, 0, 32);
MLXSW_ITEM_BUF(reg, ralue, dip6, 0x0C, 16);
enum mlxsw_reg_ralue_entry_type {
	MLXSW_REG_RALUE_ENTRY_TYPE_MARKER_ENTRY = 1,
	MLXSW_REG_RALUE_ENTRY_TYPE_ROUTE_ENTRY = 2,
	MLXSW_REG_RALUE_ENTRY_TYPE_MARKER_AND_ROUTE_ENTRY = 3,
};
MLXSW_ITEM32(reg, ralue, entry_type, 0x1C, 30, 2);
MLXSW_ITEM32(reg, ralue, bmp_len, 0x1C, 16, 8);
enum mlxsw_reg_ralue_action_type {
	MLXSW_REG_RALUE_ACTION_TYPE_REMOTE,
	MLXSW_REG_RALUE_ACTION_TYPE_LOCAL,
	MLXSW_REG_RALUE_ACTION_TYPE_IP2ME,
};
MLXSW_ITEM32(reg, ralue, action_type, 0x1C, 0, 2);
enum mlxsw_reg_ralue_trap_action {
	MLXSW_REG_RALUE_TRAP_ACTION_NOP,
	MLXSW_REG_RALUE_TRAP_ACTION_TRAP,
	MLXSW_REG_RALUE_TRAP_ACTION_MIRROR_TO_CPU,
	MLXSW_REG_RALUE_TRAP_ACTION_MIRROR,
	MLXSW_REG_RALUE_TRAP_ACTION_DISCARD_ERROR,
};
MLXSW_ITEM32(reg, ralue, trap_action, 0x20, 28, 4);
MLXSW_ITEM32(reg, ralue, trap_id, 0x20, 0, 9);
MLXSW_ITEM32(reg, ralue, adjacency_index, 0x24, 0, 24);
MLXSW_ITEM32(reg, ralue, ecmp_size, 0x28, 0, 13);
MLXSW_ITEM32(reg, ralue, local_erif, 0x24, 0, 16);
MLXSW_ITEM32(reg, ralue, ip2me_v, 0x24, 31, 1);
MLXSW_ITEM32(reg, ralue, ip2me_tunnel_ptr, 0x24, 0, 24);
static inline void mlxsw_reg_ralue_pack(char *payload,
					enum mlxsw_reg_ralxx_protocol protocol,
					enum mlxsw_reg_ralue_op op,
					u16 virtual_router, u8 prefix_len)
{
	MLXSW_REG_ZERO(ralue, payload);
	mlxsw_reg_ralue_protocol_set(payload, protocol);
	mlxsw_reg_ralue_op_set(payload, op);
	mlxsw_reg_ralue_virtual_router_set(payload, virtual_router);
	mlxsw_reg_ralue_prefix_len_set(payload, prefix_len);
	mlxsw_reg_ralue_entry_type_set(payload,
				       MLXSW_REG_RALUE_ENTRY_TYPE_ROUTE_ENTRY);
	mlxsw_reg_ralue_bmp_len_set(payload, prefix_len);
}
static inline void mlxsw_reg_ralue_pack4(char *payload,
					 enum mlxsw_reg_ralxx_protocol protocol,
					 enum mlxsw_reg_ralue_op op,
					 u16 virtual_router, u8 prefix_len,
					 u32 dip)
{
	mlxsw_reg_ralue_pack(payload, protocol, op, virtual_router, prefix_len);
	mlxsw_reg_ralue_dip4_set(payload, dip);
}
static inline void mlxsw_reg_ralue_pack6(char *payload,
					 enum mlxsw_reg_ralxx_protocol protocol,
					 enum mlxsw_reg_ralue_op op,
					 u16 virtual_router, u8 prefix_len,
					 const void *dip)
{
	mlxsw_reg_ralue_pack(payload, protocol, op, virtual_router, prefix_len);
	mlxsw_reg_ralue_dip6_memcpy_to(payload, dip);
}
static inline void
mlxsw_reg_ralue_act_remote_pack(char *payload,
				enum mlxsw_reg_ralue_trap_action trap_action,
				u16 trap_id, u32 adjacency_index, u16 ecmp_size)
{
	mlxsw_reg_ralue_action_type_set(payload,
					MLXSW_REG_RALUE_ACTION_TYPE_REMOTE);
	mlxsw_reg_ralue_trap_action_set(payload, trap_action);
	mlxsw_reg_ralue_trap_id_set(payload, trap_id);
	mlxsw_reg_ralue_adjacency_index_set(payload, adjacency_index);
	mlxsw_reg_ralue_ecmp_size_set(payload, ecmp_size);
}
static inline void
mlxsw_reg_ralue_act_local_pack(char *payload,
			       enum mlxsw_reg_ralue_trap_action trap_action,
			       u16 trap_id, u16 local_erif)
{
	mlxsw_reg_ralue_action_type_set(payload,
					MLXSW_REG_RALUE_ACTION_TYPE_LOCAL);
	mlxsw_reg_ralue_trap_action_set(payload, trap_action);
	mlxsw_reg_ralue_trap_id_set(payload, trap_id);
	mlxsw_reg_ralue_local_erif_set(payload, local_erif);
}
static inline void
mlxsw_reg_ralue_act_ip2me_pack(char *payload)
{
	mlxsw_reg_ralue_action_type_set(payload,
					MLXSW_REG_RALUE_ACTION_TYPE_IP2ME);
}
static inline void
mlxsw_reg_ralue_act_ip2me_tun_pack(char *payload, u32 tunnel_ptr)
{
	mlxsw_reg_ralue_action_type_set(payload,
					MLXSW_REG_RALUE_ACTION_TYPE_IP2ME);
	mlxsw_reg_ralue_ip2me_v_set(payload, 1);
	mlxsw_reg_ralue_ip2me_tunnel_ptr_set(payload, tunnel_ptr);
}
#define MLXSW_REG_RAUHT_ID 0x8014
#define MLXSW_REG_RAUHT_LEN 0x74
MLXSW_REG_DEFINE(rauht, MLXSW_REG_RAUHT_ID, MLXSW_REG_RAUHT_LEN);
enum mlxsw_reg_rauht_type {
	MLXSW_REG_RAUHT_TYPE_IPV4,
	MLXSW_REG_RAUHT_TYPE_IPV6,
};
MLXSW_ITEM32(reg, rauht, type, 0x00, 24, 2);
enum mlxsw_reg_rauht_op {
	MLXSW_REG_RAUHT_OP_QUERY_READ = 0,
	MLXSW_REG_RAUHT_OP_QUERY_CLEAR_ON_READ = 1,
	MLXSW_REG_RAUHT_OP_WRITE_ADD = 0,
	MLXSW_REG_RAUHT_OP_WRITE_UPDATE = 1,
	MLXSW_REG_RAUHT_OP_WRITE_CLEAR_ACTIVITY = 2,
	MLXSW_REG_RAUHT_OP_WRITE_DELETE = 3,
	MLXSW_REG_RAUHT_OP_WRITE_DELETE_ALL = 4,
};
MLXSW_ITEM32(reg, rauht, op, 0x00, 20, 3);
MLXSW_ITEM32(reg, rauht, a, 0x00, 16, 1);
MLXSW_ITEM32(reg, rauht, rif, 0x00, 0, 16);
MLXSW_ITEM32(reg, rauht, dip4, 0x1C, 0x0, 32);
MLXSW_ITEM_BUF(reg, rauht, dip6, 0x10, 16);
enum mlxsw_reg_rauht_trap_action {
	MLXSW_REG_RAUHT_TRAP_ACTION_NOP,
	MLXSW_REG_RAUHT_TRAP_ACTION_TRAP,
	MLXSW_REG_RAUHT_TRAP_ACTION_MIRROR_TO_CPU,
	MLXSW_REG_RAUHT_TRAP_ACTION_MIRROR,
	MLXSW_REG_RAUHT_TRAP_ACTION_DISCARD_ERRORS,
};
MLXSW_ITEM32(reg, rauht, trap_action, 0x60, 28, 4);
enum mlxsw_reg_rauht_trap_id {
	MLXSW_REG_RAUHT_TRAP_ID_RTR_EGRESS0,
	MLXSW_REG_RAUHT_TRAP_ID_RTR_EGRESS1,
};
MLXSW_ITEM32(reg, rauht, trap_id, 0x60, 0, 9);
MLXSW_ITEM32(reg, rauht, counter_set_type, 0x68, 24, 8);
MLXSW_ITEM32(reg, rauht, counter_index, 0x68, 0, 24);
MLXSW_ITEM_BUF(reg, rauht, mac, 0x6E, 6);
static inline void mlxsw_reg_rauht_pack(char *payload,
					enum mlxsw_reg_rauht_op op, u16 rif,
					const char *mac)
{
	MLXSW_REG_ZERO(rauht, payload);
	mlxsw_reg_rauht_op_set(payload, op);
	mlxsw_reg_rauht_rif_set(payload, rif);
	mlxsw_reg_rauht_mac_memcpy_to(payload, mac);
}
static inline void mlxsw_reg_rauht_pack4(char *payload,
					 enum mlxsw_reg_rauht_op op, u16 rif,
					 const char *mac, u32 dip)
{
	mlxsw_reg_rauht_pack(payload, op, rif, mac);
	mlxsw_reg_rauht_dip4_set(payload, dip);
}
static inline void mlxsw_reg_rauht_pack6(char *payload,
					 enum mlxsw_reg_rauht_op op, u16 rif,
					 const char *mac, const char *dip)
{
	mlxsw_reg_rauht_pack(payload, op, rif, mac);
	mlxsw_reg_rauht_type_set(payload, MLXSW_REG_RAUHT_TYPE_IPV6);
	mlxsw_reg_rauht_dip6_memcpy_to(payload, dip);
}
static inline void mlxsw_reg_rauht_pack_counter(char *payload,
						u64 counter_index)
{
	mlxsw_reg_rauht_counter_index_set(payload, counter_index);
	mlxsw_reg_rauht_counter_set_type_set(payload,
					     MLXSW_REG_FLOW_COUNTER_SET_TYPE_PACKETS_BYTES);
}
#define MLXSW_REG_RALEU_ID 0x8015
#define MLXSW_REG_RALEU_LEN 0x28
MLXSW_REG_DEFINE(raleu, MLXSW_REG_RALEU_ID, MLXSW_REG_RALEU_LEN);
MLXSW_ITEM32(reg, raleu, protocol, 0x00, 24, 4);
MLXSW_ITEM32(reg, raleu, virtual_router, 0x00, 0, 16);
MLXSW_ITEM32(reg, raleu, adjacency_index, 0x10, 0, 24);
MLXSW_ITEM32(reg, raleu, ecmp_size, 0x14, 0, 13);
MLXSW_ITEM32(reg, raleu, new_adjacency_index, 0x20, 0, 24);
MLXSW_ITEM32(reg, raleu, new_ecmp_size, 0x24, 0, 13);
static inline void mlxsw_reg_raleu_pack(char *payload,
					enum mlxsw_reg_ralxx_protocol protocol,
					u16 virtual_router,
					u32 adjacency_index, u16 ecmp_size,
					u32 new_adjacency_index,
					u16 new_ecmp_size)
{
	MLXSW_REG_ZERO(raleu, payload);
	mlxsw_reg_raleu_protocol_set(payload, protocol);
	mlxsw_reg_raleu_virtual_router_set(payload, virtual_router);
	mlxsw_reg_raleu_adjacency_index_set(payload, adjacency_index);
	mlxsw_reg_raleu_ecmp_size_set(payload, ecmp_size);
	mlxsw_reg_raleu_new_adjacency_index_set(payload, new_adjacency_index);
	mlxsw_reg_raleu_new_ecmp_size_set(payload, new_ecmp_size);
}
#define MLXSW_REG_RAUHTD_ID 0x8018
#define MLXSW_REG_RAUHTD_BASE_LEN 0x20
#define MLXSW_REG_RAUHTD_REC_LEN 0x20
#define MLXSW_REG_RAUHTD_REC_MAX_NUM 32
#define MLXSW_REG_RAUHTD_LEN (MLXSW_REG_RAUHTD_BASE_LEN + \
		MLXSW_REG_RAUHTD_REC_MAX_NUM * MLXSW_REG_RAUHTD_REC_LEN)
#define MLXSW_REG_RAUHTD_IPV4_ENT_PER_REC 4
MLXSW_REG_DEFINE(rauhtd, MLXSW_REG_RAUHTD_ID, MLXSW_REG_RAUHTD_LEN);
#define MLXSW_REG_RAUHTD_FILTER_A BIT(0)
#define MLXSW_REG_RAUHTD_FILTER_RIF BIT(3)
MLXSW_ITEM32(reg, rauhtd, filter_fields, 0x00, 0, 8);
enum mlxsw_reg_rauhtd_op {
	MLXSW_REG_RAUHTD_OP_DUMP,
	MLXSW_REG_RAUHTD_OP_DUMP_AND_CLEAR,
};
MLXSW_ITEM32(reg, rauhtd, op, 0x04, 24, 2);
MLXSW_ITEM32(reg, rauhtd, num_rec, 0x04, 0, 8);
MLXSW_ITEM32(reg, rauhtd, entry_a, 0x08, 16, 1);
enum mlxsw_reg_rauhtd_type {
	MLXSW_REG_RAUHTD_TYPE_IPV4,
	MLXSW_REG_RAUHTD_TYPE_IPV6,
};
MLXSW_ITEM32(reg, rauhtd, type, 0x08, 0, 4);
MLXSW_ITEM32(reg, rauhtd, entry_rif, 0x0C, 0, 16);
static inline void mlxsw_reg_rauhtd_pack(char *payload,
					 enum mlxsw_reg_rauhtd_type type)
{
	MLXSW_REG_ZERO(rauhtd, payload);
	mlxsw_reg_rauhtd_filter_fields_set(payload, MLXSW_REG_RAUHTD_FILTER_A);
	mlxsw_reg_rauhtd_op_set(payload, MLXSW_REG_RAUHTD_OP_DUMP_AND_CLEAR);
	mlxsw_reg_rauhtd_num_rec_set(payload, MLXSW_REG_RAUHTD_REC_MAX_NUM);
	mlxsw_reg_rauhtd_entry_a_set(payload, 1);
	mlxsw_reg_rauhtd_type_set(payload, type);
}
MLXSW_ITEM32_INDEXED(reg, rauhtd, ipv4_rec_num_entries,
		     MLXSW_REG_RAUHTD_BASE_LEN, 28, 2,
		     MLXSW_REG_RAUHTD_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, rauhtd, rec_type, MLXSW_REG_RAUHTD_BASE_LEN, 24, 2,
		     MLXSW_REG_RAUHTD_REC_LEN, 0x00, false);
#define MLXSW_REG_RAUHTD_IPV4_ENT_LEN 0x8
MLXSW_ITEM32_INDEXED(reg, rauhtd, ipv4_ent_a, MLXSW_REG_RAUHTD_BASE_LEN, 16, 1,
		     MLXSW_REG_RAUHTD_IPV4_ENT_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, rauhtd, ipv4_ent_rif, MLXSW_REG_RAUHTD_BASE_LEN, 0,
		     16, MLXSW_REG_RAUHTD_IPV4_ENT_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, rauhtd, ipv4_ent_dip, MLXSW_REG_RAUHTD_BASE_LEN, 0,
		     32, MLXSW_REG_RAUHTD_IPV4_ENT_LEN, 0x04, false);
#define MLXSW_REG_RAUHTD_IPV6_ENT_LEN 0x20
MLXSW_ITEM32_INDEXED(reg, rauhtd, ipv6_ent_a, MLXSW_REG_RAUHTD_BASE_LEN, 16, 1,
		     MLXSW_REG_RAUHTD_IPV6_ENT_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, rauhtd, ipv6_ent_rif, MLXSW_REG_RAUHTD_BASE_LEN, 0,
		     16, MLXSW_REG_RAUHTD_IPV6_ENT_LEN, 0x00, false);
MLXSW_ITEM_BUF_INDEXED(reg, rauhtd, ipv6_ent_dip, MLXSW_REG_RAUHTD_BASE_LEN,
		       16, MLXSW_REG_RAUHTD_IPV6_ENT_LEN, 0x10);
static inline void mlxsw_reg_rauhtd_ent_ipv4_unpack(char *payload,
						    int ent_index, u16 *p_rif,
						    u32 *p_dip)
{
	*p_rif = mlxsw_reg_rauhtd_ipv4_ent_rif_get(payload, ent_index);
	*p_dip = mlxsw_reg_rauhtd_ipv4_ent_dip_get(payload, ent_index);
}
static inline void mlxsw_reg_rauhtd_ent_ipv6_unpack(char *payload,
						    int rec_index, u16 *p_rif,
						    char *p_dip)
{
	*p_rif = mlxsw_reg_rauhtd_ipv6_ent_rif_get(payload, rec_index);
	mlxsw_reg_rauhtd_ipv6_ent_dip_memcpy_from(payload, rec_index, p_dip);
}
#define MLXSW_REG_RTDP_ID 0x8020
#define MLXSW_REG_RTDP_LEN 0x44
MLXSW_REG_DEFINE(rtdp, MLXSW_REG_RTDP_ID, MLXSW_REG_RTDP_LEN);
enum mlxsw_reg_rtdp_type {
	MLXSW_REG_RTDP_TYPE_NVE,
	MLXSW_REG_RTDP_TYPE_IPIP,
};
MLXSW_ITEM32(reg, rtdp, type, 0x00, 28, 4);
MLXSW_ITEM32(reg, rtdp, tunnel_index, 0x00, 0, 24);
MLXSW_ITEM32(reg, rtdp, egress_router_interface, 0x40, 0, 16);
MLXSW_ITEM32(reg, rtdp, ipip_irif, 0x04, 16, 16);
enum mlxsw_reg_rtdp_ipip_sip_check {
	MLXSW_REG_RTDP_IPIP_SIP_CHECK_NO,
	MLXSW_REG_RTDP_IPIP_SIP_CHECK_FILTER_IPV4,
	MLXSW_REG_RTDP_IPIP_SIP_CHECK_FILTER_IPV6 = 3,
};
MLXSW_ITEM32(reg, rtdp, ipip_sip_check, 0x04, 0, 3);
#define MLXSW_REG_RTDP_IPIP_TYPE_CHECK_ALLOW_IPIP	BIT(0)
#define MLXSW_REG_RTDP_IPIP_TYPE_CHECK_ALLOW_GRE	BIT(1)
#define MLXSW_REG_RTDP_IPIP_TYPE_CHECK_ALLOW_GRE_KEY	BIT(2)
MLXSW_ITEM32(reg, rtdp, ipip_type_check, 0x08, 24, 3);
MLXSW_ITEM32(reg, rtdp, ipip_gre_key_check, 0x08, 23, 1);
MLXSW_ITEM32(reg, rtdp, ipip_ipv4_usip, 0x0C, 0, 32);
MLXSW_ITEM32(reg, rtdp, ipip_ipv6_usip_ptr, 0x10, 0, 24);
MLXSW_ITEM32(reg, rtdp, ipip_expected_gre_key, 0x14, 0, 32);
static inline void mlxsw_reg_rtdp_pack(char *payload,
				       enum mlxsw_reg_rtdp_type type,
				       u32 tunnel_index)
{
	MLXSW_REG_ZERO(rtdp, payload);
	mlxsw_reg_rtdp_type_set(payload, type);
	mlxsw_reg_rtdp_tunnel_index_set(payload, tunnel_index);
}
static inline void
mlxsw_reg_rtdp_ipip_pack(char *payload, u16 irif,
			 enum mlxsw_reg_rtdp_ipip_sip_check sip_check,
			 unsigned int type_check, bool gre_key_check,
			 u32 expected_gre_key)
{
	mlxsw_reg_rtdp_ipip_irif_set(payload, irif);
	mlxsw_reg_rtdp_ipip_sip_check_set(payload, sip_check);
	mlxsw_reg_rtdp_ipip_type_check_set(payload, type_check);
	mlxsw_reg_rtdp_ipip_gre_key_check_set(payload, gre_key_check);
	mlxsw_reg_rtdp_ipip_expected_gre_key_set(payload, expected_gre_key);
}
static inline void
mlxsw_reg_rtdp_ipip4_pack(char *payload, u16 irif,
			  enum mlxsw_reg_rtdp_ipip_sip_check sip_check,
			  unsigned int type_check, bool gre_key_check,
			  u32 ipv4_usip, u32 expected_gre_key)
{
	mlxsw_reg_rtdp_ipip_pack(payload, irif, sip_check, type_check,
				 gre_key_check, expected_gre_key);
	mlxsw_reg_rtdp_ipip_ipv4_usip_set(payload, ipv4_usip);
}
static inline void
mlxsw_reg_rtdp_ipip6_pack(char *payload, u16 irif,
			  enum mlxsw_reg_rtdp_ipip_sip_check sip_check,
			  unsigned int type_check, bool gre_key_check,
			  u32 ipv6_usip_ptr, u32 expected_gre_key)
{
	mlxsw_reg_rtdp_ipip_pack(payload, irif, sip_check, type_check,
				 gre_key_check, expected_gre_key);
	mlxsw_reg_rtdp_ipip_ipv6_usip_ptr_set(payload, ipv6_usip_ptr);
}
#define MLXSW_REG_RIPS_ID 0x8021
#define MLXSW_REG_RIPS_LEN 0x14
MLXSW_REG_DEFINE(rips, MLXSW_REG_RIPS_ID, MLXSW_REG_RIPS_LEN);
MLXSW_ITEM32(reg, rips, index, 0x00, 0, 24);
MLXSW_ITEM_BUF(reg, rips, ipv6, 0x04, 16);
static inline void mlxsw_reg_rips_pack(char *payload, u32 index,
				       const struct in6_addr *ipv6)
{
	MLXSW_REG_ZERO(rips, payload);
	mlxsw_reg_rips_index_set(payload, index);
	mlxsw_reg_rips_ipv6_memcpy_to(payload, (const char *)ipv6);
}
#define MLXSW_REG_RATRAD_ID 0x8022
#define MLXSW_REG_RATRAD_LEN 0x210
MLXSW_REG_DEFINE(ratrad, MLXSW_REG_RATRAD_ID, MLXSW_REG_RATRAD_LEN);
enum {
	MLXSW_REG_RATRAD_OP_READ_ACTIVITY,
	MLXSW_REG_RATRAD_OP_READ_CLEAR_ACTIVITY,
};
MLXSW_ITEM32(reg, ratrad, op, 0x00, 30, 2);
MLXSW_ITEM32(reg, ratrad, ecmp_size, 0x00, 0, 13);
MLXSW_ITEM32(reg, ratrad, adjacency_index, 0x04, 0, 24);
MLXSW_ITEM_BIT_ARRAY(reg, ratrad, activity_vector, 0x10, 0x200, 1);
static inline void mlxsw_reg_ratrad_pack(char *payload, u32 adjacency_index,
					 u16 ecmp_size)
{
	MLXSW_REG_ZERO(ratrad, payload);
	mlxsw_reg_ratrad_op_set(payload,
				MLXSW_REG_RATRAD_OP_READ_CLEAR_ACTIVITY);
	mlxsw_reg_ratrad_ecmp_size_set(payload, ecmp_size);
	mlxsw_reg_ratrad_adjacency_index_set(payload, adjacency_index);
}
#define MLXSW_REG_RIGR2_ID 0x8023
#define MLXSW_REG_RIGR2_LEN 0xB0
#define MLXSW_REG_RIGR2_MAX_ERIFS 32
MLXSW_REG_DEFINE(rigr2, MLXSW_REG_RIGR2_ID, MLXSW_REG_RIGR2_LEN);
MLXSW_ITEM32(reg, rigr2, rigr_index, 0x04, 0, 24);
MLXSW_ITEM32(reg, rigr2, vnext, 0x08, 31, 1);
MLXSW_ITEM32(reg, rigr2, next_rigr_index, 0x08, 0, 24);
MLXSW_ITEM32(reg, rigr2, vrmid, 0x20, 31, 1);
MLXSW_ITEM32(reg, rigr2, rmid_index, 0x20, 0, 16);
MLXSW_ITEM32_INDEXED(reg, rigr2, erif_entry_v, 0x24, 31, 1, 4, 0, false);
MLXSW_ITEM32_INDEXED(reg, rigr2, erif_entry_erif, 0x24, 0, 16, 4, 0, false);
static inline void mlxsw_reg_rigr2_pack(char *payload, u32 rigr_index,
					bool vnext, u32 next_rigr_index)
{
	MLXSW_REG_ZERO(rigr2, payload);
	mlxsw_reg_rigr2_rigr_index_set(payload, rigr_index);
	mlxsw_reg_rigr2_vnext_set(payload, vnext);
	mlxsw_reg_rigr2_next_rigr_index_set(payload, next_rigr_index);
	mlxsw_reg_rigr2_vrmid_set(payload, 0);
	mlxsw_reg_rigr2_rmid_index_set(payload, 0);
}
static inline void mlxsw_reg_rigr2_erif_entry_pack(char *payload, int index,
						   bool v, u16 erif)
{
	mlxsw_reg_rigr2_erif_entry_v_set(payload, index, v);
	mlxsw_reg_rigr2_erif_entry_erif_set(payload, index, erif);
}
#define MLXSW_REG_RECR2_ID 0x8025
#define MLXSW_REG_RECR2_LEN 0x38
MLXSW_REG_DEFINE(recr2, MLXSW_REG_RECR2_ID, MLXSW_REG_RECR2_LEN);
MLXSW_ITEM32(reg, recr2, pp, 0x00, 24, 1);
MLXSW_ITEM32(reg, recr2, sh, 0x00, 8, 1);
MLXSW_ITEM32(reg, recr2, seed, 0x08, 0, 32);
enum {
	MLXSW_REG_RECR2_IPV4_EN_NOT_TCP_NOT_UDP	= 3,
	MLXSW_REG_RECR2_IPV4_EN_TCP_UDP		= 4,
	MLXSW_REG_RECR2_IPV6_EN_NOT_TCP_NOT_UDP	= 5,
	MLXSW_REG_RECR2_IPV6_EN_TCP_UDP		= 6,
	MLXSW_REG_RECR2_TCP_UDP_EN_IPV4		= 7,
	MLXSW_REG_RECR2_TCP_UDP_EN_IPV6		= 8,
	__MLXSW_REG_RECR2_HEADER_CNT,
};
MLXSW_ITEM_BIT_ARRAY(reg, recr2, outer_header_enables, 0x10, 0x04, 1);
enum {
	MLXSW_REG_RECR2_IPV4_SIP0			= 9,
	MLXSW_REG_RECR2_IPV4_SIP3			= 12,
	MLXSW_REG_RECR2_IPV4_DIP0			= 13,
	MLXSW_REG_RECR2_IPV4_DIP3			= 16,
	MLXSW_REG_RECR2_IPV4_PROTOCOL			= 17,
	MLXSW_REG_RECR2_IPV6_SIP0_7			= 21,
	MLXSW_REG_RECR2_IPV6_SIP8			= 29,
	MLXSW_REG_RECR2_IPV6_SIP15			= 36,
	MLXSW_REG_RECR2_IPV6_DIP0_7			= 37,
	MLXSW_REG_RECR2_IPV6_DIP8			= 45,
	MLXSW_REG_RECR2_IPV6_DIP15			= 52,
	MLXSW_REG_RECR2_IPV6_NEXT_HEADER		= 53,
	MLXSW_REG_RECR2_IPV6_FLOW_LABEL			= 57,
	MLXSW_REG_RECR2_TCP_UDP_SPORT			= 74,
	MLXSW_REG_RECR2_TCP_UDP_DPORT			= 75,
	__MLXSW_REG_RECR2_FIELD_CNT,
};
MLXSW_ITEM_BIT_ARRAY(reg, recr2, outer_header_fields_enable, 0x14, 0x14, 1);
MLXSW_ITEM_BIT_ARRAY(reg, recr2, inner_header_enables, 0x2C, 0x04, 1);
enum {
	MLXSW_REG_RECR2_INNER_IPV4_SIP0			= 3,
	MLXSW_REG_RECR2_INNER_IPV4_SIP3			= 6,
	MLXSW_REG_RECR2_INNER_IPV4_DIP0			= 7,
	MLXSW_REG_RECR2_INNER_IPV4_DIP3			= 10,
	MLXSW_REG_RECR2_INNER_IPV4_PROTOCOL		= 11,
	MLXSW_REG_RECR2_INNER_IPV6_SIP0_7		= 12,
	MLXSW_REG_RECR2_INNER_IPV6_SIP8			= 20,
	MLXSW_REG_RECR2_INNER_IPV6_SIP15		= 27,
	MLXSW_REG_RECR2_INNER_IPV6_DIP0_7		= 28,
	MLXSW_REG_RECR2_INNER_IPV6_DIP8			= 36,
	MLXSW_REG_RECR2_INNER_IPV6_DIP15		= 43,
	MLXSW_REG_RECR2_INNER_IPV6_NEXT_HEADER		= 44,
	MLXSW_REG_RECR2_INNER_IPV6_FLOW_LABEL		= 45,
	MLXSW_REG_RECR2_INNER_TCP_UDP_SPORT		= 46,
	MLXSW_REG_RECR2_INNER_TCP_UDP_DPORT		= 47,
	__MLXSW_REG_RECR2_INNER_FIELD_CNT,
};
MLXSW_ITEM_BIT_ARRAY(reg, recr2, inner_header_fields_enable, 0x30, 0x08, 1);
static inline void mlxsw_reg_recr2_pack(char *payload, u32 seed)
{
	MLXSW_REG_ZERO(recr2, payload);
	mlxsw_reg_recr2_pp_set(payload, false);
	mlxsw_reg_recr2_sh_set(payload, true);
	mlxsw_reg_recr2_seed_set(payload, seed);
}
#define MLXSW_REG_RMFT2_ID 0x8027
#define MLXSW_REG_RMFT2_LEN 0x174
MLXSW_REG_DEFINE(rmft2, MLXSW_REG_RMFT2_ID, MLXSW_REG_RMFT2_LEN);
MLXSW_ITEM32(reg, rmft2, v, 0x00, 31, 1);
enum mlxsw_reg_rmft2_type {
	MLXSW_REG_RMFT2_TYPE_IPV4,
	MLXSW_REG_RMFT2_TYPE_IPV6
};
MLXSW_ITEM32(reg, rmft2, type, 0x00, 28, 2);
enum mlxsw_sp_reg_rmft2_op {
	MLXSW_REG_RMFT2_OP_READ_WRITE,
};
MLXSW_ITEM32(reg, rmft2, op, 0x00, 20, 2);
MLXSW_ITEM32(reg, rmft2, a, 0x00, 16, 1);
MLXSW_ITEM32(reg, rmft2, offset, 0x00, 0, 16);
MLXSW_ITEM32(reg, rmft2, virtual_router, 0x04, 0, 16);
enum mlxsw_reg_rmft2_irif_mask {
	MLXSW_REG_RMFT2_IRIF_MASK_IGNORE,
	MLXSW_REG_RMFT2_IRIF_MASK_COMPARE
};
MLXSW_ITEM32(reg, rmft2, irif_mask, 0x08, 24, 1);
MLXSW_ITEM32(reg, rmft2, irif, 0x08, 0, 16);
MLXSW_ITEM_BUF(reg, rmft2, dip6, 0x10, 16);
MLXSW_ITEM32(reg, rmft2, dip4, 0x1C, 0, 32);
MLXSW_ITEM_BUF(reg, rmft2, dip6_mask, 0x20, 16);
MLXSW_ITEM32(reg, rmft2, dip4_mask, 0x2C, 0, 32);
MLXSW_ITEM_BUF(reg, rmft2, sip6, 0x30, 16);
MLXSW_ITEM32(reg, rmft2, sip4, 0x3C, 0, 32);
MLXSW_ITEM_BUF(reg, rmft2, sip6_mask, 0x40, 16);
MLXSW_ITEM32(reg, rmft2, sip4_mask, 0x4C, 0, 32);
MLXSW_ITEM_BUF(reg, rmft2, flexible_action_set, 0x80,
	       MLXSW_REG_FLEX_ACTION_SET_LEN);
static inline void
mlxsw_reg_rmft2_common_pack(char *payload, bool v, u16 offset,
			    u16 virtual_router,
			    enum mlxsw_reg_rmft2_irif_mask irif_mask, u16 irif,
			    const char *flex_action_set)
{
	MLXSW_REG_ZERO(rmft2, payload);
	mlxsw_reg_rmft2_v_set(payload, v);
	mlxsw_reg_rmft2_op_set(payload, MLXSW_REG_RMFT2_OP_READ_WRITE);
	mlxsw_reg_rmft2_offset_set(payload, offset);
	mlxsw_reg_rmft2_virtual_router_set(payload, virtual_router);
	mlxsw_reg_rmft2_irif_mask_set(payload, irif_mask);
	mlxsw_reg_rmft2_irif_set(payload, irif);
	if (flex_action_set)
		mlxsw_reg_rmft2_flexible_action_set_memcpy_to(payload,
							      flex_action_set);
}
static inline void
mlxsw_reg_rmft2_ipv4_pack(char *payload, bool v, u16 offset, u16 virtual_router,
			  enum mlxsw_reg_rmft2_irif_mask irif_mask, u16 irif,
			  u32 dip4, u32 dip4_mask, u32 sip4, u32 sip4_mask,
			  const char *flexible_action_set)
{
	mlxsw_reg_rmft2_common_pack(payload, v, offset, virtual_router,
				    irif_mask, irif, flexible_action_set);
	mlxsw_reg_rmft2_type_set(payload, MLXSW_REG_RMFT2_TYPE_IPV4);
	mlxsw_reg_rmft2_dip4_set(payload, dip4);
	mlxsw_reg_rmft2_dip4_mask_set(payload, dip4_mask);
	mlxsw_reg_rmft2_sip4_set(payload, sip4);
	mlxsw_reg_rmft2_sip4_mask_set(payload, sip4_mask);
}
static inline void
mlxsw_reg_rmft2_ipv6_pack(char *payload, bool v, u16 offset, u16 virtual_router,
			  enum mlxsw_reg_rmft2_irif_mask irif_mask, u16 irif,
			  struct in6_addr dip6, struct in6_addr dip6_mask,
			  struct in6_addr sip6, struct in6_addr sip6_mask,
			  const char *flexible_action_set)
{
	mlxsw_reg_rmft2_common_pack(payload, v, offset, virtual_router,
				    irif_mask, irif, flexible_action_set);
	mlxsw_reg_rmft2_type_set(payload, MLXSW_REG_RMFT2_TYPE_IPV6);
	mlxsw_reg_rmft2_dip6_memcpy_to(payload, (void *)&dip6);
	mlxsw_reg_rmft2_dip6_mask_memcpy_to(payload, (void *)&dip6_mask);
	mlxsw_reg_rmft2_sip6_memcpy_to(payload, (void *)&sip6);
	mlxsw_reg_rmft2_sip6_mask_memcpy_to(payload, (void *)&sip6_mask);
}
#define MLXSW_REG_REIV_ID 0x8034
#define MLXSW_REG_REIV_BASE_LEN 0x20  
#define MLXSW_REG_REIV_REC_LEN 0x04  
#define MLXSW_REG_REIV_REC_MAX_COUNT 256  
#define MLXSW_REG_REIV_LEN (MLXSW_REG_REIV_BASE_LEN +	\
			    MLXSW_REG_REIV_REC_LEN *	\
			    MLXSW_REG_REIV_REC_MAX_COUNT)
MLXSW_REG_DEFINE(reiv, MLXSW_REG_REIV_ID, MLXSW_REG_REIV_LEN);
MLXSW_ITEM32(reg, reiv, port_page, 0x00, 0, 4);
MLXSW_ITEM32(reg, reiv, erif, 0x04, 0, 16);
MLXSW_ITEM32_INDEXED(reg, reiv, rec_update, MLXSW_REG_REIV_BASE_LEN, 31, 1,
		     MLXSW_REG_REIV_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, reiv, rec_evid, MLXSW_REG_REIV_BASE_LEN, 0, 12,
		     MLXSW_REG_REIV_REC_LEN, 0x00, false);
static inline void mlxsw_reg_reiv_pack(char *payload, u8 port_page, u16 erif)
{
	MLXSW_REG_ZERO(reiv, payload);
	mlxsw_reg_reiv_port_page_set(payload, port_page);
	mlxsw_reg_reiv_erif_set(payload, erif);
}
#define MLXSW_REG_MFCR_ID 0x9001
#define MLXSW_REG_MFCR_LEN 0x08
MLXSW_REG_DEFINE(mfcr, MLXSW_REG_MFCR_ID, MLXSW_REG_MFCR_LEN);
enum mlxsw_reg_mfcr_pwm_frequency {
	MLXSW_REG_MFCR_PWM_FEQ_11HZ = 0x00,
	MLXSW_REG_MFCR_PWM_FEQ_14_7HZ = 0x01,
	MLXSW_REG_MFCR_PWM_FEQ_22_1HZ = 0x02,
	MLXSW_REG_MFCR_PWM_FEQ_1_4KHZ = 0x40,
	MLXSW_REG_MFCR_PWM_FEQ_5KHZ = 0x41,
	MLXSW_REG_MFCR_PWM_FEQ_20KHZ = 0x42,
	MLXSW_REG_MFCR_PWM_FEQ_22_5KHZ = 0x43,
	MLXSW_REG_MFCR_PWM_FEQ_25KHZ = 0x44,
};
MLXSW_ITEM32(reg, mfcr, pwm_frequency, 0x00, 0, 7);
#define MLXSW_MFCR_TACHOS_MAX 10
MLXSW_ITEM32(reg, mfcr, tacho_active, 0x04, 16, MLXSW_MFCR_TACHOS_MAX);
#define MLXSW_MFCR_PWMS_MAX 5
MLXSW_ITEM32(reg, mfcr, pwm_active, 0x04, 0, MLXSW_MFCR_PWMS_MAX);
static inline void
mlxsw_reg_mfcr_pack(char *payload,
		    enum mlxsw_reg_mfcr_pwm_frequency pwm_frequency)
{
	MLXSW_REG_ZERO(mfcr, payload);
	mlxsw_reg_mfcr_pwm_frequency_set(payload, pwm_frequency);
}
static inline void
mlxsw_reg_mfcr_unpack(char *payload,
		      enum mlxsw_reg_mfcr_pwm_frequency *p_pwm_frequency,
		      u16 *p_tacho_active, u8 *p_pwm_active)
{
	*p_pwm_frequency = mlxsw_reg_mfcr_pwm_frequency_get(payload);
	*p_tacho_active = mlxsw_reg_mfcr_tacho_active_get(payload);
	*p_pwm_active = mlxsw_reg_mfcr_pwm_active_get(payload);
}
#define MLXSW_REG_MFSC_ID 0x9002
#define MLXSW_REG_MFSC_LEN 0x08
MLXSW_REG_DEFINE(mfsc, MLXSW_REG_MFSC_ID, MLXSW_REG_MFSC_LEN);
MLXSW_ITEM32(reg, mfsc, pwm, 0x00, 24, 3);
MLXSW_ITEM32(reg, mfsc, pwm_duty_cycle, 0x04, 0, 8);
static inline void mlxsw_reg_mfsc_pack(char *payload, u8 pwm,
				       u8 pwm_duty_cycle)
{
	MLXSW_REG_ZERO(mfsc, payload);
	mlxsw_reg_mfsc_pwm_set(payload, pwm);
	mlxsw_reg_mfsc_pwm_duty_cycle_set(payload, pwm_duty_cycle);
}
#define MLXSW_REG_MFSM_ID 0x9003
#define MLXSW_REG_MFSM_LEN 0x08
MLXSW_REG_DEFINE(mfsm, MLXSW_REG_MFSM_ID, MLXSW_REG_MFSM_LEN);
MLXSW_ITEM32(reg, mfsm, tacho, 0x00, 24, 4);
MLXSW_ITEM32(reg, mfsm, rpm, 0x04, 0, 16);
static inline void mlxsw_reg_mfsm_pack(char *payload, u8 tacho)
{
	MLXSW_REG_ZERO(mfsm, payload);
	mlxsw_reg_mfsm_tacho_set(payload, tacho);
}
#define MLXSW_REG_MFSL_ID 0x9004
#define MLXSW_REG_MFSL_LEN 0x0C
MLXSW_REG_DEFINE(mfsl, MLXSW_REG_MFSL_ID, MLXSW_REG_MFSL_LEN);
MLXSW_ITEM32(reg, mfsl, tacho, 0x00, 24, 4);
MLXSW_ITEM32(reg, mfsl, tach_min, 0x04, 0, 16);
MLXSW_ITEM32(reg, mfsl, tach_max, 0x08, 0, 16);
static inline void mlxsw_reg_mfsl_pack(char *payload, u8 tacho,
				       u16 tach_min, u16 tach_max)
{
	MLXSW_REG_ZERO(mfsl, payload);
	mlxsw_reg_mfsl_tacho_set(payload, tacho);
	mlxsw_reg_mfsl_tach_min_set(payload, tach_min);
	mlxsw_reg_mfsl_tach_max_set(payload, tach_max);
}
static inline void mlxsw_reg_mfsl_unpack(char *payload, u8 tacho,
					 u16 *p_tach_min, u16 *p_tach_max)
{
	if (p_tach_min)
		*p_tach_min = mlxsw_reg_mfsl_tach_min_get(payload);
	if (p_tach_max)
		*p_tach_max = mlxsw_reg_mfsl_tach_max_get(payload);
}
#define MLXSW_REG_FORE_ID 0x9007
#define MLXSW_REG_FORE_LEN 0x0C
MLXSW_REG_DEFINE(fore, MLXSW_REG_FORE_ID, MLXSW_REG_FORE_LEN);
MLXSW_ITEM32(reg, fore, fan_under_limit, 0x00, 16, 10);
static inline void mlxsw_reg_fore_unpack(char *payload, u8 tacho,
					 bool *fault)
{
	u16 limit;
	if (fault) {
		limit = mlxsw_reg_fore_fan_under_limit_get(payload);
		*fault = limit & BIT(tacho);
	}
}
#define MLXSW_REG_MTCAP_ID 0x9009
#define MLXSW_REG_MTCAP_LEN 0x08
MLXSW_REG_DEFINE(mtcap, MLXSW_REG_MTCAP_ID, MLXSW_REG_MTCAP_LEN);
MLXSW_ITEM32(reg, mtcap, sensor_count, 0x00, 0, 7);
#define MLXSW_REG_MTMP_ID 0x900A
#define MLXSW_REG_MTMP_LEN 0x20
MLXSW_REG_DEFINE(mtmp, MLXSW_REG_MTMP_ID, MLXSW_REG_MTMP_LEN);
MLXSW_ITEM32(reg, mtmp, slot_index, 0x00, 16, 4);
#define MLXSW_REG_MTMP_MODULE_INDEX_MIN 64
#define MLXSW_REG_MTMP_GBOX_INDEX_MIN 256
MLXSW_ITEM32(reg, mtmp, sensor_index, 0x00, 0, 12);
#define MLXSW_REG_MTMP_TEMP_TO_MC(val) ({ typeof(val) v_ = (val); \
					  ((v_) >= 0) ? ((v_) * 125) : \
					  ((s16)((GENMASK(15, 0) + (v_) + 1) \
					   * 125)); })
MLXSW_ITEM32(reg, mtmp, max_operational_temperature, 0x04, 16, 16);
MLXSW_ITEM32(reg, mtmp, temperature, 0x04, 0, 16);
MLXSW_ITEM32(reg, mtmp, mte, 0x08, 31, 1);
MLXSW_ITEM32(reg, mtmp, mtr, 0x08, 30, 1);
MLXSW_ITEM32(reg, mtmp, max_temperature, 0x08, 0, 16);
enum mlxsw_reg_mtmp_tee {
	MLXSW_REG_MTMP_TEE_NO_EVENT,
	MLXSW_REG_MTMP_TEE_GENERATE_EVENT,
	MLXSW_REG_MTMP_TEE_GENERATE_SINGLE_EVENT,
};
MLXSW_ITEM32(reg, mtmp, tee, 0x0C, 30, 2);
#define MLXSW_REG_MTMP_THRESH_HI 0x348	 
MLXSW_ITEM32(reg, mtmp, temperature_threshold_hi, 0x0C, 0, 16);
#define MLXSW_REG_MTMP_HYSTERESIS_TEMP 0x28  
MLXSW_ITEM32(reg, mtmp, temperature_threshold_lo, 0x10, 0, 16);
#define MLXSW_REG_MTMP_SENSOR_NAME_SIZE 8
MLXSW_ITEM_BUF(reg, mtmp, sensor_name, 0x18, MLXSW_REG_MTMP_SENSOR_NAME_SIZE);
static inline void mlxsw_reg_mtmp_pack(char *payload, u8 slot_index,
				       u16 sensor_index, bool max_temp_enable,
				       bool max_temp_reset)
{
	MLXSW_REG_ZERO(mtmp, payload);
	mlxsw_reg_mtmp_slot_index_set(payload, slot_index);
	mlxsw_reg_mtmp_sensor_index_set(payload, sensor_index);
	mlxsw_reg_mtmp_mte_set(payload, max_temp_enable);
	mlxsw_reg_mtmp_mtr_set(payload, max_temp_reset);
	mlxsw_reg_mtmp_temperature_threshold_hi_set(payload,
						    MLXSW_REG_MTMP_THRESH_HI);
}
static inline void mlxsw_reg_mtmp_unpack(char *payload, int *p_temp,
					 int *p_max_temp, int *p_temp_hi,
					 int *p_max_oper_temp,
					 char *sensor_name)
{
	s16 temp;
	if (p_temp) {
		temp = mlxsw_reg_mtmp_temperature_get(payload);
		*p_temp = MLXSW_REG_MTMP_TEMP_TO_MC(temp);
	}
	if (p_max_temp) {
		temp = mlxsw_reg_mtmp_max_temperature_get(payload);
		*p_max_temp = MLXSW_REG_MTMP_TEMP_TO_MC(temp);
	}
	if (p_temp_hi) {
		temp = mlxsw_reg_mtmp_temperature_threshold_hi_get(payload);
		*p_temp_hi = MLXSW_REG_MTMP_TEMP_TO_MC(temp);
	}
	if (p_max_oper_temp) {
		temp = mlxsw_reg_mtmp_max_operational_temperature_get(payload);
		*p_max_oper_temp = MLXSW_REG_MTMP_TEMP_TO_MC(temp);
	}
	if (sensor_name)
		mlxsw_reg_mtmp_sensor_name_memcpy_from(payload, sensor_name);
}
#define MLXSW_REG_MTWE_ID 0x900B
#define MLXSW_REG_MTWE_LEN 0x10
MLXSW_REG_DEFINE(mtwe, MLXSW_REG_MTWE_ID, MLXSW_REG_MTWE_LEN);
MLXSW_ITEM_BIT_ARRAY(reg, mtwe, sensor_warning, 0x0, 0x10, 1);
#define MLXSW_REG_MTBR_ID 0x900F
#define MLXSW_REG_MTBR_BASE_LEN 0x10  
#define MLXSW_REG_MTBR_REC_LEN 0x04  
#define MLXSW_REG_MTBR_REC_MAX_COUNT 47  
#define MLXSW_REG_MTBR_LEN (MLXSW_REG_MTBR_BASE_LEN +	\
			    MLXSW_REG_MTBR_REC_LEN *	\
			    MLXSW_REG_MTBR_REC_MAX_COUNT)
MLXSW_REG_DEFINE(mtbr, MLXSW_REG_MTBR_ID, MLXSW_REG_MTBR_LEN);
MLXSW_ITEM32(reg, mtbr, slot_index, 0x00, 16, 4);
MLXSW_ITEM32(reg, mtbr, base_sensor_index, 0x00, 0, 12);
MLXSW_ITEM32(reg, mtbr, num_rec, 0x04, 0, 8);
MLXSW_ITEM32_INDEXED(reg, mtbr, rec_max_temp, MLXSW_REG_MTBR_BASE_LEN, 16,
		     16, MLXSW_REG_MTBR_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, mtbr, rec_temp, MLXSW_REG_MTBR_BASE_LEN, 0, 16,
		     MLXSW_REG_MTBR_REC_LEN, 0x00, false);
static inline void mlxsw_reg_mtbr_pack(char *payload, u8 slot_index,
				       u16 base_sensor_index, u8 num_rec)
{
	MLXSW_REG_ZERO(mtbr, payload);
	mlxsw_reg_mtbr_slot_index_set(payload, slot_index);
	mlxsw_reg_mtbr_base_sensor_index_set(payload, base_sensor_index);
	mlxsw_reg_mtbr_num_rec_set(payload, num_rec);
}
enum mlxsw_reg_mtbr_temp_status {
	MLXSW_REG_MTBR_NO_CONN		= 0x8000,
	MLXSW_REG_MTBR_NO_TEMP_SENS	= 0x8001,
	MLXSW_REG_MTBR_INDEX_NA		= 0x8002,
	MLXSW_REG_MTBR_BAD_SENS_INFO	= 0x8003,
};
#define MLXSW_REG_MTBR_BASE_MODULE_INDEX 64
static inline void mlxsw_reg_mtbr_temp_unpack(char *payload, int rec_ind,
					      u16 *p_temp, u16 *p_max_temp)
{
	if (p_temp)
		*p_temp = mlxsw_reg_mtbr_rec_temp_get(payload, rec_ind);
	if (p_max_temp)
		*p_max_temp = mlxsw_reg_mtbr_rec_max_temp_get(payload, rec_ind);
}
#define MLXSW_REG_MCIA_ID 0x9014
#define MLXSW_REG_MCIA_LEN 0x94
MLXSW_REG_DEFINE(mcia, MLXSW_REG_MCIA_ID, MLXSW_REG_MCIA_LEN);
MLXSW_ITEM32(reg, mcia, module, 0x00, 16, 8);
MLXSW_ITEM32(reg, mcia, slot, 0x00, 12, 4);
enum {
	MLXSW_REG_MCIA_STATUS_GOOD = 0,
	MLXSW_REG_MCIA_STATUS_NO_EEPROM_MODULE = 1,
	MLXSW_REG_MCIA_STATUS_MODULE_NOT_SUPPORTED = 2,
	MLXSW_REG_MCIA_STATUS_MODULE_NOT_CONNECTED = 3,
	MLXSW_REG_MCIA_STATUS_I2C_ERROR = 9,
	MLXSW_REG_MCIA_STATUS_MODULE_DISABLED = 16,
};
MLXSW_ITEM32(reg, mcia, status, 0x00, 0, 8);
MLXSW_ITEM32(reg, mcia, i2c_device_address, 0x04, 24, 8);
MLXSW_ITEM32(reg, mcia, page_number, 0x04, 16, 8);
MLXSW_ITEM32(reg, mcia, device_address, 0x04, 0, 16);
MLXSW_ITEM32(reg, mcia, bank_number, 0x08, 16, 8);
MLXSW_ITEM32(reg, mcia, size, 0x08, 0, 16);
#define MLXSW_REG_MCIA_EEPROM_PAGE_LENGTH	256
#define MLXSW_REG_MCIA_EEPROM_UP_PAGE_LENGTH	128
#define MLXSW_REG_MCIA_I2C_ADDR_LOW		0x50
#define MLXSW_REG_MCIA_I2C_ADDR_HIGH		0x51
#define MLXSW_REG_MCIA_PAGE0_LO_OFF		0xa0
#define MLXSW_REG_MCIA_TH_ITEM_SIZE		2
#define MLXSW_REG_MCIA_TH_PAGE_NUM		3
#define MLXSW_REG_MCIA_TH_PAGE_CMIS_NUM		2
#define MLXSW_REG_MCIA_PAGE0_LO			0
#define MLXSW_REG_MCIA_TH_PAGE_OFF		0x80
#define MLXSW_REG_MCIA_EEPROM_CMIS_FLAT_MEMORY	BIT(7)
enum mlxsw_reg_mcia_eeprom_module_info_rev_id {
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_REV_ID_UNSPC	= 0x00,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_REV_ID_8436	= 0x01,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_REV_ID_8636	= 0x03,
};
enum mlxsw_reg_mcia_eeprom_module_info_id {
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_ID_SFP	= 0x03,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_ID_QSFP	= 0x0C,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_ID_QSFP_PLUS	= 0x0D,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_ID_QSFP28	= 0x11,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_ID_QSFP_DD	= 0x18,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_ID_OSFP	= 0x19,
};
enum mlxsw_reg_mcia_eeprom_module_info {
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_ID,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_REV_ID,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_TYPE_ID,
	MLXSW_REG_MCIA_EEPROM_MODULE_INFO_SIZE,
};
MLXSW_ITEM_BUF(reg, mcia, eeprom, 0x10, 128);
#define MLXSW_REG_MCIA_PAGE_GET(off) (((off) - \
				MLXSW_REG_MCIA_EEPROM_PAGE_LENGTH) / \
				MLXSW_REG_MCIA_EEPROM_UP_PAGE_LENGTH + 1)
static inline void mlxsw_reg_mcia_pack(char *payload, u8 slot_index, u8 module,
				       u8 page_number, u16 device_addr, u8 size,
				       u8 i2c_device_addr)
{
	MLXSW_REG_ZERO(mcia, payload);
	mlxsw_reg_mcia_slot_set(payload, slot_index);
	mlxsw_reg_mcia_module_set(payload, module);
	mlxsw_reg_mcia_page_number_set(payload, page_number);
	mlxsw_reg_mcia_device_address_set(payload, device_addr);
	mlxsw_reg_mcia_size_set(payload, size);
	mlxsw_reg_mcia_i2c_device_address_set(payload, i2c_device_addr);
}
#define MLXSW_REG_MPAT_ID 0x901A
#define MLXSW_REG_MPAT_LEN 0x78
MLXSW_REG_DEFINE(mpat, MLXSW_REG_MPAT_ID, MLXSW_REG_MPAT_LEN);
MLXSW_ITEM32(reg, mpat, pa_id, 0x00, 28, 4);
MLXSW_ITEM32(reg, mpat, session_id, 0x00, 24, 4);
MLXSW_ITEM32(reg, mpat, system_port, 0x00, 0, 16);
MLXSW_ITEM32(reg, mpat, e, 0x04, 31, 1);
MLXSW_ITEM32(reg, mpat, qos, 0x04, 26, 1);
MLXSW_ITEM32(reg, mpat, be, 0x04, 25, 1);
enum mlxsw_reg_mpat_span_type {
	MLXSW_REG_MPAT_SPAN_TYPE_LOCAL_ETH = 0x0,
	MLXSW_REG_MPAT_SPAN_TYPE_REMOTE_ETH = 0x1,
	MLXSW_REG_MPAT_SPAN_TYPE_REMOTE_ETH_L3 = 0x3,
};
MLXSW_ITEM32(reg, mpat, span_type, 0x04, 0, 4);
MLXSW_ITEM32(reg, mpat, pide, 0x0C, 15, 1);
MLXSW_ITEM32(reg, mpat, pid, 0x0C, 0, 14);
MLXSW_ITEM32(reg, mpat, eth_rspan_vid, 0x18, 0, 12);
enum mlxsw_reg_mpat_eth_rspan_version {
	MLXSW_REG_MPAT_ETH_RSPAN_VERSION_NO_HEADER = 15,
};
MLXSW_ITEM32(reg, mpat, eth_rspan_version, 0x10, 18, 4);
MLXSW_ITEM_BUF(reg, mpat, eth_rspan_mac, 0x12, 6);
MLXSW_ITEM32(reg, mpat, eth_rspan_tp, 0x18, 16, 1);
enum mlxsw_reg_mpat_eth_rspan_protocol {
	MLXSW_REG_MPAT_ETH_RSPAN_PROTOCOL_IPV4,
	MLXSW_REG_MPAT_ETH_RSPAN_PROTOCOL_IPV6,
};
MLXSW_ITEM32(reg, mpat, eth_rspan_protocol, 0x18, 24, 4);
MLXSW_ITEM32(reg, mpat, eth_rspan_ttl, 0x1C, 4, 8);
MLXSW_ITEM_BUF(reg, mpat, eth_rspan_smac, 0x22, 6);
MLXSW_ITEM32(reg, mpat, eth_rspan_dip4, 0x4C, 0, 32);
MLXSW_ITEM_BUF(reg, mpat, eth_rspan_dip6, 0x40, 16);
MLXSW_ITEM32(reg, mpat, eth_rspan_sip4, 0x5C, 0, 32);
MLXSW_ITEM_BUF(reg, mpat, eth_rspan_sip6, 0x50, 16);
static inline void mlxsw_reg_mpat_pack(char *payload, u8 pa_id,
				       u16 system_port, bool e,
				       enum mlxsw_reg_mpat_span_type span_type)
{
	MLXSW_REG_ZERO(mpat, payload);
	mlxsw_reg_mpat_pa_id_set(payload, pa_id);
	mlxsw_reg_mpat_system_port_set(payload, system_port);
	mlxsw_reg_mpat_e_set(payload, e);
	mlxsw_reg_mpat_qos_set(payload, 1);
	mlxsw_reg_mpat_be_set(payload, 1);
	mlxsw_reg_mpat_span_type_set(payload, span_type);
}
static inline void mlxsw_reg_mpat_eth_rspan_pack(char *payload, u16 vid)
{
	mlxsw_reg_mpat_eth_rspan_vid_set(payload, vid);
}
static inline void
mlxsw_reg_mpat_eth_rspan_l2_pack(char *payload,
				 enum mlxsw_reg_mpat_eth_rspan_version version,
				 const char *mac,
				 bool tp)
{
	mlxsw_reg_mpat_eth_rspan_version_set(payload, version);
	mlxsw_reg_mpat_eth_rspan_mac_memcpy_to(payload, mac);
	mlxsw_reg_mpat_eth_rspan_tp_set(payload, tp);
}
static inline void
mlxsw_reg_mpat_eth_rspan_l3_ipv4_pack(char *payload, u8 ttl,
				      const char *smac,
				      u32 sip, u32 dip)
{
	mlxsw_reg_mpat_eth_rspan_ttl_set(payload, ttl);
	mlxsw_reg_mpat_eth_rspan_smac_memcpy_to(payload, smac);
	mlxsw_reg_mpat_eth_rspan_protocol_set(payload,
				    MLXSW_REG_MPAT_ETH_RSPAN_PROTOCOL_IPV4);
	mlxsw_reg_mpat_eth_rspan_sip4_set(payload, sip);
	mlxsw_reg_mpat_eth_rspan_dip4_set(payload, dip);
}
static inline void
mlxsw_reg_mpat_eth_rspan_l3_ipv6_pack(char *payload, u8 ttl,
				      const char *smac,
				      struct in6_addr sip, struct in6_addr dip)
{
	mlxsw_reg_mpat_eth_rspan_ttl_set(payload, ttl);
	mlxsw_reg_mpat_eth_rspan_smac_memcpy_to(payload, smac);
	mlxsw_reg_mpat_eth_rspan_protocol_set(payload,
				    MLXSW_REG_MPAT_ETH_RSPAN_PROTOCOL_IPV6);
	mlxsw_reg_mpat_eth_rspan_sip6_memcpy_to(payload, (void *)&sip);
	mlxsw_reg_mpat_eth_rspan_dip6_memcpy_to(payload, (void *)&dip);
}
#define MLXSW_REG_MPAR_ID 0x901B
#define MLXSW_REG_MPAR_LEN 0x0C
MLXSW_REG_DEFINE(mpar, MLXSW_REG_MPAR_ID, MLXSW_REG_MPAR_LEN);
MLXSW_ITEM32_LP(reg, mpar, 0x00, 16, 0x00, 4);
enum mlxsw_reg_mpar_i_e {
	MLXSW_REG_MPAR_TYPE_EGRESS,
	MLXSW_REG_MPAR_TYPE_INGRESS,
};
MLXSW_ITEM32(reg, mpar, i_e, 0x00, 0, 4);
MLXSW_ITEM32(reg, mpar, enable, 0x04, 31, 1);
MLXSW_ITEM32(reg, mpar, pa_id, 0x04, 0, 4);
#define MLXSW_REG_MPAR_RATE_MAX 3500000000UL
MLXSW_ITEM32(reg, mpar, probability_rate, 0x08, 0, 32);
static inline void mlxsw_reg_mpar_pack(char *payload, u16 local_port,
				       enum mlxsw_reg_mpar_i_e i_e,
				       bool enable, u8 pa_id,
				       u32 probability_rate)
{
	MLXSW_REG_ZERO(mpar, payload);
	mlxsw_reg_mpar_local_port_set(payload, local_port);
	mlxsw_reg_mpar_enable_set(payload, enable);
	mlxsw_reg_mpar_i_e_set(payload, i_e);
	mlxsw_reg_mpar_pa_id_set(payload, pa_id);
	mlxsw_reg_mpar_probability_rate_set(payload, probability_rate);
}
#define MLXSW_REG_MGIR_ID 0x9020
#define MLXSW_REG_MGIR_LEN 0x9C
MLXSW_REG_DEFINE(mgir, MLXSW_REG_MGIR_ID, MLXSW_REG_MGIR_LEN);
MLXSW_ITEM32(reg, mgir, hw_info_device_hw_revision, 0x0, 16, 16);
MLXSW_ITEM32(reg, mgir, fw_info_latency_tlv, 0x20, 29, 1);
MLXSW_ITEM32(reg, mgir, fw_info_string_tlv, 0x20, 28, 1);
#define MLXSW_REG_MGIR_FW_INFO_PSID_SIZE 16
MLXSW_ITEM_BUF(reg, mgir, fw_info_psid, 0x30, MLXSW_REG_MGIR_FW_INFO_PSID_SIZE);
MLXSW_ITEM32(reg, mgir, fw_info_extended_major, 0x44, 0, 32);
MLXSW_ITEM32(reg, mgir, fw_info_extended_minor, 0x48, 0, 32);
MLXSW_ITEM32(reg, mgir, fw_info_extended_sub_minor, 0x4C, 0, 32);
static inline void mlxsw_reg_mgir_pack(char *payload)
{
	MLXSW_REG_ZERO(mgir, payload);
}
static inline void
mlxsw_reg_mgir_unpack(char *payload, u32 *hw_rev, char *fw_info_psid,
		      u32 *fw_major, u32 *fw_minor, u32 *fw_sub_minor)
{
	*hw_rev = mlxsw_reg_mgir_hw_info_device_hw_revision_get(payload);
	mlxsw_reg_mgir_fw_info_psid_memcpy_from(payload, fw_info_psid);
	*fw_major = mlxsw_reg_mgir_fw_info_extended_major_get(payload);
	*fw_minor = mlxsw_reg_mgir_fw_info_extended_minor_get(payload);
	*fw_sub_minor = mlxsw_reg_mgir_fw_info_extended_sub_minor_get(payload);
}
#define MLXSW_REG_MRSR_ID 0x9023
#define MLXSW_REG_MRSR_LEN 0x08
MLXSW_REG_DEFINE(mrsr, MLXSW_REG_MRSR_ID, MLXSW_REG_MRSR_LEN);
MLXSW_ITEM32(reg, mrsr, command, 0x00, 0, 4);
static inline void mlxsw_reg_mrsr_pack(char *payload)
{
	MLXSW_REG_ZERO(mrsr, payload);
	mlxsw_reg_mrsr_command_set(payload, 1);
}
#define MLXSW_REG_MLCR_ID 0x902B
#define MLXSW_REG_MLCR_LEN 0x0C
MLXSW_REG_DEFINE(mlcr, MLXSW_REG_MLCR_ID, MLXSW_REG_MLCR_LEN);
MLXSW_ITEM32_LP(reg, mlcr, 0x00, 16, 0x00, 24);
#define MLXSW_REG_MLCR_DURATION_MAX 0xFFFF
MLXSW_ITEM32(reg, mlcr, beacon_duration, 0x04, 0, 16);
MLXSW_ITEM32(reg, mlcr, beacon_remain, 0x08, 0, 16);
static inline void mlxsw_reg_mlcr_pack(char *payload, u16 local_port,
				       bool active)
{
	MLXSW_REG_ZERO(mlcr, payload);
	mlxsw_reg_mlcr_local_port_set(payload, local_port);
	mlxsw_reg_mlcr_beacon_duration_set(payload, active ?
					   MLXSW_REG_MLCR_DURATION_MAX : 0);
}
#define MLXSW_REG_MCION_ID 0x9052
#define MLXSW_REG_MCION_LEN 0x18
MLXSW_REG_DEFINE(mcion, MLXSW_REG_MCION_ID, MLXSW_REG_MCION_LEN);
MLXSW_ITEM32(reg, mcion, module, 0x00, 16, 8);
MLXSW_ITEM32(reg, mcion, slot_index, 0x00, 12, 4);
enum {
	MLXSW_REG_MCION_MODULE_STATUS_BITS_PRESENT_MASK = BIT(0),
	MLXSW_REG_MCION_MODULE_STATUS_BITS_LOW_POWER_MASK = BIT(8),
};
MLXSW_ITEM32(reg, mcion, module_status_bits, 0x04, 0, 16);
static inline void mlxsw_reg_mcion_pack(char *payload, u8 slot_index, u8 module)
{
	MLXSW_REG_ZERO(mcion, payload);
	mlxsw_reg_mcion_slot_index_set(payload, slot_index);
	mlxsw_reg_mcion_module_set(payload, module);
}
#define MLXSW_REG_MTPPS_ID 0x9053
#define MLXSW_REG_MTPPS_LEN 0x3C
MLXSW_REG_DEFINE(mtpps, MLXSW_REG_MTPPS_ID, MLXSW_REG_MTPPS_LEN);
MLXSW_ITEM32(reg, mtpps, enable, 0x20, 31, 1);
enum mlxsw_reg_mtpps_pin_mode {
	MLXSW_REG_MTPPS_PIN_MODE_VIRTUAL_PIN = 0x2,
};
MLXSW_ITEM32(reg, mtpps, pin_mode, 0x20, 8, 4);
#define MLXSW_REG_MTPPS_PIN_SP_VIRTUAL_PIN	7
MLXSW_ITEM32(reg, mtpps, pin, 0x20, 0, 8);
MLXSW_ITEM64(reg, mtpps, time_stamp, 0x28, 0, 64);
static inline void
mlxsw_reg_mtpps_vpin_pack(char *payload, u64 time_stamp)
{
	MLXSW_REG_ZERO(mtpps, payload);
	mlxsw_reg_mtpps_pin_set(payload, MLXSW_REG_MTPPS_PIN_SP_VIRTUAL_PIN);
	mlxsw_reg_mtpps_pin_mode_set(payload,
				     MLXSW_REG_MTPPS_PIN_MODE_VIRTUAL_PIN);
	mlxsw_reg_mtpps_enable_set(payload, true);
	mlxsw_reg_mtpps_time_stamp_set(payload, time_stamp);
}
#define MLXSW_REG_MTUTC_ID 0x9055
#define MLXSW_REG_MTUTC_LEN 0x1C
MLXSW_REG_DEFINE(mtutc, MLXSW_REG_MTUTC_ID, MLXSW_REG_MTUTC_LEN);
enum mlxsw_reg_mtutc_operation {
	MLXSW_REG_MTUTC_OPERATION_SET_TIME_AT_NEXT_SEC = 0,
	MLXSW_REG_MTUTC_OPERATION_SET_TIME_IMMEDIATE = 1,
	MLXSW_REG_MTUTC_OPERATION_ADJUST_TIME = 2,
	MLXSW_REG_MTUTC_OPERATION_ADJUST_FREQ = 3,
};
MLXSW_ITEM32(reg, mtutc, operation, 0x00, 0, 4);
MLXSW_ITEM32(reg, mtutc, freq_adjustment, 0x04, 0, 32);
#define MLXSW_REG_MTUTC_MAX_FREQ_ADJ (50 * 1000 * 1000)
MLXSW_ITEM32(reg, mtutc, utc_sec, 0x10, 0, 32);
MLXSW_ITEM32(reg, mtutc, utc_nsec, 0x14, 0, 30);
MLXSW_ITEM32(reg, mtutc, time_adjustment, 0x18, 0, 32);
static inline void
mlxsw_reg_mtutc_pack(char *payload, enum mlxsw_reg_mtutc_operation oper,
		     u32 freq_adj, u32 utc_sec, u32 utc_nsec, u32 time_adj)
{
	MLXSW_REG_ZERO(mtutc, payload);
	mlxsw_reg_mtutc_operation_set(payload, oper);
	mlxsw_reg_mtutc_freq_adjustment_set(payload, freq_adj);
	mlxsw_reg_mtutc_utc_sec_set(payload, utc_sec);
	mlxsw_reg_mtutc_utc_nsec_set(payload, utc_nsec);
	mlxsw_reg_mtutc_time_adjustment_set(payload, time_adj);
}
#define MLXSW_REG_MCQI_ID 0x9061
#define MLXSW_REG_MCQI_BASE_LEN 0x18
#define MLXSW_REG_MCQI_CAP_LEN 0x14
#define MLXSW_REG_MCQI_LEN (MLXSW_REG_MCQI_BASE_LEN + MLXSW_REG_MCQI_CAP_LEN)
MLXSW_REG_DEFINE(mcqi, MLXSW_REG_MCQI_ID, MLXSW_REG_MCQI_LEN);
MLXSW_ITEM32(reg, mcqi, component_index, 0x00, 0, 16);
enum mlxfw_reg_mcqi_info_type {
	MLXSW_REG_MCQI_INFO_TYPE_CAPABILITIES,
};
MLXSW_ITEM32(reg, mcqi, info_type, 0x08, 0, 5);
MLXSW_ITEM32(reg, mcqi, offset, 0x10, 0, 32);
MLXSW_ITEM32(reg, mcqi, data_size, 0x14, 0, 16);
MLXSW_ITEM32(reg, mcqi, cap_max_component_size, 0x20, 0, 32);
MLXSW_ITEM32(reg, mcqi, cap_log_mcda_word_size, 0x24, 28, 4);
MLXSW_ITEM32(reg, mcqi, cap_mcda_max_write_size, 0x24, 0, 16);
static inline void mlxsw_reg_mcqi_pack(char *payload, u16 component_index)
{
	MLXSW_REG_ZERO(mcqi, payload);
	mlxsw_reg_mcqi_component_index_set(payload, component_index);
	mlxsw_reg_mcqi_info_type_set(payload,
				     MLXSW_REG_MCQI_INFO_TYPE_CAPABILITIES);
	mlxsw_reg_mcqi_offset_set(payload, 0);
	mlxsw_reg_mcqi_data_size_set(payload, MLXSW_REG_MCQI_CAP_LEN);
}
static inline void mlxsw_reg_mcqi_unpack(char *payload,
					 u32 *p_cap_max_component_size,
					 u8 *p_cap_log_mcda_word_size,
					 u16 *p_cap_mcda_max_write_size)
{
	*p_cap_max_component_size =
		mlxsw_reg_mcqi_cap_max_component_size_get(payload);
	*p_cap_log_mcda_word_size =
		mlxsw_reg_mcqi_cap_log_mcda_word_size_get(payload);
	*p_cap_mcda_max_write_size =
		mlxsw_reg_mcqi_cap_mcda_max_write_size_get(payload);
}
#define MLXSW_REG_MCC_ID 0x9062
#define MLXSW_REG_MCC_LEN 0x1C
MLXSW_REG_DEFINE(mcc, MLXSW_REG_MCC_ID, MLXSW_REG_MCC_LEN);
enum mlxsw_reg_mcc_instruction {
	MLXSW_REG_MCC_INSTRUCTION_LOCK_UPDATE_HANDLE = 0x01,
	MLXSW_REG_MCC_INSTRUCTION_RELEASE_UPDATE_HANDLE = 0x02,
	MLXSW_REG_MCC_INSTRUCTION_UPDATE_COMPONENT = 0x03,
	MLXSW_REG_MCC_INSTRUCTION_VERIFY_COMPONENT = 0x04,
	MLXSW_REG_MCC_INSTRUCTION_ACTIVATE = 0x06,
	MLXSW_REG_MCC_INSTRUCTION_CANCEL = 0x08,
};
MLXSW_ITEM32(reg, mcc, instruction, 0x00, 0, 8);
MLXSW_ITEM32(reg, mcc, component_index, 0x04, 0, 16);
MLXSW_ITEM32(reg, mcc, update_handle, 0x08, 0, 24);
MLXSW_ITEM32(reg, mcc, error_code, 0x0C, 8, 8);
MLXSW_ITEM32(reg, mcc, control_state, 0x0C, 0, 4);
MLXSW_ITEM32(reg, mcc, component_size, 0x10, 0, 32);
static inline void mlxsw_reg_mcc_pack(char *payload,
				      enum mlxsw_reg_mcc_instruction instr,
				      u16 component_index, u32 update_handle,
				      u32 component_size)
{
	MLXSW_REG_ZERO(mcc, payload);
	mlxsw_reg_mcc_instruction_set(payload, instr);
	mlxsw_reg_mcc_component_index_set(payload, component_index);
	mlxsw_reg_mcc_update_handle_set(payload, update_handle);
	mlxsw_reg_mcc_component_size_set(payload, component_size);
}
static inline void mlxsw_reg_mcc_unpack(char *payload, u32 *p_update_handle,
					u8 *p_error_code, u8 *p_control_state)
{
	if (p_update_handle)
		*p_update_handle = mlxsw_reg_mcc_update_handle_get(payload);
	if (p_error_code)
		*p_error_code = mlxsw_reg_mcc_error_code_get(payload);
	if (p_control_state)
		*p_control_state = mlxsw_reg_mcc_control_state_get(payload);
}
#define MLXSW_REG_MCDA_ID 0x9063
#define MLXSW_REG_MCDA_BASE_LEN 0x10
#define MLXSW_REG_MCDA_MAX_DATA_LEN 0x80
#define MLXSW_REG_MCDA_LEN \
		(MLXSW_REG_MCDA_BASE_LEN + MLXSW_REG_MCDA_MAX_DATA_LEN)
MLXSW_REG_DEFINE(mcda, MLXSW_REG_MCDA_ID, MLXSW_REG_MCDA_LEN);
MLXSW_ITEM32(reg, mcda, update_handle, 0x00, 0, 24);
MLXSW_ITEM32(reg, mcda, offset, 0x04, 0, 32);
MLXSW_ITEM32(reg, mcda, size, 0x08, 0, 16);
MLXSW_ITEM32_INDEXED(reg, mcda, data, 0x10, 0, 32, 4, 0, false);
static inline void mlxsw_reg_mcda_pack(char *payload, u32 update_handle,
				       u32 offset, u16 size, u8 *data)
{
	int i;
	MLXSW_REG_ZERO(mcda, payload);
	mlxsw_reg_mcda_update_handle_set(payload, update_handle);
	mlxsw_reg_mcda_offset_set(payload, offset);
	mlxsw_reg_mcda_size_set(payload, size);
	for (i = 0; i < size / 4; i++)
		mlxsw_reg_mcda_data_set(payload, i, *(u32 *) &data[i * 4]);
}
#define MLXSW_REG_MCAM_ID 0x907F
#define MLXSW_REG_MCAM_LEN 0x48
MLXSW_REG_DEFINE(mcam, MLXSW_REG_MCAM_ID, MLXSW_REG_MCAM_LEN);
enum mlxsw_reg_mcam_feature_group {
	MLXSW_REG_MCAM_FEATURE_GROUP_ENHANCED_FEATURES,
};
MLXSW_ITEM32(reg, mcam, feature_group, 0x00, 16, 8);
enum mlxsw_reg_mcam_mng_feature_cap_mask_bits {
	MLXSW_REG_MCAM_MCIA_128B = 34,
};
#define MLXSW_REG_BYTES_PER_DWORD 0x4
#define MLXSW_REG_MCAM_MNG_FEATURE_CAP_MASK_DWORD(_dw_num, _offset)	 \
	MLXSW_ITEM_BIT_ARRAY(reg, mcam, mng_feature_cap_mask_dw##_dw_num, \
			     _offset, MLXSW_REG_BYTES_PER_DWORD, 1)
MLXSW_REG_MCAM_MNG_FEATURE_CAP_MASK_DWORD(0, 0x28);
MLXSW_REG_MCAM_MNG_FEATURE_CAP_MASK_DWORD(1, 0x2C);
MLXSW_REG_MCAM_MNG_FEATURE_CAP_MASK_DWORD(2, 0x30);
MLXSW_REG_MCAM_MNG_FEATURE_CAP_MASK_DWORD(3, 0x34);
static inline void
mlxsw_reg_mcam_pack(char *payload, enum mlxsw_reg_mcam_feature_group feat_group)
{
	MLXSW_REG_ZERO(mcam, payload);
	mlxsw_reg_mcam_feature_group_set(payload, feat_group);
}
static inline void
mlxsw_reg_mcam_unpack(char *payload,
		      enum mlxsw_reg_mcam_mng_feature_cap_mask_bits bit,
		      bool *p_mng_feature_cap_val)
{
	int offset = bit % (MLXSW_REG_BYTES_PER_DWORD * BITS_PER_BYTE);
	int dword = bit / (MLXSW_REG_BYTES_PER_DWORD * BITS_PER_BYTE);
	u8 (*getters[])(const char *, u16) = {
		mlxsw_reg_mcam_mng_feature_cap_mask_dw0_get,
		mlxsw_reg_mcam_mng_feature_cap_mask_dw1_get,
		mlxsw_reg_mcam_mng_feature_cap_mask_dw2_get,
		mlxsw_reg_mcam_mng_feature_cap_mask_dw3_get,
	};
	if (!WARN_ON_ONCE(dword >= ARRAY_SIZE(getters)))
		*p_mng_feature_cap_val = getters[dword](payload, offset);
}
#define MLXSW_REG_MPSC_ID 0x9080
#define MLXSW_REG_MPSC_LEN 0x1C
MLXSW_REG_DEFINE(mpsc, MLXSW_REG_MPSC_ID, MLXSW_REG_MPSC_LEN);
MLXSW_ITEM32_LP(reg, mpsc, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, mpsc, e, 0x04, 30, 1);
#define MLXSW_REG_MPSC_RATE_MAX 3500000000UL
MLXSW_ITEM32(reg, mpsc, rate, 0x08, 0, 32);
static inline void mlxsw_reg_mpsc_pack(char *payload, u16 local_port, bool e,
				       u32 rate)
{
	MLXSW_REG_ZERO(mpsc, payload);
	mlxsw_reg_mpsc_local_port_set(payload, local_port);
	mlxsw_reg_mpsc_e_set(payload, e);
	mlxsw_reg_mpsc_rate_set(payload, rate);
}
#define MLXSW_REG_MGPC_ID 0x9081
#define MLXSW_REG_MGPC_LEN 0x18
MLXSW_REG_DEFINE(mgpc, MLXSW_REG_MGPC_ID, MLXSW_REG_MGPC_LEN);
MLXSW_ITEM32(reg, mgpc, counter_set_type, 0x00, 24, 8);
MLXSW_ITEM32(reg, mgpc, counter_index, 0x00, 0, 24);
enum mlxsw_reg_mgpc_opcode {
	MLXSW_REG_MGPC_OPCODE_NOP = 0x00,
	MLXSW_REG_MGPC_OPCODE_CLEAR = 0x08,
};
MLXSW_ITEM32(reg, mgpc, opcode, 0x04, 28, 4);
MLXSW_ITEM64(reg, mgpc, byte_counter, 0x08, 0, 64);
MLXSW_ITEM64(reg, mgpc, packet_counter, 0x10, 0, 64);
static inline void mlxsw_reg_mgpc_pack(char *payload, u32 counter_index,
				       enum mlxsw_reg_mgpc_opcode opcode,
				       enum mlxsw_reg_flow_counter_set_type set_type)
{
	MLXSW_REG_ZERO(mgpc, payload);
	mlxsw_reg_mgpc_counter_index_set(payload, counter_index);
	mlxsw_reg_mgpc_counter_set_type_set(payload, set_type);
	mlxsw_reg_mgpc_opcode_set(payload, opcode);
}
#define MLXSW_REG_MPRS_ID 0x9083
#define MLXSW_REG_MPRS_LEN 0x14
MLXSW_REG_DEFINE(mprs, MLXSW_REG_MPRS_ID, MLXSW_REG_MPRS_LEN);
MLXSW_ITEM32(reg, mprs, parsing_depth, 0x00, 0, 16);
MLXSW_ITEM32(reg, mprs, parsing_en, 0x04, 0, 16);
MLXSW_ITEM32(reg, mprs, vxlan_udp_dport, 0x10, 0, 16);
static inline void mlxsw_reg_mprs_pack(char *payload, u16 parsing_depth,
				       u16 vxlan_udp_dport)
{
	MLXSW_REG_ZERO(mprs, payload);
	mlxsw_reg_mprs_parsing_depth_set(payload, parsing_depth);
	mlxsw_reg_mprs_parsing_en_set(payload, true);
	mlxsw_reg_mprs_vxlan_udp_dport_set(payload, vxlan_udp_dport);
}
#define MLXSW_REG_MOGCR_ID 0x9086
#define MLXSW_REG_MOGCR_LEN 0x20
MLXSW_REG_DEFINE(mogcr, MLXSW_REG_MOGCR_ID, MLXSW_REG_MOGCR_LEN);
MLXSW_ITEM32(reg, mogcr, ptp_iftc, 0x00, 1, 1);
MLXSW_ITEM32(reg, mogcr, ptp_eftc, 0x00, 0, 1);
MLXSW_ITEM32(reg, mogcr, mirroring_pid_base, 0x0C, 0, 14);
#define MLXSW_REG_MPAGR_ID 0x9089
#define MLXSW_REG_MPAGR_LEN 0x0C
MLXSW_REG_DEFINE(mpagr, MLXSW_REG_MPAGR_ID, MLXSW_REG_MPAGR_LEN);
enum mlxsw_reg_mpagr_trigger {
	MLXSW_REG_MPAGR_TRIGGER_EGRESS,
	MLXSW_REG_MPAGR_TRIGGER_INGRESS,
	MLXSW_REG_MPAGR_TRIGGER_INGRESS_WRED,
	MLXSW_REG_MPAGR_TRIGGER_INGRESS_SHARED_BUFFER,
	MLXSW_REG_MPAGR_TRIGGER_INGRESS_ING_CONG,
	MLXSW_REG_MPAGR_TRIGGER_INGRESS_EGR_CONG,
	MLXSW_REG_MPAGR_TRIGGER_EGRESS_ECN,
	MLXSW_REG_MPAGR_TRIGGER_EGRESS_HIGH_LATENCY,
};
MLXSW_ITEM32(reg, mpagr, trigger, 0x00, 0, 4);
MLXSW_ITEM32(reg, mpagr, pa_id, 0x04, 0, 4);
#define MLXSW_REG_MPAGR_RATE_MAX 3500000000UL
MLXSW_ITEM32(reg, mpagr, probability_rate, 0x08, 0, 32);
static inline void mlxsw_reg_mpagr_pack(char *payload,
					enum mlxsw_reg_mpagr_trigger trigger,
					u8 pa_id, u32 probability_rate)
{
	MLXSW_REG_ZERO(mpagr, payload);
	mlxsw_reg_mpagr_trigger_set(payload, trigger);
	mlxsw_reg_mpagr_pa_id_set(payload, pa_id);
	mlxsw_reg_mpagr_probability_rate_set(payload, probability_rate);
}
#define MLXSW_REG_MOMTE_ID 0x908D
#define MLXSW_REG_MOMTE_LEN 0x10
MLXSW_REG_DEFINE(momte, MLXSW_REG_MOMTE_ID, MLXSW_REG_MOMTE_LEN);
MLXSW_ITEM32_LP(reg, momte, 0x00, 16, 0x00, 12);
enum mlxsw_reg_momte_type {
	MLXSW_REG_MOMTE_TYPE_WRED = 0x20,
	MLXSW_REG_MOMTE_TYPE_SHARED_BUFFER_TCLASS = 0x31,
	MLXSW_REG_MOMTE_TYPE_SHARED_BUFFER_TCLASS_DESCRIPTORS = 0x32,
	MLXSW_REG_MOMTE_TYPE_SHARED_BUFFER_EGRESS_PORT = 0x33,
	MLXSW_REG_MOMTE_TYPE_ING_CONG = 0x40,
	MLXSW_REG_MOMTE_TYPE_EGR_CONG = 0x50,
	MLXSW_REG_MOMTE_TYPE_ECN = 0x60,
	MLXSW_REG_MOMTE_TYPE_HIGH_LATENCY = 0x70,
};
MLXSW_ITEM32(reg, momte, type, 0x04, 0, 8);
MLXSW_ITEM_BIT_ARRAY(reg, momte, tclass_en, 0x08, 0x08, 1);
static inline void mlxsw_reg_momte_pack(char *payload, u16 local_port,
					enum mlxsw_reg_momte_type type)
{
	MLXSW_REG_ZERO(momte, payload);
	mlxsw_reg_momte_local_port_set(payload, local_port);
	mlxsw_reg_momte_type_set(payload, type);
}
#define MLXSW_REG_MTPPPC_ID 0x9090
#define MLXSW_REG_MTPPPC_LEN 0x28
MLXSW_REG_DEFINE(mtpppc, MLXSW_REG_MTPPPC_ID, MLXSW_REG_MTPPPC_LEN);
MLXSW_ITEM32(reg, mtpppc, ing_timestamp_message_type, 0x08, 0, 16);
MLXSW_ITEM32(reg, mtpppc, egr_timestamp_message_type, 0x0C, 0, 16);
static inline void mlxsw_reg_mtpppc_pack(char *payload, u16 ing, u16 egr)
{
	MLXSW_REG_ZERO(mtpppc, payload);
	mlxsw_reg_mtpppc_ing_timestamp_message_type_set(payload, ing);
	mlxsw_reg_mtpppc_egr_timestamp_message_type_set(payload, egr);
}
#define MLXSW_REG_MTPPTR_ID 0x9091
#define MLXSW_REG_MTPPTR_BASE_LEN 0x10  
#define MLXSW_REG_MTPPTR_REC_LEN 0x10  
#define MLXSW_REG_MTPPTR_REC_MAX_COUNT 4
#define MLXSW_REG_MTPPTR_LEN (MLXSW_REG_MTPPTR_BASE_LEN +		\
		    MLXSW_REG_MTPPTR_REC_LEN * MLXSW_REG_MTPPTR_REC_MAX_COUNT)
MLXSW_REG_DEFINE(mtpptr, MLXSW_REG_MTPPTR_ID, MLXSW_REG_MTPPTR_LEN);
MLXSW_ITEM32_LP(reg, mtpptr, 0x00, 16, 0x00, 12);
enum mlxsw_reg_mtpptr_dir {
	MLXSW_REG_MTPPTR_DIR_INGRESS,
	MLXSW_REG_MTPPTR_DIR_EGRESS,
};
MLXSW_ITEM32(reg, mtpptr, dir, 0x00, 0, 1);
MLXSW_ITEM32(reg, mtpptr, clr, 0x04, 31, 1);
MLXSW_ITEM32(reg, mtpptr, num_rec, 0x08, 0, 4);
MLXSW_ITEM32_INDEXED(reg, mtpptr, rec_message_type,
		     MLXSW_REG_MTPPTR_BASE_LEN, 8, 4,
		     MLXSW_REG_MTPPTR_REC_LEN, 0, false);
MLXSW_ITEM32_INDEXED(reg, mtpptr, rec_domain_number,
		     MLXSW_REG_MTPPTR_BASE_LEN, 0, 8,
		     MLXSW_REG_MTPPTR_REC_LEN, 0, false);
MLXSW_ITEM32_INDEXED(reg, mtpptr, rec_sequence_id,
		     MLXSW_REG_MTPPTR_BASE_LEN, 0, 16,
		     MLXSW_REG_MTPPTR_REC_LEN, 0x4, false);
MLXSW_ITEM32_INDEXED(reg, mtpptr, rec_timestamp_high,
		     MLXSW_REG_MTPPTR_BASE_LEN, 0, 32,
		     MLXSW_REG_MTPPTR_REC_LEN, 0x8, false);
MLXSW_ITEM32_INDEXED(reg, mtpptr, rec_timestamp_low,
		     MLXSW_REG_MTPPTR_BASE_LEN, 0, 32,
		     MLXSW_REG_MTPPTR_REC_LEN, 0xC, false);
static inline void mlxsw_reg_mtpptr_unpack(const char *payload,
					   unsigned int rec,
					   u8 *p_message_type,
					   u8 *p_domain_number,
					   u16 *p_sequence_id,
					   u64 *p_timestamp)
{
	u32 timestamp_high, timestamp_low;
	*p_message_type = mlxsw_reg_mtpptr_rec_message_type_get(payload, rec);
	*p_domain_number = mlxsw_reg_mtpptr_rec_domain_number_get(payload, rec);
	*p_sequence_id = mlxsw_reg_mtpptr_rec_sequence_id_get(payload, rec);
	timestamp_high = mlxsw_reg_mtpptr_rec_timestamp_high_get(payload, rec);
	timestamp_low = mlxsw_reg_mtpptr_rec_timestamp_low_get(payload, rec);
	*p_timestamp = (u64)timestamp_high << 32 | timestamp_low;
}
#define MLXSW_REG_MTPTPT_ID 0x9092
#define MLXSW_REG_MTPTPT_LEN 0x08
MLXSW_REG_DEFINE(mtptpt, MLXSW_REG_MTPTPT_ID, MLXSW_REG_MTPTPT_LEN);
enum mlxsw_reg_mtptpt_trap_id {
	MLXSW_REG_MTPTPT_TRAP_ID_PTP0,
	MLXSW_REG_MTPTPT_TRAP_ID_PTP1,
};
MLXSW_ITEM32(reg, mtptpt, trap_id, 0x00, 0, 4);
MLXSW_ITEM32(reg, mtptpt, message_type, 0x04, 0, 16);
static inline void mlxsw_reg_mtptpt_pack(char *payload,
					 enum mlxsw_reg_mtptpt_trap_id trap_id,
					 u16 message_type)
{
	MLXSW_REG_ZERO(mtptpt, payload);
	mlxsw_reg_mtptpt_trap_id_set(payload, trap_id);
	mlxsw_reg_mtptpt_message_type_set(payload, message_type);
}
#define MLXSW_REG_MTPCPC_ID 0x9093
#define MLXSW_REG_MTPCPC_LEN 0x2C
MLXSW_REG_DEFINE(mtpcpc, MLXSW_REG_MTPCPC_ID, MLXSW_REG_MTPCPC_LEN);
MLXSW_ITEM32(reg, mtpcpc, pport, 0x00, 31, 1);
MLXSW_ITEM32_LP(reg, mtpcpc, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, mtpcpc, ptp_trap_en, 0x04, 0, 1);
MLXSW_ITEM32(reg, mtpcpc, ing_correction_message_type, 0x10, 0, 16);
MLXSW_ITEM32(reg, mtpcpc, egr_correction_message_type, 0x14, 0, 16);
static inline void mlxsw_reg_mtpcpc_pack(char *payload, bool pport,
					 u16 local_port, bool ptp_trap_en,
					 u16 ing, u16 egr)
{
	MLXSW_REG_ZERO(mtpcpc, payload);
	mlxsw_reg_mtpcpc_pport_set(payload, pport);
	mlxsw_reg_mtpcpc_local_port_set(payload, pport ? local_port : 0);
	mlxsw_reg_mtpcpc_ptp_trap_en_set(payload, ptp_trap_en);
	mlxsw_reg_mtpcpc_ing_correction_message_type_set(payload, ing);
	mlxsw_reg_mtpcpc_egr_correction_message_type_set(payload, egr);
}
#define MLXSW_REG_MFGD_ID 0x90F0
#define MLXSW_REG_MFGD_LEN 0x0C
MLXSW_REG_DEFINE(mfgd, MLXSW_REG_MFGD_ID, MLXSW_REG_MFGD_LEN);
MLXSW_ITEM32(reg, mfgd, fatal_event_mode, 0x00, 9, 2);
MLXSW_ITEM32(reg, mfgd, trigger_test, 0x00, 11, 1);
#define MLXSW_REG_MGPIR_ID 0x9100
#define MLXSW_REG_MGPIR_LEN 0xA0
MLXSW_REG_DEFINE(mgpir, MLXSW_REG_MGPIR_ID, MLXSW_REG_MGPIR_LEN);
enum mlxsw_reg_mgpir_device_type {
	MLXSW_REG_MGPIR_DEVICE_TYPE_NONE,
	MLXSW_REG_MGPIR_DEVICE_TYPE_GEARBOX_DIE,
};
MLXSW_ITEM32(reg, mgpir, slot_index, 0x00, 28, 4);
MLXSW_ITEM32(reg, mgpir, device_type, 0x00, 24, 4);
MLXSW_ITEM32(reg, mgpir, devices_per_flash, 0x00, 16, 8);
MLXSW_ITEM32(reg, mgpir, num_of_devices, 0x00, 0, 8);
MLXSW_ITEM32(reg, mgpir, max_modules_per_slot, 0x04, 16, 8);
MLXSW_ITEM32(reg, mgpir, num_of_slots, 0x04, 8, 8);
MLXSW_ITEM32(reg, mgpir, num_of_modules, 0x04, 0, 8);
static inline void mlxsw_reg_mgpir_pack(char *payload, u8 slot_index)
{
	MLXSW_REG_ZERO(mgpir, payload);
	mlxsw_reg_mgpir_slot_index_set(payload, slot_index);
}
static inline void
mlxsw_reg_mgpir_unpack(char *payload, u8 *num_of_devices,
		       enum mlxsw_reg_mgpir_device_type *device_type,
		       u8 *devices_per_flash, u8 *num_of_modules,
		       u8 *num_of_slots)
{
	if (num_of_devices)
		*num_of_devices = mlxsw_reg_mgpir_num_of_devices_get(payload);
	if (device_type)
		*device_type = mlxsw_reg_mgpir_device_type_get(payload);
	if (devices_per_flash)
		*devices_per_flash =
				mlxsw_reg_mgpir_devices_per_flash_get(payload);
	if (num_of_modules)
		*num_of_modules = mlxsw_reg_mgpir_num_of_modules_get(payload);
	if (num_of_slots)
		*num_of_slots = mlxsw_reg_mgpir_num_of_slots_get(payload);
}
#define MLXSW_REG_MBCT_ID 0x9120
#define MLXSW_REG_MBCT_LEN 0x420
MLXSW_REG_DEFINE(mbct, MLXSW_REG_MBCT_ID, MLXSW_REG_MBCT_LEN);
MLXSW_ITEM32(reg, mbct, slot_index, 0x00, 0, 4);
MLXSW_ITEM32(reg, mbct, data_size, 0x04, 0, 11);
enum mlxsw_reg_mbct_op {
	MLXSW_REG_MBCT_OP_ERASE_INI_IMAGE = 1,
	MLXSW_REG_MBCT_OP_DATA_TRANSFER,  
	MLXSW_REG_MBCT_OP_ACTIVATE,
	MLXSW_REG_MBCT_OP_CLEAR_ERRORS = 6,
	MLXSW_REG_MBCT_OP_QUERY_STATUS,
};
MLXSW_ITEM32(reg, mbct, op, 0x08, 28, 4);
MLXSW_ITEM32(reg, mbct, last, 0x08, 26, 1);
MLXSW_ITEM32(reg, mbct, oee, 0x08, 25, 1);
enum mlxsw_reg_mbct_status {
	MLXSW_REG_MBCT_STATUS_PART_DATA = 2,
	MLXSW_REG_MBCT_STATUS_LAST_DATA,
	MLXSW_REG_MBCT_STATUS_ERASE_COMPLETE,
	MLXSW_REG_MBCT_STATUS_ERROR_INI_IN_USE,
	MLXSW_REG_MBCT_STATUS_ERASE_FAILED = 7,
	MLXSW_REG_MBCT_STATUS_INI_ERROR,
	MLXSW_REG_MBCT_STATUS_ACTIVATION_FAILED,
	MLXSW_REG_MBCT_STATUS_ILLEGAL_OPERATION = 11,
};
MLXSW_ITEM32(reg, mbct, status, 0x0C, 24, 5);
enum mlxsw_reg_mbct_fsm_state {
	MLXSW_REG_MBCT_FSM_STATE_INI_IN_USE = 5,
	MLXSW_REG_MBCT_FSM_STATE_ERROR,
};
MLXSW_ITEM32(reg, mbct, fsm_state,  0x0C, 16, 4);
#define MLXSW_REG_MBCT_DATA_LEN 1024
MLXSW_ITEM_BUF(reg, mbct, data, 0x20, MLXSW_REG_MBCT_DATA_LEN);
static inline void mlxsw_reg_mbct_pack(char *payload, u8 slot_index,
				       enum mlxsw_reg_mbct_op op, bool oee)
{
	MLXSW_REG_ZERO(mbct, payload);
	mlxsw_reg_mbct_slot_index_set(payload, slot_index);
	mlxsw_reg_mbct_op_set(payload, op);
	mlxsw_reg_mbct_oee_set(payload, oee);
}
static inline void mlxsw_reg_mbct_dt_pack(char *payload,
					  u16 data_size, bool last,
					  const char *data)
{
	if (WARN_ON(data_size > MLXSW_REG_MBCT_DATA_LEN))
		return;
	mlxsw_reg_mbct_data_size_set(payload, data_size);
	mlxsw_reg_mbct_last_set(payload, last);
	mlxsw_reg_mbct_data_memcpy_to(payload, data);
}
static inline void
mlxsw_reg_mbct_unpack(const char *payload, u8 *p_slot_index,
		      enum mlxsw_reg_mbct_status *p_status,
		      enum mlxsw_reg_mbct_fsm_state *p_fsm_state)
{
	if (p_slot_index)
		*p_slot_index = mlxsw_reg_mbct_slot_index_get(payload);
	*p_status = mlxsw_reg_mbct_status_get(payload);
	if (p_fsm_state)
		*p_fsm_state = mlxsw_reg_mbct_fsm_state_get(payload);
}
#define MLXSW_REG_MDDT_ID 0x9160
#define MLXSW_REG_MDDT_LEN 0x110
MLXSW_REG_DEFINE(mddt, MLXSW_REG_MDDT_ID, MLXSW_REG_MDDT_LEN);
MLXSW_ITEM32(reg, mddt, slot_index, 0x00, 8, 4);
MLXSW_ITEM32(reg, mddt, device_index, 0x00, 0, 8);
MLXSW_ITEM32(reg, mddt, read_size, 0x04, 24, 8);
MLXSW_ITEM32(reg, mddt, write_size, 0x04, 16, 8);
enum mlxsw_reg_mddt_status {
	MLXSW_REG_MDDT_STATUS_OK,
};
MLXSW_ITEM32(reg, mddt, status, 0x0C, 24, 8);
enum mlxsw_reg_mddt_method {
	MLXSW_REG_MDDT_METHOD_QUERY,
	MLXSW_REG_MDDT_METHOD_WRITE,
};
MLXSW_ITEM32(reg, mddt, method, 0x0C, 22, 2);
MLXSW_ITEM32(reg, mddt, register_id, 0x0C, 0, 16);
#define MLXSW_REG_MDDT_PAYLOAD_OFFSET 0x0C
#define MLXSW_REG_MDDT_PRM_REGISTER_HEADER_LEN 4
static inline char *mlxsw_reg_mddt_inner_payload(char *payload)
{
	return payload + MLXSW_REG_MDDT_PAYLOAD_OFFSET +
	       MLXSW_REG_MDDT_PRM_REGISTER_HEADER_LEN;
}
static inline void mlxsw_reg_mddt_pack(char *payload, u8 slot_index,
				       u8 device_index,
				       enum mlxsw_reg_mddt_method method,
				       const struct mlxsw_reg_info *reg,
				       char **inner_payload)
{
	int len = reg->len + MLXSW_REG_MDDT_PRM_REGISTER_HEADER_LEN;
	if (WARN_ON(len + MLXSW_REG_MDDT_PAYLOAD_OFFSET > MLXSW_REG_MDDT_LEN))
		len = MLXSW_REG_MDDT_LEN - MLXSW_REG_MDDT_PAYLOAD_OFFSET;
	MLXSW_REG_ZERO(mddt, payload);
	mlxsw_reg_mddt_slot_index_set(payload, slot_index);
	mlxsw_reg_mddt_device_index_set(payload, device_index);
	mlxsw_reg_mddt_method_set(payload, method);
	mlxsw_reg_mddt_register_id_set(payload, reg->id);
	mlxsw_reg_mddt_read_size_set(payload, len / 4);
	mlxsw_reg_mddt_write_size_set(payload, len / 4);
	*inner_payload = mlxsw_reg_mddt_inner_payload(payload);
}
#define MLXSW_REG_MDDQ_ID 0x9161
#define MLXSW_REG_MDDQ_LEN 0x30
MLXSW_REG_DEFINE(mddq, MLXSW_REG_MDDQ_ID, MLXSW_REG_MDDQ_LEN);
MLXSW_ITEM32(reg, mddq, sie, 0x00, 31, 1);
enum mlxsw_reg_mddq_query_type {
	MLXSW_REG_MDDQ_QUERY_TYPE_SLOT_INFO = 1,
	MLXSW_REG_MDDQ_QUERY_TYPE_DEVICE_INFO,  
	MLXSW_REG_MDDQ_QUERY_TYPE_SLOT_NAME,
};
MLXSW_ITEM32(reg, mddq, query_type, 0x00, 16, 8);
MLXSW_ITEM32(reg, mddq, slot_index, 0x00, 0, 4);
MLXSW_ITEM32(reg, mddq, response_msg_seq, 0x04, 16, 8);
MLXSW_ITEM32(reg, mddq, request_msg_seq, 0x04, 0, 8);
MLXSW_ITEM32(reg, mddq, data_valid, 0x08, 31, 1);
MLXSW_ITEM32(reg, mddq, slot_info_provisioned, 0x10, 31, 1);
MLXSW_ITEM32(reg, mddq, slot_info_sr_valid, 0x10, 30, 1);
enum mlxsw_reg_mddq_slot_info_ready {
	MLXSW_REG_MDDQ_SLOT_INFO_READY_NOT_READY,
	MLXSW_REG_MDDQ_SLOT_INFO_READY_READY,
	MLXSW_REG_MDDQ_SLOT_INFO_READY_ERROR,
};
MLXSW_ITEM32(reg, mddq, slot_info_lc_ready, 0x10, 28, 2);
MLXSW_ITEM32(reg, mddq, slot_info_active, 0x10, 27, 1);
MLXSW_ITEM32(reg, mddq, slot_info_hw_revision, 0x14, 16, 16);
MLXSW_ITEM32(reg, mddq, slot_info_ini_file_version, 0x14, 0, 16);
MLXSW_ITEM32(reg, mddq, slot_info_card_type, 0x18, 0, 8);
static inline void
__mlxsw_reg_mddq_pack(char *payload, u8 slot_index,
		      enum mlxsw_reg_mddq_query_type query_type)
{
	MLXSW_REG_ZERO(mddq, payload);
	mlxsw_reg_mddq_slot_index_set(payload, slot_index);
	mlxsw_reg_mddq_query_type_set(payload, query_type);
}
static inline void
mlxsw_reg_mddq_slot_info_pack(char *payload, u8 slot_index, bool sie)
{
	__mlxsw_reg_mddq_pack(payload, slot_index,
			      MLXSW_REG_MDDQ_QUERY_TYPE_SLOT_INFO);
	mlxsw_reg_mddq_sie_set(payload, sie);
}
static inline void
mlxsw_reg_mddq_slot_info_unpack(const char *payload, u8 *p_slot_index,
				bool *p_provisioned, bool *p_sr_valid,
				enum mlxsw_reg_mddq_slot_info_ready *p_lc_ready,
				bool *p_active, u16 *p_hw_revision,
				u16 *p_ini_file_version,
				u8 *p_card_type)
{
	*p_slot_index = mlxsw_reg_mddq_slot_index_get(payload);
	*p_provisioned = mlxsw_reg_mddq_slot_info_provisioned_get(payload);
	*p_sr_valid = mlxsw_reg_mddq_slot_info_sr_valid_get(payload);
	*p_lc_ready = mlxsw_reg_mddq_slot_info_lc_ready_get(payload);
	*p_active = mlxsw_reg_mddq_slot_info_active_get(payload);
	*p_hw_revision = mlxsw_reg_mddq_slot_info_hw_revision_get(payload);
	*p_ini_file_version = mlxsw_reg_mddq_slot_info_ini_file_version_get(payload);
	*p_card_type = mlxsw_reg_mddq_slot_info_card_type_get(payload);
}
MLXSW_ITEM32(reg, mddq, device_info_flash_owner, 0x10, 30, 1);
MLXSW_ITEM32(reg, mddq, device_info_device_index, 0x10, 0, 8);
MLXSW_ITEM32(reg, mddq, device_info_fw_major, 0x14, 16, 16);
MLXSW_ITEM32(reg, mddq, device_info_fw_minor, 0x18, 16, 16);
MLXSW_ITEM32(reg, mddq, device_info_fw_sub_minor, 0x18, 0, 16);
static inline void
mlxsw_reg_mddq_device_info_pack(char *payload, u8 slot_index,
				u8 request_msg_seq)
{
	__mlxsw_reg_mddq_pack(payload, slot_index,
			      MLXSW_REG_MDDQ_QUERY_TYPE_DEVICE_INFO);
	mlxsw_reg_mddq_request_msg_seq_set(payload, request_msg_seq);
}
static inline void
mlxsw_reg_mddq_device_info_unpack(const char *payload, u8 *p_response_msg_seq,
				  bool *p_data_valid, bool *p_flash_owner,
				  u8 *p_device_index, u16 *p_fw_major,
				  u16 *p_fw_minor, u16 *p_fw_sub_minor)
{
	*p_response_msg_seq = mlxsw_reg_mddq_response_msg_seq_get(payload);
	*p_data_valid = mlxsw_reg_mddq_data_valid_get(payload);
	*p_flash_owner = mlxsw_reg_mddq_device_info_flash_owner_get(payload);
	*p_device_index = mlxsw_reg_mddq_device_info_device_index_get(payload);
	*p_fw_major = mlxsw_reg_mddq_device_info_fw_major_get(payload);
	*p_fw_minor = mlxsw_reg_mddq_device_info_fw_minor_get(payload);
	*p_fw_sub_minor = mlxsw_reg_mddq_device_info_fw_sub_minor_get(payload);
}
#define MLXSW_REG_MDDQ_SLOT_ASCII_NAME_LEN 20
MLXSW_ITEM_BUF(reg, mddq, slot_ascii_name, 0x10,
	       MLXSW_REG_MDDQ_SLOT_ASCII_NAME_LEN);
static inline void
mlxsw_reg_mddq_slot_name_pack(char *payload, u8 slot_index)
{
	__mlxsw_reg_mddq_pack(payload, slot_index,
			      MLXSW_REG_MDDQ_QUERY_TYPE_SLOT_NAME);
}
static inline void
mlxsw_reg_mddq_slot_name_unpack(const char *payload, char *slot_ascii_name)
{
	mlxsw_reg_mddq_slot_ascii_name_memcpy_from(payload, slot_ascii_name);
}
#define MLXSW_REG_MDDC_ID 0x9163
#define MLXSW_REG_MDDC_LEN 0x30
MLXSW_REG_DEFINE(mddc, MLXSW_REG_MDDC_ID, MLXSW_REG_MDDC_LEN);
MLXSW_ITEM32(reg, mddc, slot_index, 0x00, 0, 4);
MLXSW_ITEM32(reg, mddc, rst, 0x04, 29, 1);
MLXSW_ITEM32(reg, mddc, device_enable, 0x04, 28, 1);
static inline void mlxsw_reg_mddc_pack(char *payload, u8 slot_index, bool rst,
				       bool device_enable)
{
	MLXSW_REG_ZERO(mddc, payload);
	mlxsw_reg_mddc_slot_index_set(payload, slot_index);
	mlxsw_reg_mddc_rst_set(payload, rst);
	mlxsw_reg_mddc_device_enable_set(payload, device_enable);
}
#define MLXSW_REG_MFDE_ID 0x9200
#define MLXSW_REG_MFDE_LEN 0x30
MLXSW_REG_DEFINE(mfde, MLXSW_REG_MFDE_ID, MLXSW_REG_MFDE_LEN);
MLXSW_ITEM32(reg, mfde, irisc_id, 0x00, 24, 8);
enum mlxsw_reg_mfde_severity {
	MLXSW_REG_MFDE_SEVERITY_FATL = 2,
	MLXSW_REG_MFDE_SEVERITY_NRML = 3,
	MLXSW_REG_MFDE_SEVERITY_INTR = 5,
};
MLXSW_ITEM32(reg, mfde, severity, 0x00, 16, 8);
enum mlxsw_reg_mfde_event_id {
	MLXSW_REG_MFDE_EVENT_ID_CRSPACE_TO = 1,
	MLXSW_REG_MFDE_EVENT_ID_KVD_IM_STOP,
	MLXSW_REG_MFDE_EVENT_ID_TEST,
	MLXSW_REG_MFDE_EVENT_ID_FW_ASSERT,
	MLXSW_REG_MFDE_EVENT_ID_FATAL_CAUSE,
};
MLXSW_ITEM32(reg, mfde, event_id, 0x00, 0, 16);
enum mlxsw_reg_mfde_method {
	MLXSW_REG_MFDE_METHOD_QUERY,
	MLXSW_REG_MFDE_METHOD_WRITE,
};
MLXSW_ITEM32(reg, mfde, method, 0x04, 29, 1);
MLXSW_ITEM32(reg, mfde, long_process, 0x04, 28, 1);
enum mlxsw_reg_mfde_command_type {
	MLXSW_REG_MFDE_COMMAND_TYPE_MAD,
	MLXSW_REG_MFDE_COMMAND_TYPE_EMAD,
	MLXSW_REG_MFDE_COMMAND_TYPE_CMDIF,
};
MLXSW_ITEM32(reg, mfde, command_type, 0x04, 24, 2);
MLXSW_ITEM32(reg, mfde, reg_attr_id, 0x04, 0, 16);
MLXSW_ITEM32(reg, mfde, crspace_to_log_address, 0x10, 0, 32);
MLXSW_ITEM32(reg, mfde, crspace_to_oe, 0x14, 24, 1);
MLXSW_ITEM32(reg, mfde, crspace_to_log_id, 0x14, 0, 4);
MLXSW_ITEM64(reg, mfde, crspace_to_log_ip, 0x18, 0, 64);
MLXSW_ITEM32(reg, mfde, kvd_im_stop_oe, 0x10, 24, 1);
MLXSW_ITEM32(reg, mfde, kvd_im_stop_pipes_mask, 0x10, 0, 16);
MLXSW_ITEM32(reg, mfde, fw_assert_var0, 0x10, 0, 32);
MLXSW_ITEM32(reg, mfde, fw_assert_var1, 0x14, 0, 32);
MLXSW_ITEM32(reg, mfde, fw_assert_var2, 0x18, 0, 32);
MLXSW_ITEM32(reg, mfde, fw_assert_var3, 0x1C, 0, 32);
MLXSW_ITEM32(reg, mfde, fw_assert_var4, 0x20, 0, 32);
MLXSW_ITEM32(reg, mfde, fw_assert_existptr, 0x24, 0, 32);
MLXSW_ITEM32(reg, mfde, fw_assert_callra, 0x28, 0, 32);
MLXSW_ITEM32(reg, mfde, fw_assert_oe, 0x2C, 24, 1);
MLXSW_ITEM32(reg, mfde, fw_assert_tile_v, 0x2C, 23, 1);
MLXSW_ITEM32(reg, mfde, fw_assert_tile_index, 0x2C, 16, 6);
MLXSW_ITEM32(reg, mfde, fw_assert_ext_synd, 0x2C, 0, 16);
MLXSW_ITEM32(reg, mfde, fatal_cause_id, 0x10, 0, 18);
MLXSW_ITEM32(reg, mfde, fatal_cause_tile_v, 0x14, 23, 1);
MLXSW_ITEM32(reg, mfde, fatal_cause_tile_index, 0x14, 16, 6);
#define MLXSW_REG_TNGCR_ID 0xA001
#define MLXSW_REG_TNGCR_LEN 0x44
MLXSW_REG_DEFINE(tngcr, MLXSW_REG_TNGCR_ID, MLXSW_REG_TNGCR_LEN);
enum mlxsw_reg_tngcr_type {
	MLXSW_REG_TNGCR_TYPE_VXLAN,
	MLXSW_REG_TNGCR_TYPE_VXLAN_GPE,
	MLXSW_REG_TNGCR_TYPE_GENEVE,
	MLXSW_REG_TNGCR_TYPE_NVGRE,
};
MLXSW_ITEM32(reg, tngcr, type, 0x00, 0, 4);
MLXSW_ITEM32(reg, tngcr, nve_valid, 0x04, 31, 1);
MLXSW_ITEM32(reg, tngcr, nve_ttl_uc, 0x04, 0, 8);
MLXSW_ITEM32(reg, tngcr, nve_ttl_mc, 0x08, 0, 8);
enum {
	MLXSW_REG_TNGCR_FL_NO_COPY,
	MLXSW_REG_TNGCR_FL_COPY,
};
MLXSW_ITEM32(reg, tngcr, nve_flc, 0x0C, 25, 1);
enum {
	MLXSW_REG_TNGCR_FL_NO_HASH,
	MLXSW_REG_TNGCR_FL_HASH,
};
MLXSW_ITEM32(reg, tngcr, nve_flh, 0x0C, 24, 1);
MLXSW_ITEM32(reg, tngcr, nve_fl_prefix, 0x0C, 8, 12);
MLXSW_ITEM32(reg, tngcr, nve_fl_suffix, 0x0C, 0, 8);
enum {
	MLXSW_REG_TNGCR_UDP_SPORT_NO_HASH,
	MLXSW_REG_TNGCR_UDP_SPORT_HASH,
};
MLXSW_ITEM32(reg, tngcr, nve_udp_sport_type, 0x10, 24, 1);
MLXSW_ITEM32(reg, tngcr, nve_udp_sport_prefix, 0x10, 8, 8);
MLXSW_ITEM32(reg, tngcr, nve_group_size_mc, 0x18, 0, 8);
MLXSW_ITEM32(reg, tngcr, nve_group_size_flood, 0x1C, 0, 8);
MLXSW_ITEM32(reg, tngcr, learn_enable, 0x20, 31, 1);
MLXSW_ITEM32(reg, tngcr, underlay_virtual_router, 0x20, 0, 16);
MLXSW_ITEM32(reg, tngcr, underlay_rif, 0x24, 0, 16);
MLXSW_ITEM32(reg, tngcr, usipv4, 0x28, 0, 32);
MLXSW_ITEM_BUF(reg, tngcr, usipv6, 0x30, 16);
static inline void mlxsw_reg_tngcr_pack(char *payload,
					enum mlxsw_reg_tngcr_type type,
					bool valid, u8 ttl)
{
	MLXSW_REG_ZERO(tngcr, payload);
	mlxsw_reg_tngcr_type_set(payload, type);
	mlxsw_reg_tngcr_nve_valid_set(payload, valid);
	mlxsw_reg_tngcr_nve_ttl_uc_set(payload, ttl);
	mlxsw_reg_tngcr_nve_ttl_mc_set(payload, ttl);
	mlxsw_reg_tngcr_nve_flc_set(payload, MLXSW_REG_TNGCR_FL_NO_COPY);
	mlxsw_reg_tngcr_nve_flh_set(payload, 0);
	mlxsw_reg_tngcr_nve_udp_sport_type_set(payload,
					       MLXSW_REG_TNGCR_UDP_SPORT_HASH);
	mlxsw_reg_tngcr_nve_udp_sport_prefix_set(payload, 0);
	mlxsw_reg_tngcr_nve_group_size_mc_set(payload, 1);
	mlxsw_reg_tngcr_nve_group_size_flood_set(payload, 1);
}
#define MLXSW_REG_TNUMT_ID 0xA003
#define MLXSW_REG_TNUMT_LEN 0x20
MLXSW_REG_DEFINE(tnumt, MLXSW_REG_TNUMT_ID, MLXSW_REG_TNUMT_LEN);
enum mlxsw_reg_tnumt_record_type {
	MLXSW_REG_TNUMT_RECORD_TYPE_IPV4,
	MLXSW_REG_TNUMT_RECORD_TYPE_IPV6,
	MLXSW_REG_TNUMT_RECORD_TYPE_LABEL,
};
MLXSW_ITEM32(reg, tnumt, record_type, 0x00, 28, 4);
MLXSW_ITEM32(reg, tnumt, tunnel_port, 0x00, 24, 4);
MLXSW_ITEM32(reg, tnumt, underlay_mc_ptr, 0x00, 0, 24);
MLXSW_ITEM32(reg, tnumt, vnext, 0x04, 31, 1);
MLXSW_ITEM32(reg, tnumt, next_underlay_mc_ptr, 0x04, 0, 24);
MLXSW_ITEM32(reg, tnumt, record_size, 0x08, 0, 3);
MLXSW_ITEM32_INDEXED(reg, tnumt, udip, 0x0C, 0, 32, 0x04, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, tnumt, udip_ptr, 0x0C, 0, 24, 0x04, 0x00, false);
static inline void mlxsw_reg_tnumt_pack(char *payload,
					enum mlxsw_reg_tnumt_record_type type,
					enum mlxsw_reg_tunnel_port tport,
					u32 underlay_mc_ptr, bool vnext,
					u32 next_underlay_mc_ptr,
					u8 record_size)
{
	MLXSW_REG_ZERO(tnumt, payload);
	mlxsw_reg_tnumt_record_type_set(payload, type);
	mlxsw_reg_tnumt_tunnel_port_set(payload, tport);
	mlxsw_reg_tnumt_underlay_mc_ptr_set(payload, underlay_mc_ptr);
	mlxsw_reg_tnumt_vnext_set(payload, vnext);
	mlxsw_reg_tnumt_next_underlay_mc_ptr_set(payload, next_underlay_mc_ptr);
	mlxsw_reg_tnumt_record_size_set(payload, record_size);
}
#define MLXSW_REG_TNQCR_ID 0xA010
#define MLXSW_REG_TNQCR_LEN 0x0C
MLXSW_REG_DEFINE(tnqcr, MLXSW_REG_TNQCR_ID, MLXSW_REG_TNQCR_LEN);
MLXSW_ITEM32(reg, tnqcr, enc_set_dscp, 0x04, 28, 1);
static inline void mlxsw_reg_tnqcr_pack(char *payload)
{
	MLXSW_REG_ZERO(tnqcr, payload);
	mlxsw_reg_tnqcr_enc_set_dscp_set(payload, 0);
}
#define MLXSW_REG_TNQDR_ID 0xA011
#define MLXSW_REG_TNQDR_LEN 0x08
MLXSW_REG_DEFINE(tnqdr, MLXSW_REG_TNQDR_ID, MLXSW_REG_TNQDR_LEN);
MLXSW_ITEM32_LP(reg, tnqdr, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, tnqdr, dscp, 0x04, 0, 6);
static inline void mlxsw_reg_tnqdr_pack(char *payload, u16 local_port)
{
	MLXSW_REG_ZERO(tnqdr, payload);
	mlxsw_reg_tnqdr_local_port_set(payload, local_port);
	mlxsw_reg_tnqdr_dscp_set(payload, 0);
}
#define MLXSW_REG_TNEEM_ID 0xA012
#define MLXSW_REG_TNEEM_LEN 0x0C
MLXSW_REG_DEFINE(tneem, MLXSW_REG_TNEEM_ID, MLXSW_REG_TNEEM_LEN);
MLXSW_ITEM32(reg, tneem, overlay_ecn, 0x04, 24, 2);
MLXSW_ITEM32(reg, tneem, underlay_ecn, 0x04, 16, 2);
static inline void mlxsw_reg_tneem_pack(char *payload, u8 overlay_ecn,
					u8 underlay_ecn)
{
	MLXSW_REG_ZERO(tneem, payload);
	mlxsw_reg_tneem_overlay_ecn_set(payload, overlay_ecn);
	mlxsw_reg_tneem_underlay_ecn_set(payload, underlay_ecn);
}
#define MLXSW_REG_TNDEM_ID 0xA013
#define MLXSW_REG_TNDEM_LEN 0x0C
MLXSW_REG_DEFINE(tndem, MLXSW_REG_TNDEM_ID, MLXSW_REG_TNDEM_LEN);
MLXSW_ITEM32(reg, tndem, underlay_ecn, 0x04, 24, 2);
MLXSW_ITEM32(reg, tndem, overlay_ecn, 0x04, 16, 2);
MLXSW_ITEM32(reg, tndem, eip_ecn, 0x04, 8, 2);
MLXSW_ITEM32(reg, tndem, trap_en, 0x08, 28, 4);
MLXSW_ITEM32(reg, tndem, trap_id, 0x08, 0, 9);
static inline void mlxsw_reg_tndem_pack(char *payload, u8 underlay_ecn,
					u8 overlay_ecn, u8 ecn, bool trap_en,
					u16 trap_id)
{
	MLXSW_REG_ZERO(tndem, payload);
	mlxsw_reg_tndem_underlay_ecn_set(payload, underlay_ecn);
	mlxsw_reg_tndem_overlay_ecn_set(payload, overlay_ecn);
	mlxsw_reg_tndem_eip_ecn_set(payload, ecn);
	mlxsw_reg_tndem_trap_en_set(payload, trap_en);
	mlxsw_reg_tndem_trap_id_set(payload, trap_id);
}
#define MLXSW_REG_TNPC_ID 0xA020
#define MLXSW_REG_TNPC_LEN 0x18
MLXSW_REG_DEFINE(tnpc, MLXSW_REG_TNPC_ID, MLXSW_REG_TNPC_LEN);
MLXSW_ITEM32(reg, tnpc, tunnel_port, 0x00, 0, 4);
MLXSW_ITEM32(reg, tnpc, learn_enable_v6, 0x04, 1, 1);
MLXSW_ITEM32(reg, tnpc, learn_enable_v4, 0x04, 0, 1);
static inline void mlxsw_reg_tnpc_pack(char *payload,
				       enum mlxsw_reg_tunnel_port tport,
				       bool learn_enable)
{
	MLXSW_REG_ZERO(tnpc, payload);
	mlxsw_reg_tnpc_tunnel_port_set(payload, tport);
	mlxsw_reg_tnpc_learn_enable_v4_set(payload, learn_enable);
	mlxsw_reg_tnpc_learn_enable_v6_set(payload, learn_enable);
}
#define MLXSW_REG_TIGCR_ID 0xA801
#define MLXSW_REG_TIGCR_LEN 0x10
MLXSW_REG_DEFINE(tigcr, MLXSW_REG_TIGCR_ID, MLXSW_REG_TIGCR_LEN);
MLXSW_ITEM32(reg, tigcr, ttlc, 0x04, 8, 1);
MLXSW_ITEM32(reg, tigcr, ttl_uc, 0x04, 0, 8);
static inline void mlxsw_reg_tigcr_pack(char *payload, bool ttlc, u8 ttl_uc)
{
	MLXSW_REG_ZERO(tigcr, payload);
	mlxsw_reg_tigcr_ttlc_set(payload, ttlc);
	mlxsw_reg_tigcr_ttl_uc_set(payload, ttl_uc);
}
#define MLXSW_REG_TIEEM_ID 0xA812
#define MLXSW_REG_TIEEM_LEN 0x0C
MLXSW_REG_DEFINE(tieem, MLXSW_REG_TIEEM_ID, MLXSW_REG_TIEEM_LEN);
MLXSW_ITEM32(reg, tieem, overlay_ecn, 0x04, 24, 2);
MLXSW_ITEM32(reg, tieem, underlay_ecn, 0x04, 16, 2);
static inline void mlxsw_reg_tieem_pack(char *payload, u8 overlay_ecn,
					u8 underlay_ecn)
{
	MLXSW_REG_ZERO(tieem, payload);
	mlxsw_reg_tieem_overlay_ecn_set(payload, overlay_ecn);
	mlxsw_reg_tieem_underlay_ecn_set(payload, underlay_ecn);
}
#define MLXSW_REG_TIDEM_ID 0xA813
#define MLXSW_REG_TIDEM_LEN 0x0C
MLXSW_REG_DEFINE(tidem, MLXSW_REG_TIDEM_ID, MLXSW_REG_TIDEM_LEN);
MLXSW_ITEM32(reg, tidem, underlay_ecn, 0x04, 24, 2);
MLXSW_ITEM32(reg, tidem, overlay_ecn, 0x04, 16, 2);
MLXSW_ITEM32(reg, tidem, eip_ecn, 0x04, 8, 2);
MLXSW_ITEM32(reg, tidem, trap_en, 0x08, 28, 4);
MLXSW_ITEM32(reg, tidem, trap_id, 0x08, 0, 9);
static inline void mlxsw_reg_tidem_pack(char *payload, u8 underlay_ecn,
					u8 overlay_ecn, u8 eip_ecn,
					bool trap_en, u16 trap_id)
{
	MLXSW_REG_ZERO(tidem, payload);
	mlxsw_reg_tidem_underlay_ecn_set(payload, underlay_ecn);
	mlxsw_reg_tidem_overlay_ecn_set(payload, overlay_ecn);
	mlxsw_reg_tidem_eip_ecn_set(payload, eip_ecn);
	mlxsw_reg_tidem_trap_en_set(payload, trap_en);
	mlxsw_reg_tidem_trap_id_set(payload, trap_id);
}
#define MLXSW_REG_SBPR_ID 0xB001
#define MLXSW_REG_SBPR_LEN 0x14
MLXSW_REG_DEFINE(sbpr, MLXSW_REG_SBPR_ID, MLXSW_REG_SBPR_LEN);
MLXSW_ITEM32(reg, sbpr, desc, 0x00, 31, 1);
enum mlxsw_reg_sbxx_dir {
	MLXSW_REG_SBXX_DIR_INGRESS,
	MLXSW_REG_SBXX_DIR_EGRESS,
};
MLXSW_ITEM32(reg, sbpr, dir, 0x00, 24, 2);
MLXSW_ITEM32(reg, sbpr, pool, 0x00, 0, 4);
MLXSW_ITEM32(reg, sbpr, infi_size, 0x04, 31, 1);
MLXSW_ITEM32(reg, sbpr, size, 0x04, 0, 24);
enum mlxsw_reg_sbpr_mode {
	MLXSW_REG_SBPR_MODE_STATIC,
	MLXSW_REG_SBPR_MODE_DYNAMIC,
};
MLXSW_ITEM32(reg, sbpr, mode, 0x08, 0, 4);
static inline void mlxsw_reg_sbpr_pack(char *payload, u8 pool,
				       enum mlxsw_reg_sbxx_dir dir,
				       enum mlxsw_reg_sbpr_mode mode, u32 size,
				       bool infi_size)
{
	MLXSW_REG_ZERO(sbpr, payload);
	mlxsw_reg_sbpr_pool_set(payload, pool);
	mlxsw_reg_sbpr_dir_set(payload, dir);
	mlxsw_reg_sbpr_mode_set(payload, mode);
	mlxsw_reg_sbpr_size_set(payload, size);
	mlxsw_reg_sbpr_infi_size_set(payload, infi_size);
}
#define MLXSW_REG_SBCM_ID 0xB002
#define MLXSW_REG_SBCM_LEN 0x28
MLXSW_REG_DEFINE(sbcm, MLXSW_REG_SBCM_ID, MLXSW_REG_SBCM_LEN);
MLXSW_ITEM32_LP(reg, sbcm, 0x00, 16, 0x00, 4);
MLXSW_ITEM32(reg, sbcm, pg_buff, 0x00, 8, 6);
MLXSW_ITEM32(reg, sbcm, dir, 0x00, 0, 2);
MLXSW_ITEM32(reg, sbcm, min_buff, 0x18, 0, 24);
#define MLXSW_REG_SBXX_DYN_MAX_BUFF_MIN 1
#define MLXSW_REG_SBXX_DYN_MAX_BUFF_MAX 14
MLXSW_ITEM32(reg, sbcm, infi_max, 0x1C, 31, 1);
MLXSW_ITEM32(reg, sbcm, max_buff, 0x1C, 0, 24);
MLXSW_ITEM32(reg, sbcm, pool, 0x24, 0, 4);
static inline void mlxsw_reg_sbcm_pack(char *payload, u16 local_port, u8 pg_buff,
				       enum mlxsw_reg_sbxx_dir dir,
				       u32 min_buff, u32 max_buff,
				       bool infi_max, u8 pool)
{
	MLXSW_REG_ZERO(sbcm, payload);
	mlxsw_reg_sbcm_local_port_set(payload, local_port);
	mlxsw_reg_sbcm_pg_buff_set(payload, pg_buff);
	mlxsw_reg_sbcm_dir_set(payload, dir);
	mlxsw_reg_sbcm_min_buff_set(payload, min_buff);
	mlxsw_reg_sbcm_max_buff_set(payload, max_buff);
	mlxsw_reg_sbcm_infi_max_set(payload, infi_max);
	mlxsw_reg_sbcm_pool_set(payload, pool);
}
#define MLXSW_REG_SBPM_ID 0xB003
#define MLXSW_REG_SBPM_LEN 0x28
MLXSW_REG_DEFINE(sbpm, MLXSW_REG_SBPM_ID, MLXSW_REG_SBPM_LEN);
MLXSW_ITEM32_LP(reg, sbpm, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, sbpm, pool, 0x00, 8, 4);
MLXSW_ITEM32(reg, sbpm, dir, 0x00, 0, 2);
MLXSW_ITEM32(reg, sbpm, buff_occupancy, 0x10, 0, 24);
MLXSW_ITEM32(reg, sbpm, clr, 0x14, 31, 1);
MLXSW_ITEM32(reg, sbpm, max_buff_occupancy, 0x14, 0, 24);
MLXSW_ITEM32(reg, sbpm, min_buff, 0x18, 0, 24);
MLXSW_ITEM32(reg, sbpm, max_buff, 0x1C, 0, 24);
static inline void mlxsw_reg_sbpm_pack(char *payload, u16 local_port, u8 pool,
				       enum mlxsw_reg_sbxx_dir dir, bool clr,
				       u32 min_buff, u32 max_buff)
{
	MLXSW_REG_ZERO(sbpm, payload);
	mlxsw_reg_sbpm_local_port_set(payload, local_port);
	mlxsw_reg_sbpm_pool_set(payload, pool);
	mlxsw_reg_sbpm_dir_set(payload, dir);
	mlxsw_reg_sbpm_clr_set(payload, clr);
	mlxsw_reg_sbpm_min_buff_set(payload, min_buff);
	mlxsw_reg_sbpm_max_buff_set(payload, max_buff);
}
static inline void mlxsw_reg_sbpm_unpack(char *payload, u32 *p_buff_occupancy,
					 u32 *p_max_buff_occupancy)
{
	*p_buff_occupancy = mlxsw_reg_sbpm_buff_occupancy_get(payload);
	*p_max_buff_occupancy = mlxsw_reg_sbpm_max_buff_occupancy_get(payload);
}
#define MLXSW_REG_SBMM_ID 0xB004
#define MLXSW_REG_SBMM_LEN 0x28
MLXSW_REG_DEFINE(sbmm, MLXSW_REG_SBMM_ID, MLXSW_REG_SBMM_LEN);
MLXSW_ITEM32(reg, sbmm, prio, 0x00, 8, 4);
MLXSW_ITEM32(reg, sbmm, min_buff, 0x18, 0, 24);
MLXSW_ITEM32(reg, sbmm, max_buff, 0x1C, 0, 24);
MLXSW_ITEM32(reg, sbmm, pool, 0x24, 0, 4);
static inline void mlxsw_reg_sbmm_pack(char *payload, u8 prio, u32 min_buff,
				       u32 max_buff, u8 pool)
{
	MLXSW_REG_ZERO(sbmm, payload);
	mlxsw_reg_sbmm_prio_set(payload, prio);
	mlxsw_reg_sbmm_min_buff_set(payload, min_buff);
	mlxsw_reg_sbmm_max_buff_set(payload, max_buff);
	mlxsw_reg_sbmm_pool_set(payload, pool);
}
#define MLXSW_REG_SBSR_ID 0xB005
#define MLXSW_REG_SBSR_BASE_LEN 0x5C  
#define MLXSW_REG_SBSR_REC_LEN 0x8  
#define MLXSW_REG_SBSR_REC_MAX_COUNT 120
#define MLXSW_REG_SBSR_LEN (MLXSW_REG_SBSR_BASE_LEN +	\
			    MLXSW_REG_SBSR_REC_LEN *	\
			    MLXSW_REG_SBSR_REC_MAX_COUNT)
MLXSW_REG_DEFINE(sbsr, MLXSW_REG_SBSR_ID, MLXSW_REG_SBSR_LEN);
MLXSW_ITEM32(reg, sbsr, clr, 0x00, 31, 1);
#define MLXSW_REG_SBSR_NUM_PORTS_IN_PAGE 256
MLXSW_ITEM32(reg, sbsr, port_page, 0x04, 0, 4);
MLXSW_ITEM_BIT_ARRAY(reg, sbsr, ingress_port_mask, 0x10, 0x20, 1);
MLXSW_ITEM_BIT_ARRAY(reg, sbsr, pg_buff_mask, 0x30, 0x4, 1);
MLXSW_ITEM_BIT_ARRAY(reg, sbsr, egress_port_mask, 0x34, 0x20, 1);
MLXSW_ITEM_BIT_ARRAY(reg, sbsr, tclass_mask, 0x54, 0x8, 1);
static inline void mlxsw_reg_sbsr_pack(char *payload, bool clr)
{
	MLXSW_REG_ZERO(sbsr, payload);
	mlxsw_reg_sbsr_clr_set(payload, clr);
}
MLXSW_ITEM32_INDEXED(reg, sbsr, rec_buff_occupancy, MLXSW_REG_SBSR_BASE_LEN,
		     0, 24, MLXSW_REG_SBSR_REC_LEN, 0x00, false);
MLXSW_ITEM32_INDEXED(reg, sbsr, rec_max_buff_occupancy, MLXSW_REG_SBSR_BASE_LEN,
		     0, 24, MLXSW_REG_SBSR_REC_LEN, 0x04, false);
static inline void mlxsw_reg_sbsr_rec_unpack(char *payload, int rec_index,
					     u32 *p_buff_occupancy,
					     u32 *p_max_buff_occupancy)
{
	*p_buff_occupancy =
		mlxsw_reg_sbsr_rec_buff_occupancy_get(payload, rec_index);
	*p_max_buff_occupancy =
		mlxsw_reg_sbsr_rec_max_buff_occupancy_get(payload, rec_index);
}
#define MLXSW_REG_SBIB_ID 0xB006
#define MLXSW_REG_SBIB_LEN 0x10
MLXSW_REG_DEFINE(sbib, MLXSW_REG_SBIB_ID, MLXSW_REG_SBIB_LEN);
MLXSW_ITEM32_LP(reg, sbib, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, sbib, buff_size, 0x08, 0, 24);
static inline void mlxsw_reg_sbib_pack(char *payload, u16 local_port,
				       u32 buff_size)
{
	MLXSW_REG_ZERO(sbib, payload);
	mlxsw_reg_sbib_local_port_set(payload, local_port);
	mlxsw_reg_sbib_buff_size_set(payload, buff_size);
}
static const struct mlxsw_reg_info *mlxsw_reg_infos[] = {
	MLXSW_REG(sgcr),
	MLXSW_REG(spad),
	MLXSW_REG(sspr),
	MLXSW_REG(sfdat),
	MLXSW_REG(sfd),
	MLXSW_REG(sfn),
	MLXSW_REG(spms),
	MLXSW_REG(spvid),
	MLXSW_REG(spvm),
	MLXSW_REG(spaft),
	MLXSW_REG(sfgc),
	MLXSW_REG(sfdf),
	MLXSW_REG(sldr),
	MLXSW_REG(slcr),
	MLXSW_REG(slcor),
	MLXSW_REG(spmlr),
	MLXSW_REG(svfa),
	MLXSW_REG(spvtr),
	MLXSW_REG(svpe),
	MLXSW_REG(sfmr),
	MLXSW_REG(spvmlr),
	MLXSW_REG(spfsr),
	MLXSW_REG(spvc),
	MLXSW_REG(spevet),
	MLXSW_REG(smpe),
	MLXSW_REG(smid2),
	MLXSW_REG(cwtp),
	MLXSW_REG(cwtpm),
	MLXSW_REG(pgcr),
	MLXSW_REG(ppbt),
	MLXSW_REG(pacl),
	MLXSW_REG(pagt),
	MLXSW_REG(ptar),
	MLXSW_REG(pprr),
	MLXSW_REG(ppbs),
	MLXSW_REG(prcr),
	MLXSW_REG(pefa),
	MLXSW_REG(pemrbt),
	MLXSW_REG(ptce2),
	MLXSW_REG(perpt),
	MLXSW_REG(peabfe),
	MLXSW_REG(perar),
	MLXSW_REG(ptce3),
	MLXSW_REG(percr),
	MLXSW_REG(pererp),
	MLXSW_REG(iedr),
	MLXSW_REG(qpts),
	MLXSW_REG(qpcr),
	MLXSW_REG(qtct),
	MLXSW_REG(qeec),
	MLXSW_REG(qrwe),
	MLXSW_REG(qpdsm),
	MLXSW_REG(qpdp),
	MLXSW_REG(qpdpm),
	MLXSW_REG(qtctm),
	MLXSW_REG(qpsc),
	MLXSW_REG(pmlp),
	MLXSW_REG(pmtu),
	MLXSW_REG(ptys),
	MLXSW_REG(ppad),
	MLXSW_REG(paos),
	MLXSW_REG(pfcc),
	MLXSW_REG(ppcnt),
	MLXSW_REG(pptb),
	MLXSW_REG(pbmc),
	MLXSW_REG(pspa),
	MLXSW_REG(pmaos),
	MLXSW_REG(pplr),
	MLXSW_REG(pmtdb),
	MLXSW_REG(pmecr),
	MLXSW_REG(pmpe),
	MLXSW_REG(pddr),
	MLXSW_REG(pmmp),
	MLXSW_REG(pllp),
	MLXSW_REG(pmtm),
	MLXSW_REG(htgt),
	MLXSW_REG(hpkt),
	MLXSW_REG(rgcr),
	MLXSW_REG(ritr),
	MLXSW_REG(rtar),
	MLXSW_REG(ratr),
	MLXSW_REG(rtdp),
	MLXSW_REG(rips),
	MLXSW_REG(ratrad),
	MLXSW_REG(rdpm),
	MLXSW_REG(ricnt),
	MLXSW_REG(rrcr),
	MLXSW_REG(ralta),
	MLXSW_REG(ralst),
	MLXSW_REG(raltb),
	MLXSW_REG(ralue),
	MLXSW_REG(rauht),
	MLXSW_REG(raleu),
	MLXSW_REG(rauhtd),
	MLXSW_REG(rigr2),
	MLXSW_REG(recr2),
	MLXSW_REG(rmft2),
	MLXSW_REG(reiv),
	MLXSW_REG(mfcr),
	MLXSW_REG(mfsc),
	MLXSW_REG(mfsm),
	MLXSW_REG(mfsl),
	MLXSW_REG(fore),
	MLXSW_REG(mtcap),
	MLXSW_REG(mtmp),
	MLXSW_REG(mtwe),
	MLXSW_REG(mtbr),
	MLXSW_REG(mcia),
	MLXSW_REG(mpat),
	MLXSW_REG(mpar),
	MLXSW_REG(mgir),
	MLXSW_REG(mrsr),
	MLXSW_REG(mlcr),
	MLXSW_REG(mcion),
	MLXSW_REG(mtpps),
	MLXSW_REG(mtutc),
	MLXSW_REG(mcqi),
	MLXSW_REG(mcc),
	MLXSW_REG(mcda),
	MLXSW_REG(mcam),
	MLXSW_REG(mpsc),
	MLXSW_REG(mgpc),
	MLXSW_REG(mprs),
	MLXSW_REG(mogcr),
	MLXSW_REG(mpagr),
	MLXSW_REG(momte),
	MLXSW_REG(mtpppc),
	MLXSW_REG(mtpptr),
	MLXSW_REG(mtptpt),
	MLXSW_REG(mtpcpc),
	MLXSW_REG(mfgd),
	MLXSW_REG(mgpir),
	MLXSW_REG(mbct),
	MLXSW_REG(mddt),
	MLXSW_REG(mddq),
	MLXSW_REG(mddc),
	MLXSW_REG(mfde),
	MLXSW_REG(tngcr),
	MLXSW_REG(tnumt),
	MLXSW_REG(tnqcr),
	MLXSW_REG(tnqdr),
	MLXSW_REG(tneem),
	MLXSW_REG(tndem),
	MLXSW_REG(tnpc),
	MLXSW_REG(tigcr),
	MLXSW_REG(tieem),
	MLXSW_REG(tidem),
	MLXSW_REG(sbpr),
	MLXSW_REG(sbcm),
	MLXSW_REG(sbpm),
	MLXSW_REG(sbmm),
	MLXSW_REG(sbsr),
	MLXSW_REG(sbib),
};
static inline const char *mlxsw_reg_id_str(u16 reg_id)
{
	const struct mlxsw_reg_info *reg_info;
	int i;
	for (i = 0; i < ARRAY_SIZE(mlxsw_reg_infos); i++) {
		reg_info = mlxsw_reg_infos[i];
		if (reg_info->id == reg_id)
			return reg_info->name;
	}
	return "*UNKNOWN*";
}
#define MLXSW_REG_PUDE_LEN 0x10
MLXSW_ITEM32(reg, pude, swid, 0x00, 24, 8);
MLXSW_ITEM32_LP(reg, pude, 0x00, 16, 0x00, 12);
MLXSW_ITEM32(reg, pude, admin_status, 0x00, 8, 4);
MLXSW_ITEM32(reg, pude, oper_status, 0x00, 0, 4);
#endif
