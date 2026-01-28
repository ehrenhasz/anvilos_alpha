from collections import defaultdict, namedtuple
import os
import re
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../tools"))
import boardgen
AFS = {
    "SAMD21": ["eic", "adc0", "sercom1", "sercom2", "tcc1", "tcc2"],
    "SAMD51": ["eic", "adc0", "adc1", "sercom1", "sercom2", "tc", "tcc1", "tcc2"],
}
class SamdPin(boardgen.Pin):
    def __init__(self, cpu_pin_name):
        super().__init__(cpu_pin_name)
        self._port = cpu_pin_name[1]
        self._pin = int(cpu_pin_name[2:])
        self._afs = defaultdict(lambda: 0xFF)
    def add_af(self, af_idx, af_name, af):
        self._available = True
        name = AFS[self._generator.args.mcu][af_idx]
        assert name == af_name.lower()
        if name == "eic" or name.startswith("adc"):
            v = int(af)
        else:
            v = int(af, 16)
        self._afs[AFS[self._generator.args.mcu][af_idx]] = v
    def definition(self):
        return "PIN({:s}, {})".format(
            self.name(),
            ", ".join("0x{:02x}".format(self._afs[x]) for x in AFS[self._generator.args.mcu]),
        )
    def enable_macro(self):
        return "defined(PIN_{})".format(self.name())
    @staticmethod
    def validate_cpu_pin_name(cpu_pin_name):
        boardgen.Pin.validate_cpu_pin_name(cpu_pin_name)
        if not re.match("P[A-D][0-9][0-9]$", cpu_pin_name):
            raise boardgen.PinGeneratorError("Invalid cpu pin name '{}'".format(cpu_pin_name))
class SamdPinGenerator(boardgen.PinGenerator):
    def __init__(self):
        super().__init__(
            pin_type=SamdPin,
            enable_af=True,
        )
    def parse_af_csv(self, filename):
        return super().parse_af_csv(filename, header_rows=1, pin_col=0, af_col=1)
    def extra_args(self, parser):
        parser.add_argument("--mcu")
if __name__ == "__main__":
    SamdPinGenerator().main()
