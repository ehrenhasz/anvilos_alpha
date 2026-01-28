from __future__ import print_function
import argparse
import sys
from . import run, CrossCompileError
try:
    print(run(sys.argv[1:]))
except CrossCompileError as er:
    print(er.args[0], file=sys.stderr)
    raise SystemExit(1)
