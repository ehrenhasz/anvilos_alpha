from collections import defaultdict, namedtuple
import os
import re
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../tools"))
import boardgen
SUPPORTED_AF = {
    "TIM": ["CH1", "CH2", "CH3", "CH4", "CH1N", "CH2N", "CH3N", "CH1_ETR", "ETR", "BKIN"],
    "I2C": ["SDA", "SCL"],
    "I2S": ["CK", "MCK", "SD", "WS", "EXTSD"],
    "USART": ["RX", "TX", "CTS", "RTS", "CK"],
    "UART": ["RX", "TX", "CTS", "RTS"],
    "LPUART": ["RX", "TX", "CTS", "RTS"],
    "SPI": ["NSS", "SCK", "MISO", "MOSI"],
    "SDMMC": ["CK", "CMD", "D0", "D1", "D2", "D3"],
    "CAN": ["TX", "RX"],
}
CONDITIONAL_VAR = {
    "I2C": "MICROPY_HW_I2C{num}_SCL",
    "I2S": "MICROPY_HW_I2S{num}",
    "SPI": "MICROPY_HW_SPI{num}_SCK",
    "UART": "MICROPY_HW_UART{num}_TX",
    "LPUART": "MICROPY_HW_LPUART{num}_TX",
    "USART": "MICROPY_HW_UART{num}_TX",
    "SDMMC": "MICROPY_HW_SDMMC{num}_CK",
    "CAN": "MICROPY_HW_CAN{num}_TX",
}
PinAf = namedtuple(
    "PinAf",
    [
        "af_idx",  # int, 0-15
        "af_fn",  # e.g. "I2C"
        "af_unit",  # int, e.g. 1 (for I2C1) or None (for OTG_HS_ULPI_CK)
        "af_pin",  # e.g. "SDA"
        "af_supported",  # bool, see table above
        "af_name",  # e.g. "I2C1_SDA"
    ],
)
MIN_ADC_UNIT = 1
MAX_ADC_UNIT = 3
class Stm32Pin(boardgen.Pin):
    def __init__(self, cpu_pin_name):
        super().__init__(cpu_pin_name)
        self._analog_only = cpu_pin_name.endswith("_C")
        if self._analog_only:
            cpu_pin_name = cpu_pin_name[:-2]
        self._port = cpu_pin_name[1]
        self._pin = int(cpu_pin_name[2:])
        self._afs = []
        self._adc_channel = 0
        self._adc_units = []
    def add_af(self, af_idx, af_name, af):
        if af_idx > 16:
            return
        if af_idx == 16:
            if af_name != "ADC":
                raise boardgen.PinGeneratorError(
                    "Invalid AF column name '{:s}' for ADC column with index {:d}.".format(
                        af_name, af_idx
                    )
                )
            return self.add_adc(af)
        if af_name != "AF{:d}".format(af_idx):
            raise boardgen.PinGeneratorError(
                "Invalid AF column name '{:s}' for AF index {:d}.".format(af_name, af_idx)
            )
        for af_name in af.split("/"):
            if not af_name.strip():
                continue
            m = re.match("([A-Z0-9]+[A-Z])(([0-9]+)(ext)?)?(_(.*))?", af_name)
            if not m:
                raise boardgen.PinGeneratorError(
                    "Invalid af '{:s}' for pin '{:s}'".format(af_name, self.name())
                )
            else:
                af_fn = m.group(1)
                af_unit = int(m.group(3)) if m.group(3) is not None else None
                af_ext = m.group(4) == "ext"
                af_pin = m.group(6)
                if af_ext:
                    af_pin = "EXT" + af_pin
                af_supported = af_fn in SUPPORTED_AF and af_pin in SUPPORTED_AF[af_fn]
                self._afs.append(PinAf(af_idx, af_fn, af_unit, af_pin, af_supported, af_name))
    def add_adc(self, adc):
        if not adc.strip():
            return
        for adc_name in adc.split("/"):
            m = re.match("ADC([1-5]+)_(IN[NP]?)([0-9]+)$", adc_name)
            if not m:
                raise boardgen.PinGeneratorError(
                    "Invalid adc '{:s}' for pin '{:s}'".format(adc_name, self.name())
                )
            adc_units = [int(x) for x in m.group(1) if MIN_ADC_UNIT <= int(x) <= MAX_ADC_UNIT]
            adc_mode = m.group(2)
            if adc_mode == "INN":
                continue
            adc_channel = int(m.group(3))
            if len(adc_units) > len(self._adc_units):
                self._adc_units = adc_units
                self._adc_channel = adc_channel
    def name(self):
        return self._cpu_pin_name[1:]
    def definition(self):
        adc_units_bitfield = (
            " | ".join("PIN_ADC{}".format(unit) for unit in self._adc_units) or "0"
        )
        pin_macro = "PIN_ANALOG" if self._analog_only else "PIN"
        return "{:s}({:s}, {:d}, pin_{:s}_af, {:s}, {:d})".format(
            pin_macro, self._port, self._pin, self.name(), adc_units_bitfield, self._adc_channel
        )
    def print_source(self, out_source):
        print(file=out_source)
        print("const pin_af_obj_t pin_{:s}_af[] = {{".format(self.name()), file=out_source)
        for af in self._afs:
            if af.af_fn in CONDITIONAL_VAR:
                print(
                    "    #if defined({:s})".format(
                        CONDITIONAL_VAR[af.af_fn].format(num=af.af_unit)
                    ),
                    file=out_source,
                )
            if af.af_supported:
                print("    ", end="", file=out_source)
            else:
                print("    // ", end="", file=out_source)
            print(
                "AF({:d}, {:s}, {:d}, {:s}, {:s}{:s}),  // {:s}".format(
                    af.af_idx,
                    af.af_fn,
                    af.af_unit or 0,
                    af.af_pin or "NONE",
                    af.af_fn,
                    "" if af.af_unit is None else str(af.af_unit),
                    af.af_name,
                ),
                file=out_source,
            )
            if af.af_fn in CONDITIONAL_VAR:
                print("    #endif", file=out_source)
        print("};", file=out_source)
    @staticmethod
    def validate_cpu_pin_name(cpu_pin_name):
        boardgen.Pin.validate_cpu_pin_name(cpu_pin_name)
        if not re.match("P[A-K][0-9]+(_C)?$", cpu_pin_name):
            raise boardgen.PinGeneratorError("Invalid cpu pin name '{}'".format(cpu_pin_name))
