from __future__ import print_function
import struct
EVTYPE_GENERIC  = 0
EVTYPE_PEBS     = 1     
EVTYPE_PEBS_LL  = 2     
EVTYPE_IBS      = 3
def create_event(name, comm, dso, symbol, raw_buf):
        if (len(raw_buf) == 144):
                event = PebsEvent(name, comm, dso, symbol, raw_buf)
        elif (len(raw_buf) == 176):
                event = PebsNHM(name, comm, dso, symbol, raw_buf)
        else:
                event = PerfEvent(name, comm, dso, symbol, raw_buf)
        return event
class PerfEvent(object):
        event_num = 0
        def __init__(self, name, comm, dso, symbol, raw_buf, ev_type=EVTYPE_GENERIC):
                self.name       = name
                self.comm       = comm
                self.dso        = dso
                self.symbol     = symbol
                self.raw_buf    = raw_buf
                self.ev_type    = ev_type
                PerfEvent.event_num += 1
        def show(self):
                print("PMU event: name=%12s, symbol=%24s, comm=%8s, dso=%12s" %
                      (self.name, self.symbol, self.comm, self.dso))
class PebsEvent(PerfEvent):
        pebs_num = 0
        def __init__(self, name, comm, dso, symbol, raw_buf, ev_type=EVTYPE_PEBS):
                tmp_buf=raw_buf[0:80]
                flags, ip, ax, bx, cx, dx, si, di, bp, sp = struct.unpack('QQQQQQQQQQ', tmp_buf)
                self.flags = flags
                self.ip    = ip
                self.ax    = ax
                self.bx    = bx
                self.cx    = cx
                self.dx    = dx
                self.si    = si
                self.di    = di
                self.bp    = bp
                self.sp    = sp
                PerfEvent.__init__(self, name, comm, dso, symbol, raw_buf, ev_type)
                PebsEvent.pebs_num += 1
                del tmp_buf
class PebsNHM(PebsEvent):
        pebs_nhm_num = 0
        def __init__(self, name, comm, dso, symbol, raw_buf, ev_type=EVTYPE_PEBS_LL):
                tmp_buf=raw_buf[144:176]
                status, dla, dse, lat = struct.unpack('QQQQ', tmp_buf)
                self.status = status
                self.dla = dla
                self.dse = dse
                self.lat = lat
                PebsEvent.__init__(self, name, comm, dso, symbol, raw_buf, ev_type)
                PebsNHM.pebs_nhm_num += 1
                del tmp_buf
