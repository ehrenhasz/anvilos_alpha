from _espnow import *
from select import poll, POLLIN
class ESPNow(ESPNowBase):
    _data = [bytearray(ADDR_LEN), bytearray(MAX_DATA_LEN)]
    _none_tuple = (None, None)
    def __init__(self):
        super().__init__()
        self._poll = poll()  
        self._poll.register(self, POLLIN)
    def irecv(self, timeout_ms=None):
        n = self.recvinto(self._data, timeout_ms)
        return self._data if n else self._none_tuple
    def recv(self, timeout_ms=None):
        n = self.recvinto(self._data, timeout_ms)
        return [bytes(x) for x in self._data] if n else self._none_tuple
    def __iter__(self):
        return self
    def __next__(self):
        return self.irecv()  
    def any(self):  
        try:
            next(self._poll.ipoll(0))
            return True
        except StopIteration:
            return False
