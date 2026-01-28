import argparse
import csv
import os
import sys
class PinGeneratorError(Exception):
    pass
class Pin:
    def __init__(self, cpu_pin_name):
        self._cpu_pin_name = cpu_pin_name
        self._board_pin_names = set()
        self._available = False
        self._hidden = False
        self._generator = None
    def name(self):
        return self._cpu_pin_name
    def add_board_pin_name(self, board_pin_name, hidden=False):
        self._board_pin_names.add(
            (
                board_pin_name,
                hidden,
            )
        )
    def add_af(self, af_idx, af_name, af):
        raise NotImplementedError
    @staticmethod
    def validate_cpu_pin_name(cpu_pin_name):
        if not cpu_pin_name.strip():
            raise PinGeneratorError("Missing cpu pin name")
    @staticmethod
    def validate_board_pin_name(board_pin_name):
        pass
    def index(self):
        raise NotImplementedError
    def index_name(self):
        i = self.index()
        return str(i) if i is not None else None
    def definition(self):
        raise NotImplementedError
    def is_const(self):
        return True
    def enable_macro(self):
        return None
    def print_source(self, out_source):
        pass
class PinGenerator:
    def __init__(self, pin_type, enable_af=False):
        self._pins = []
        self._pin_type = pin_type
        self._enable_af = enable_af
    def add_cpu_pin(self, cpu_pin_name, available=True):
        pin = self._pin_type(cpu_pin_name)
        pin._available = available
        self._pins.append(pin)
        pin._generator = self
        return pin
    def available_pins(self, exclude_hidden=False):
        for pin in self._pins:
            if not pin._available:
                continue
            if exclude_hidden and pin._hidden:
                continue
            yield pin
    def extra_args(self, parser):
        pass
    def parse_board_csv(self, filename):
        with open(filename, "r") as csvfile:
            rows = csv.reader(csvfile)
            for linenum, row in enumerate(rows):
                try:
                    if len(row) == 0 or row[0].startswith("
                        continue
                    if len(row) != 2:
                        raise PinGeneratorError("Expecting two entries in each row")
                    board_pin_name, cpu_pin_name = (x.strip() for x in row)
                    cpu_hidden = False
                    if cpu_pin_name.startswith("-"):
                        cpu_hidden = True
                        cpu_pin_name = cpu_pin_name[1:]
                    self._pin_type.validate_cpu_pin_name(cpu_pin_name)
                    pin = self.find_pin_by_cpu_pin_name(cpu_pin_name, create=True)
                    pin._available = True  
                    pin._hidden = cpu_hidden  
                    if board_pin_name:
                        board_hidden = False
                        if board_pin_name.startswith("-"):
                            board_hidden = True
                            board_pin_name = board_pin_name[1:]
                        self._pin_type.validate_board_pin_name(board_pin_name)
                        pin.add_board_pin_name(board_pin_name, board_hidden)
                except PinGeneratorError as er:
                    raise PinGeneratorError("{}:{}: {}".format(filename, linenum, er))
    def parse_af_csv(self, filename, header_rows=1, pin_col=0, af_col=1):
        headings = {}
        with open(filename, "r") as csvfile:
            rows = csv.reader(csvfile)
            for linenum, row in enumerate(rows):
                try:
                    if len(row) == 0 or row[0].startswith("
                        continue
                    if header_rows:
                        if not headings:
                            for af_idx, header in enumerate(row[af_col:]):
                                headings[af_idx] = header.strip()
                        header_rows -= 1
                        continue
                    if len(row) <= max(pin_col, af_col):
                        raise PinGeneratorError(
                            "Expecting {} entries in each row".format(max(pin_col, af_col))
                        )
                    cpu_pin_name = row[pin_col].strip()
                    if cpu_pin_name == "-":
                        continue
                    self._pin_type.validate_cpu_pin_name(cpu_pin_name)
                    pin = self.find_pin_by_cpu_pin_name(cpu_pin_name, create=True)
                    for af_idx, af in enumerate(row[af_col:]):
                        af = af.strip()
                        if not af:
                            continue
                        pin.add_af(af_idx, headings.get(af_idx, ""), af)
                except PinGeneratorError as er:
                    raise PinGeneratorError("{}:{}: {}".format(filename, linenum, er))
    def find_pin_by_cpu_pin_name(self, cpu_pin_name, create=True):
        for pin in self._pins:
            if pin._cpu_pin_name == cpu_pin_name:
                return pin
        if create:
            return self.add_cpu_pin(cpu_pin_name, available=False)
        else:
            raise PinGeneratorError("Unknown cpu pin {}".format(cpu_pin_name))
    def print_board_locals_dict(self, out_source):
        print(file=out_source)
        print(
            "static const mp_rom_map_elem_t machine_pin_board_pins_locals_dict_table[] = {",
            file=out_source,
        )
        for pin in self.available_pins():
            for board_pin_name, board_hidden in pin._board_pin_names:
                if board_hidden:
                    continue
                print(
                    "    {{ MP_ROM_QSTR(MP_QSTR_{:s}), MP_ROM_PTR(pin_{:s}) }},".format(
                        board_pin_name,
                        pin.name(),
                    ),
                    file=out_source,
                )
        print("};", file=out_source)
        print(
            "MP_DEFINE_CONST_DICT(machine_pin_board_pins_locals_dict, machine_pin_board_pins_locals_dict_table);",
            file=out_source,
        )
    def print_cpu_locals_dict(self, out_source):
        print(file=out_source)
        print(
            "static const mp_rom_map_elem_t machine_pin_cpu_pins_locals_dict_table[] = {",
            file=out_source,
        )
        for pin in self.available_pins(exclude_hidden=True):
            m = pin.enable_macro()
            if m:
                print("    
            print(
                "    {{ MP_ROM_QSTR(MP_QSTR_{:s}), MP_ROM_PTR(pin_{:s}) }},".format(
                    pin.name(),
                    pin.name(),
                ),
                file=out_source,
            )
            if m:
                print("    
        print("};", file=out_source)
        print(
            "MP_DEFINE_CONST_DICT(machine_pin_cpu_pins_locals_dict, machine_pin_cpu_pins_locals_dict_table);",
            file=out_source,
        )
    def _cpu_pin_pointer(self, pin):
        return "&pin_{:s}_obj".format(pin.name())
    def board_name_define_prefix(self):
        return ""
    def print_defines(self, out_header, cpu=True, board=True):
        for pin in self.available_pins():
            print(file=out_header)
            m = pin.enable_macro()
            if m:
                print("
            if cpu:
                print(
                    "
                    file=out_header,
                )
            if board:
                for board_pin_name, _board_hidden in pin._board_pin_names:
                    print(
                        "
                            self.board_name_define_prefix(),
                            board_pin_name,
                            pin.name(),
                        ),
                        file=out_header,
                    )
            if m:
                print("
    def print_pin_objects(self, out_source):
        print(file=out_source)
        for pin in self.available_pins():
            m = pin.enable_macro()
            if m:
                print("
            print(
                "{:s}machine_pin_obj_t pin_{:s}_obj = {:s};".format(
                    "const " if pin.is_const() else "",
                    pin.name(),
                    pin.definition(),
                ),
                file=out_source,
            )
            if m:
                print("
    def print_pin_object_externs(self, out_header):
        print(file=out_header)
        for pin in self.available_pins():
            m = pin.enable_macro()
            if m:
                print("
            print(
                "extern {:s}machine_pin_obj_t pin_{:s}_obj;".format(
                    "const " if pin.is_const() else "",
                    pin.name(),
                ),
                file=out_header,
            )
            if m:
                print("
    def print_source(self, out_source):
        self.print_pin_objects(out_source)
        self.print_cpu_locals_dict(out_source)
        self.print_board_locals_dict(out_source)
    def print_header(self, out_header):
        self.print_pin_object_externs(out_header)
        self.print_defines(out_header)
    def load_inputs(self, out_source):
        if self._enable_af and self.args.af_csv:
            print("// --af-csv {:s}".format(self.args.af_csv), file=out_source)
            self.parse_af_csv(self.args.af_csv)
        if self.args.board_csv:
            print("// --board-csv {:s}".format(self.args.board_csv), file=out_source)
            self.parse_board_csv(self.args.board_csv)
        if self.args.prefix:
            print("// --prefix {:s}".format(self.args.prefix), file=out_source)
            print(file=out_source)
            with open(self.args.prefix, "r") as prefix_file:
                print(prefix_file.read(), end="", file=out_source)
    def generate_extra_files(self):
        pass
    def main(self):
        parser = argparse.ArgumentParser(description="Generate board specific pin file")
        parser.add_argument("--board-csv")
        if self._enable_af:
            parser.add_argument("--af-csv")
        parser.add_argument("--prefix")
        parser.add_argument("--output-source")
        parser.add_argument("--output-header")
        self.extra_args(parser)
        self.args = parser.parse_args()
        try:
            with open(self.args.output_source, "w") as out_source:
                print("// This file was automatically generated by make-pins.py", file=out_source)
                print("//", file=out_source)
                self.load_inputs(out_source)
                for pin in self.available_pins():
                    pin.print_source(out_source)
                self.print_source(out_source)
            with open(self.args.output_header, "w") as out_header:
                self.print_header(out_header)
            self.generate_extra_files()
        except PinGeneratorError as er:
            print(er)
            sys.exit(1)
class NumericPinGenerator(PinGenerator):
    def cpu_table_size(self):
        raise NotImplementedError
    def print_cpu_table(self, out_source):
        print(file=out_source)
        print(
            "const machine_pin_obj_t machine_pin_obj_table[{}] = {{".format(self.cpu_table_size()),
            file=out_source,
        )
        for pin in self.available_pins():
            n = pin.index_name()
            if n is None:
                continue
            m = pin.enable_macro()
            if m:
                print("    
            print(
                "    [{:s}] = {:s},".format(
                    pin.index_name(),
                    pin.definition(),
                ),
                file=out_source,
            )
            if m:
                print("    
        print("};", file=out_source)
        print(file=out_source)
        for pin in self.available_pins():
            n = pin.index_name()
            if n is not None:
                continue
            m = pin.enable_macro()
            if m:
                print("
            print(
                "{:s}machine_pin_obj_t pin_{:s}_obj = {:s};".format(
                    "const " if pin.is_const() else "",
                    pin.name(),
                    pin.definition(),
                ),
                file=out_source,
            )
            if m:
                print("
    def print_source(self, out_source):
        self.print_cpu_table(out_source)
        self.print_board_locals_dict(out_source)
    def print_header(self, out_header):
        self.print_defines(out_header)
    def _cpu_pin_pointer(self, pin):
        n = pin.index_name()
        if n is not None:
            return "&machine_pin_obj_table[{:s}]".format(pin.index_name())
        else:
            return super()._cpu_pin_pointer(pin)
