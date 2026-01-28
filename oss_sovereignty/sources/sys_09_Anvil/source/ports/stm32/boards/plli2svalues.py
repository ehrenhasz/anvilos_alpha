"""
This program computes I2S PLL parameters for STM32
processors supporting an I2S PLL in the clock tree.
Those processors are listed below in the mcu_support_plli2s[] list.
"""
import re
from collections import namedtuple
class MCU:
    def __init__(self, range_plli2sn, range_plli2sr):
        self.range_plli2sn = range_plli2sn
        self.range_plli2sr = range_plli2sr
mcu_default = MCU(range_plli2sn=range(50, 432 + 1), range_plli2sr=range(2, 7 + 1))
mcu_table = {"stm32f401xe": MCU(range_plli2sn=range(192, 432 + 1), range_plli2sr=range(2, 7 + 1))}
mcu_support_plli2s = [
    "stm32f405xx",
    "stm32f401xe",
    "stm32f407xx",
    "stm32f411xe",
    "stm32f412zx",
    "stm32f413xx",
    "stm32f427xx",
    "stm32f429xx",
    "stm32f439xx",
    "stm32f446xx",
    "stm32f722xx",
    "stm32f733xx",
    "stm32f746xx",
    "stm32f756xx",
    "stm32f767xx",
    "stm32f769xx",
]
def compute_plli2s_table(hse, pllm):
    plli2s = namedtuple("plli2s", "bits rate plli2sn plli2sr i2sdiv odd error")
    plli2s_table = []
    for bits in (16, 32):
        for rate in (8_000, 11_025, 12_000, 16_000, 22_050, 24_000, 32_000, 44_100, 48_000):
            plli2s_candidates = []
            for plli2sn in mcu.range_plli2sn:
                for plli2sr in mcu.range_plli2sr:
                    I2SxCLK = hse // pllm * plli2sn // plli2sr
                    if I2SxCLK < 192_000_000:
                        tmp = (((I2SxCLK // (bits * 2)) * 10) // rate) + 5
                        tmp = tmp // 10
                        odd = tmp & 1
                        i2sdiv = (tmp - odd) // 2
                        Fs = I2SxCLK / ((bits * 2) * ((2 * i2sdiv) + odd))
                        error = (abs(Fs - rate) / rate) * 100
                        plli2s_candidates.append(
                            plli2s(
                                bits=bits,
                                rate=rate,
                                plli2sn=plli2sn,
                                plli2sr=plli2sr,
                                i2sdiv=i2sdiv,
                                odd=odd,
                                error=error,
                            )
                        )
            plli2s_candidates_sorted = sorted(plli2s_candidates, key=lambda x: x.error)
            plli2s_table.append(plli2s_candidates_sorted[0])
    return plli2s_table
def generate_c_table(plli2s_table, hse, pllm):
    print("// MAKE generated file, created by plli2svalues.py: DO NOT EDIT")
    print("// This table is used in machine_i2s.c")
    print(f"// HSE_VALUE = {hse}")
    print(f"// MICROPY_HW_CLK_PLLM = {pllm} \n")
    print("#define PLLI2S_TABLE \\")
    print("{ \\")
    for plli2s in plli2s_table:
        print(
            f"    {{{plli2s.rate}, "
            f"{plli2s.bits}, "
            f"{plli2s.plli2sr}, "
            f"{plli2s.plli2sn} }}, "
            f"/* i2sdiv: {int(plli2s.i2sdiv)}, "
            f"odd: {plli2s.odd}, "
            f"rate error % (desired vs actual)%: {plli2s.error:.4f} */ \\"
        )
    print("}")
def search_header(filename, re_include, re_define, lookup, val):
    regex_include = re.compile(re_include)
    regex_define = re.compile(re_define)
    with open(filename) as f:
        for line in f:
            line = line.strip()
            m = regex_include.match(line)
            if m:
                search_header(m.group(1), re_include, re_define, lookup, val)
                continue
            m = regex_define.match(line)
            if m:
                if m.group(1) == lookup:
                    val[0] = int(m.group(3))
    return val
def main():
    global mcu
    import sys
    argv = sys.argv[1:]
    c_table = False
    mcu_series = "stm32f4"
    hse = None
    pllm = None
    while True:
        if argv[0] == "-c":
            c_table = True
            argv.pop(0)
        elif argv[0] == "-m":
            argv.pop(0)
            mcu_series = argv.pop(0).lower()
        else:
            break
    if mcu_series in mcu_support_plli2s:
        if len(argv) != 2:
            print("usage: pllvalues.py [-c] [-m <mcu_series>] <hse in MHz> <pllm in MHz>")
            sys.exit(1)
        if argv[0].startswith("hse:"):
            (hse,) = search_header(
                argv[0][len("hse:") :],
                r'#include "(boards/[A-Za-z0-9_./]+)"',
                r"#define +(HSE_VALUE) +\((\(uint32_t\))?([0-9]+)\)",
                "HSE_VALUE",
                [None],
            )
            if hse is None:
                raise ValueError("%s does not contain a definition of HSE_VALUE" % argv[0])
            argv.pop(0)
        if argv[0].startswith("pllm:"):
            (pllm,) = search_header(
                argv[0][len("pllm:") :],
                r'#include "(boards/[A-Za-z0-9_./]+)"',
                r"#define +(MICROPY_HW_CLK_PLLM) +\((\(uint32_t\))?([0-9]+)\)",
                "MICROPY_HW_CLK_PLLM",
                [None],
            )
            if pllm is None:
                raise ValueError(
                    "%s does not contain a definition of MICROPY_HW_CLK_PLLM" % argv[0]
                )
            argv.pop(0)
        mcu = mcu_default
        for m in mcu_table:
            if mcu_series.startswith(m):
                mcu = mcu_table[m]
                break
        plli2s_table = compute_plli2s_table(hse, pllm)
        if c_table:
            generate_c_table(plli2s_table, hse, pllm)
if __name__ == "__main__":
    main()
