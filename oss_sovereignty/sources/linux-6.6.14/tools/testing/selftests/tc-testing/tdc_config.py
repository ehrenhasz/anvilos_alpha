"""
tdc_config.py - tdc user-specified values
Copyright (C) 2017 Lucas Bates <lucasb@mojatatu.com>
"""
NAMES = {
          'TC': '/sbin/tc',
          'IP': '/sbin/ip',
          'DEV0': 'v0p0',
          'DEV1': 'v0p1',
          'DEV2': '',
          'DUMMY': 'dummy1',
	  'ETH': 'eth0',
          'BATCH_FILE': './batch.txt',
          'BATCH_DIR': 'tmp',
          'TIMEOUT': 24,
          'NS': 'tcut',
          'EBPFDIR': './'
        }
ENVIR = { }
try:
    from tdc_config_local import *
except ImportError as ie:
    pass
try:
    NAMES.update(EXTRA_NAMES)
except NameError as ne:
    pass
