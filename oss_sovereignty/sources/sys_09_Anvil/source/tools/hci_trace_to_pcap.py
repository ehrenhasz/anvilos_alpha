import re
import sys
import struct
_LINKTYPE_BLUETOOTH_HCI_H4_WITH_PHDR = 201  # "!I" direction, followed by data
sys.stdout.buffer.write(
    struct.pack("!IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, _LINKTYPE_BLUETOOTH_HCI_H4_WITH_PHDR)
)
_DIR_CONTROLLER_TO_HOST = 1
_DIR_HOST_TO_CONTROLLER = 0
reassemble_timestamp = 0
reassemble_packet = bytearray()
with open(sys.argv[1], "r") as f:
    for line in f:
        line = line.strip()
        m = re.match("([<>]) \\[ *([0-9]+)\\] ([A-Fa-f0-9:]+)", line)
        if not m:
            continue
        timestamp = int(m.group(2))
        data = bytes.fromhex(m.group(3).replace(":", ""))
        if m.group(1) == "<":
            sys.stdout.buffer.write(
                struct.pack(
                    "!IIIII",
                    timestamp // 1000,
                    timestamp % 1000 * 1000,
                    len(data) + 4,
                    len(data) + 4,
                    _DIR_HOST_TO_CONTROLLER,
                )
            )
            sys.stdout.buffer.write(data)
        if m.group(1) == ">":
            if not reassemble_packet:
                reassemble_timestamp = timestamp
            reassemble_packet.extend(data)
            if len(reassemble_packet) > 4:
                plen = 0
                if reassemble_packet[0] == 1:
                    plen = 3 + reassemble_packet[3]
                elif reassemble_packet[0] == 2:
                    plen = 5 + reassemble_packet[3] + (reassemble_packet[4] << 8)
                elif reassemble_packet[0] == 4:
                    plen = 3 + reassemble_packet[2]
                if len(reassemble_packet) >= plen:
                    data = reassemble_packet[0:plen]
                    reassemble_packet = reassemble_packet[plen:]
                    reassemble_timestamp = timestamp
                    sys.stdout.buffer.write(
                        struct.pack(
                            "!IIIII",
                            reassemble_timestamp // 1000,
                            reassemble_timestamp % 1000 * 1000,
                            len(data) + 4,
                            len(data) + 4,
                            _DIR_CONTROLLER_TO_HOST,
                        )
                    )
                    sys.stdout.buffer.write(data)
if reassemble_packet:
    print(
        "Error: Unknown byte in HCI stream. Remainder:",
        reassemble_packet.hex(":"),
        file=sys.stderr,
    )
