import sys

# MicroJSON helper (RFC-0002)
def log_event(event_id, data):
    try:
        import json
        print(json.dumps({"@ID": event_id, "data": data}))
    except ImportError:
        # Fallback manual construction
        print('{"@ID": ' + str(event_id) + ', "data": "' + str(data) + '"}')

class CharDevMem:
    """
    Recoded implementation of Linux drivers/char/mem.c (null/zero/full subset).
    """
    MODE_NULL = 0
    MODE_ZERO = 1
    MODE_FULL = 2

    def __init__(self, mode=MODE_NULL):
        self.mode = mode

    def read(self, count):
        if count < 0:
            return b''
        
        if self.mode == self.MODE_NULL:
            return b'' # EOF immediately
        elif self.mode == self.MODE_ZERO:
            return b'\x00' * count
        elif self.mode == self.MODE_FULL:
            return b'\x00' * count
        return b''

    def write(self, data):
        if self.mode == self.MODE_NULL:
            return len(data) # Accept and discard
        elif self.mode == self.MODE_ZERO:
            return len(data) # Accept and discard
        elif self.mode == self.MODE_FULL:
            # ENOSPC = 28
            raise OSError(28) 
        return 0

def test():
    # Test /dev/null behavior
    dev_null = CharDevMem(CharDevMem.MODE_NULL)
    if dev_null.read(10) != b'':
        log_event(9001, "FAIL: null read not empty")
        sys.exit(1)
    if dev_null.write(b'hello') != 5:
        log_event(9001, "FAIL: null write length mismatch")
        sys.exit(1)
        
    # Test /dev/zero behavior
    dev_zero = CharDevMem(CharDevMem.MODE_ZERO)
    zeros = dev_zero.read(5)
    if len(zeros) != 5 or zeros != b'\x00\x00\x00\x00\x00':
        log_event(9001, "FAIL: zero read mismatch")
        sys.exit(1)
    if dev_zero.write(b'data') != 4:
        log_event(9001, "FAIL: zero write length mismatch")
        sys.exit(1)

    # Test /dev/full behavior
    dev_full = CharDevMem(CharDevMem.MODE_FULL)
    try:
        dev_full.write(b'test')
        log_event(9001, "FAIL: full did not raise ENOSPC")
        sys.exit(1)
    except OSError as e:
        # In some micropython ports, errno is args[0]
        errno = e.errno if hasattr(e, 'errno') else (e.args[0] if e.args else 0)
        if errno != 28:
            log_event(9001, "FAIL: full raised wrong error " + str(errno))
            sys.exit(1)
    except Exception as e:
        log_event(9001, "FAIL: full raised wrong exception type " + str(type(e)))
        sys.exit(1)

    log_event(1001, "SUCCESS: CharDevMem verification passed")

if __name__ == '__main__':
    test()