class Stm32PinGenerator(boardgen.PinGenerator):
    def __init__(self):
        super().__init__(
            pin_type=Stm32Pin,
            enable_af=True,
        )
    def board_name_define_prefix(self):
        return "pyb_"
    def parse_af_csv(self, filename):
        return super().parse_af_csv(filename, header_rows=2, pin_col=1, af_col=2)
    def count_adc_pins(self):
        adc_units = defaultdict(lambda: (0, 0))
        for pin in self._pins:  # All pins
            for unit in pin._adc_units:
                num, max_channel = adc_units[unit]
                if pin._available:
                    adc_units[unit] = num + 1, max(max_channel, pin._adc_channel)
        return adc_units.items()
    def print_adcs(self, out_source):
        for adc_unit, (num_pins, max_channel) in self.count_adc_pins():
            print(file=out_source)
            print(
                "const machine_pin_obj_t * const pin_adc{:d}[{:d}] = {{".format(
                    adc_unit, max_channel + 1
                ),
                file=out_source,
            )
            for pin in self.available_pins():
                if pin._hidden:
                    continue
                if adc_unit in pin._adc_units:
                    print(
                        "    [{:d}] = {:s},".format(pin._adc_channel, self._cpu_pin_pointer(pin)),
                        file=out_source,
                    )
            print("};", file=out_source)
    def print_adc_externs(self, out_source):
        print(file=out_source)
        for adc_unit, (num_pins, max_channel) in self.count_adc_pins():
            print(
                "extern const machine_pin_obj_t * const pin_adc{:d}[{:d}];".format(
                    adc_unit, max_channel + 1
                ),
                file=out_source,
            )
    def print_source(self, out_source):
        super().print_source(out_source)
        self.print_adcs(out_source)
    def print_header(self, out_header):
        if self.args.mboot_mode:
            self.print_defines(out_header, cpu=False)
        else:
            super().print_header(out_header)
            self.print_adc_externs(out_header)
    def print_af_const(self, out_af_const):
        names = set()
        for pin in self.available_pins():
            for af in pin._afs:
                if not af.af_supported:
                    continue
                key = (
                    "AF{:d}_{:s}{:d}".format(af.af_idx, af.af_fn, af.af_unit),
                    af.af_fn,
                    af.af_unit,
                )
                names.add(key)
        for key in sorted(names):
            name, af_fn, af_unit = key
            if af_fn in CONDITIONAL_VAR:
                print(
                    "    #if defined({:s})".format(CONDITIONAL_VAR[af_fn].format(num=af_unit)),
                    file=out_af_const,
                )
            print(
                "    {{ MP_ROM_QSTR(MP_QSTR_{:s}), MP_ROM_INT(GPIO_{:s}) }},".format(name, name),
                file=out_af_const,
            )
            if af_fn in CONDITIONAL_VAR:
                print("    #endif", file=out_af_const)
    def print_af_defs(self, out_af_defs):
        af_defs = defaultdict(list)
        for pin in self._pins:
            for af in pin._afs:
                key = af.af_fn, af.af_unit, af.af_pin
                af_defs[key].append((pin, af.af_idx))
        for key, pins in af_defs.items():
            af_fn, af_unit, af_pin = key
            print(file=out_af_defs)
            print(
                "#define STATIC_AF_{:s}{:s}_{:s}(pin_obj) ( \\".format(
                    af_fn, "" if af_unit is None else str(af_unit), af_pin or "NULL"
                ),
                file=out_af_defs,
            )
            for pin, af_idx in pins:
                if self.args.mboot_mode:
                    print(
                        "    ((pin_obj) == (pin_{:s})) ? ({:d}) : \\".format(pin.name(), af_idx),
                        file=out_af_defs,
                    )
                else:
                    print(
                        '    ((strcmp( #pin_obj , "(&pin_{:s}_obj)") & strcmp( #pin_obj , "((&pin_{:s}_obj))")) == 0) ? ({:d}) : \\'.format(
                            pin.name(), pin.name(), af_idx
                        ),
                        file=out_af_defs,
                    )
            print("    (0xffffffffffffffffULL))", file=out_af_defs)
    def extra_args(self, parser):
        parser.add_argument("--output-af-const")
        parser.add_argument("--output-af-defs")
        parser.add_argument("--mboot-mode", action="store_true")
    def generate_extra_files(self):
        if self.args.output_af_const:
            with open(self.args.output_af_const, "w") as out_af_const:
                self.print_af_const(out_af_const)
        if self.args.output_af_defs:
            with open(self.args.output_af_defs, "w") as out_af_defs:
                self.print_af_defs(out_af_defs)
if __name__ == "__main__":
    Stm32PinGenerator().main()
