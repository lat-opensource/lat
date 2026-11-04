#!/usr/bin/env python3
"""Check that bundled GLib neither adds shared dependencies nor exports APIs."""

import re
import subprocess
import sys

binary = sys.argv[1]
dynamic = subprocess.check_output(['readelf', '-dW', binary], text=True)
for name in ('libglib-2.0.so', 'libpcre2-8.so'):
    if re.search(r'\(NEEDED\).*\[' + re.escape(name), dynamic):
        raise SystemExit(f'{binary}: unexpected dynamic dependency {name}')

exports = subprocess.check_output(['nm', '-D', '--defined-only', binary], text=True)
symbols = {line.split()[-1].split('@')[0] for line in exports.splitlines() if line.split()}
for name in ('g_free', 'g_checksum_new'):
    if name in symbols:
        raise SystemExit(f'{binary}: bundled GLib symbol {name} is exported')

print('PASS: bundled GLib dependencies and symbol visibility')
