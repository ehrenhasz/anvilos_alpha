import sys
from ctypes import CDLL, Structure, create_string_buffer, addressof, sizeof, \
		   c_void_p, c_bool, c_byte, c_char, c_int, c_uint, c_longlong, c_ulonglong
class xed_state_t(Structure):
	_fields_ = [
		("mode", c_int),
		("width", c_int)
	]
class XEDInstruction():
	def __init__(self, libxed):
		xedd_t = c_byte * 512
		self.xedd = xedd_t()
		self.xedp = addressof(self.xedd)
		libxed.xed_decoded_inst_zero(self.xedp)
		self.state = xed_state_t()
		self.statep = addressof(self.state)
		self.buffer = create_string_buffer(256)
		self.bufferp = addressof(self.buffer)
class LibXED():
	def __init__(self):
		try:
			self.libxed = CDLL("libxed.so")
		except:
			self.libxed = None
		if not self.libxed:
			self.libxed = CDLL("/usr/local/lib/libxed.so")
		self.xed_tables_init = self.libxed.xed_tables_init
		self.xed_tables_init.restype = None
		self.xed_tables_init.argtypes = []
		self.xed_decoded_inst_zero = self.libxed.xed_decoded_inst_zero
		self.xed_decoded_inst_zero.restype = None
		self.xed_decoded_inst_zero.argtypes = [ c_void_p ]
		self.xed_operand_values_set_mode = self.libxed.xed_operand_values_set_mode
		self.xed_operand_values_set_mode.restype = None
		self.xed_operand_values_set_mode.argtypes = [ c_void_p, c_void_p ]
		self.xed_decoded_inst_zero_keep_mode = self.libxed.xed_decoded_inst_zero_keep_mode
		self.xed_decoded_inst_zero_keep_mode.restype = None
		self.xed_decoded_inst_zero_keep_mode.argtypes = [ c_void_p ]
		self.xed_decode = self.libxed.xed_decode
		self.xed_decode.restype = c_int
		self.xed_decode.argtypes = [ c_void_p, c_void_p, c_uint ]
		self.xed_format_context = self.libxed.xed_format_context
		self.xed_format_context.restype = c_uint
		self.xed_format_context.argtypes = [ c_int, c_void_p, c_void_p, c_int, c_ulonglong, c_void_p, c_void_p ]
		self.xed_tables_init()
	def Instruction(self):
		return XEDInstruction(self)
	def SetMode(self, inst, mode):
		if mode:
			inst.state.mode = 4 # 32-bit
			inst.state.width = 4 # 4 bytes
		else:
			inst.state.mode = 1 # 64-bit
			inst.state.width = 8 # 8 bytes
		self.xed_operand_values_set_mode(inst.xedp, inst.statep)
	def DisassembleOne(self, inst, bytes_ptr, bytes_cnt, ip):
		self.xed_decoded_inst_zero_keep_mode(inst.xedp)
		err = self.xed_decode(inst.xedp, bytes_ptr, bytes_cnt)
		if err:
			return 0, ""
		ok = self.xed_format_context(2, inst.xedp, inst.bufferp, sizeof(inst.buffer), ip, 0, 0)
		if not ok:
			return 0, ""
		if sys.version_info[0] == 2:
			result = inst.buffer.value
		else:
			result = inst.buffer.value.decode()
		return inst.xedd[166], result
