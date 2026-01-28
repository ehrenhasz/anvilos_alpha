import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../tools"))
import boardgen
NUM_GPIOS = 49
class Esp32Pin(boardgen.Pin):
    def index(self):
        return int(self._cpu_pin_name[4:])
    def index_name(self):
        return "GPIO_NUM_{:d}".format(self.index())
    def definition(self):
        return "{ .base = { .type = &machine_pin_type }, .irq = { .base = { .type = &machine_pin_irq_type } } }"
    def enable_macro(self):
        return "MICROPY_HW_ENABLE_{}".format(self._cpu_pin_name)
    @staticmethod
    def validate_cpu_pin_name(cpu_pin_name):
        boardgen.Pin.validate_cpu_pin_name(cpu_pin_name)
        if not cpu_pin_name.startswith("GPIO") or not cpu_pin_name[4:].isnumeric():
            raise boardgen.PinGeneratorError(
                "Invalid cpu pin name '{}', must be 'GPIOn'".format(cpu_pin_name)
            )
        if not (0 <= int(cpu_pin_name[4:]) < NUM_GPIOS):
            raise boardgen.PinGeneratorError("Unknown cpu pin '{}'".format(cpu_pin_name))
class Esp32PinGenerator(boardgen.NumericPinGenerator):
    def __init__(self):
        super().__init__(pin_type=Esp32Pin)
        for i in range(NUM_GPIOS):
            self.add_cpu_pin("GPIO{}".format(i))
    def find_pin_by_cpu_pin_name(self, cpu_pin_name, create=True):
        return super().find_pin_by_cpu_pin_name(cpu_pin_name, create=False)
    def cpu_table_size(self):
        return "GPIO_NUM_MAX"
if __name__ == "__main__":
    Esp32PinGenerator().main()
