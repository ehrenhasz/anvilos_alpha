import os
import sys
from TdcPlugin import TdcPlugin
from tdc_config import *
class SubPlugin(TdcPlugin):
    def __init__(self):
        self.sub_class = 'root/SubPlugin'
        super().__init__()
    def pre_suite(self, testcount, testidlist):
        super().pre_suite(testcount, testidlist)
        if os.geteuid():
            print('This script must be run with root privileges', file=sys.stderr)
            exit(1)
