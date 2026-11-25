#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check the production AOT member comparator and runtime sorting call."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

PRELUDE = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct { uint32_t offset_in_tu; } aot_tb;
'''
TEST = r'''
static void sort_members(aot_tb **members, int member_num) {
SORT_BODY
}
int main(void) {
    aot_tb blocks[1024];
    aot_tb *members[1024];
    for (unsigned n = 0; n <= 1024; n = n < 2 ? n + 1 : n * 2) {
        for (unsigned order = 0; order < 3; order++) {
            for (unsigned i = 0; i < n; i++) {
                blocks[i].offset_in_tu = i * 4;
                unsigned index = order == 0 ? i : order == 1 ? n - 1 - i :
                                 (i * 257u) % n;
                members[i] = &blocks[index];
            }
            sort_members(members, n);
            for (unsigned i = 0; i < n; i++) {
                assert(members[i] == &blocks[i]);
            }
        }
    }
    /* Unsigned offsets must not overflow a subtraction-based comparator. */
    aot_tb limits[] = {{UINT32_MAX}, {0}, {0x80000000u}, {0x7fffffffu}};
    aot_tb *edges[] = {&limits[0], &limits[1], &limits[2], &limits[3]};
    sort_members(edges, 4);
    assert(edges[0] == &limits[1] && edges[1] == &limits[3]);
    assert(edges[2] == &limits[2] && edges[3] == &limits[0]);
    aot_tb *duplicates[] = {&limits[0], &limits[0]};
    sort_members(duplicates, 2);
    assert(duplicates[0] == &limits[0] && duplicates[1] == &limits[0]);
    puts("PASS: AOT TU member layout sorting and unsigned offset limits");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('repo', type=Path)
    args = parser.parse_args()
    text = (args.repo / 'target/i386/latx/sbt/aot_recover_tb.c').read_text()
    start = text.index('static int aot_tu_member_offset_cmp(')
    comparator = text[start:text.index('\n}', start) + 2]
    start = text.index('        qsort(members, member_num,')
    body = text[start:text.index(';', start) + 1]
    source = PRELUDE + comparator + TEST.replace('SORT_BODY', body)
    with tempfile.TemporaryDirectory(prefix='latx-tu-sort-') as tmp:
        path = Path(tmp)
        unit, binary = path / 'sort.c', path / 'sort'
        unit.write_text(source)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        subprocess.run(cc + ['-std=gnu11', '-O2', str(unit), '-o', str(binary)],
                       check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
