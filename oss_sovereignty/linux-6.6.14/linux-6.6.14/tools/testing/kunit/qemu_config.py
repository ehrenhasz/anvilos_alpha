from dataclasses import dataclass
from typing import List
@dataclass(frozen=True)
class QemuArchParams:
  linux_arch: str
  kconfig: str
  qemu_arch: str
  kernel_path: str
  kernel_command_line: str
  extra_qemu_params: List[str]
  serial: str = 'stdio'
