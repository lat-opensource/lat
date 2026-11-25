#!/bin/bash
# Syntax-check selected sources with low-memory preprocessor definitions.
# Full configure/link/runtime checks require separate low-memory builds.
set -euo pipefail
build=$(realpath "$1")

python3 - "$build" <<'PY'
import json
import shlex
import subprocess
import sys
from pathlib import Path

build = Path(sys.argv[1])
commands = json.loads((build / 'compile_commands.json').read_text())
sources = (
    'accel/tcg/cpu-exec.c',
    'target/i386/latx/translator/translate.c',
    'target/i386/latx/ir2/ir2-relocate.c',
)

for low_mem in (1, 2):
    for source in sources:
        entry = next(item for item in commands
                     if item['file'].endswith(source))
        args = shlex.split(entry['command'])
        args = args[:args.index('-MD')]
        args += [entry['file'], f'-DCONFIG_LATX_LOW_MEM_{low_mem}',
                 '-fsyntax-only']
        subprocess.run(args, cwd=build, check=True)
        print(f'low-memory {low_mem}: {source} PASS')
PY
