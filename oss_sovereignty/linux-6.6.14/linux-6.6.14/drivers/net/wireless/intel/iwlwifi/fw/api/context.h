#ifndef __iwl_fw_api_context_h__
#define __iwl_fw_api_context_h__
enum iwl_ctxt_id_and_color {
	FW_CTXT_ID_POS		= 0,
	FW_CTXT_ID_MSK		= 0xff << FW_CTXT_ID_POS,
	FW_CTXT_COLOR_POS	= 8,
	FW_CTXT_COLOR_MSK	= 0xff << FW_CTXT_COLOR_POS,
	FW_CTXT_INVALID		= 0xffffffff,
};
#define FW_CMD_ID_AND_COLOR(_id, _color) (((_id) << FW_CTXT_ID_POS) |\
					  ((_color) << FW_CTXT_COLOR_POS))
enum iwl_ctxt_action {
	FW_CTXT_ACTION_INVALID = 0,
	FW_CTXT_ACTION_ADD,
	FW_CTXT_ACTION_MODIFY,
	FW_CTXT_ACTION_REMOVE,
};  
#endif  
