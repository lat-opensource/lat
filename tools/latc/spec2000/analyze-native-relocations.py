#!/usr/bin/env python3
"""Summarize LATC native-image TB and relocation structure."""

import argparse
import collections
import glob
import json
import os
import struct
from pathlib import Path


RELOCATION_NAMES = {
    1: "runtime",
    2: "tb_target",
    3: "guest_address",
    4: "jrra_target",
    5: "image_data",
}

SYMBOL_NAMES = {
    0: "invalid",
    1: "epilogue_ret_id_1",
    2: "epilogue_ret_id_0",
    3: "jirl_epilogue_ret_id_1",
    4: "jirl_epilogue_ret_id_0",
    5: "epilogue_ret_0",
    6: "update_mxcsr_status",
    7: "fxsave",
    8: "fxrstor",
    9: "fpregs_x80_to_64",
    10: "fpregs_64_to_x80",
    11: "update_fp_status",
    12: "cpuid",
    13: "raise_illop",
    14: "raise_gpf",
    15: "raise_syscall",
    16: "pftable",
    17: "pcmpistri_xmm",
    18: "pcmpistrm_xmm",
}


def increment(counts, key):
    counts[key] += 1


def parse_image(path):
    path = Path(path)
    data = path.read_bytes()
    if data[:8] != b"LATNAT2\0":
        raise ValueError("%s: invalid LAT native image magic" % path)
    version, header_size = struct.unpack_from("<II", data, 8)
    expected_header = 240 if version >= 7 else 224
    if version not in (2, 3, 4, 5, 6, 7, 8) or header_size != expected_header or len(data) < header_size:
        raise ValueError("%s: unsupported LAT native image header" % path)
    flags = struct.unpack_from("<I", data, 16)[0]
    code_offset, code_size, tb_offset, tb_count, reloc_offset, reloc_count = \
        struct.unpack_from("<QQQQQQ", data, 56)
    pc_map_offset, pc_map_count = struct.unpack_from("<QQ", data, 104)
    tu_offset = tu_count = 0
    if version >= 7:
        tu_offset, tu_count = struct.unpack_from("<QQ", data, 120)
    tb_stride = 56 if version >= 7 else {2: 24, 3: 32, 4: 40, 5: 48, 6: 48}[version]
    reloc_stride = 40 if version >= 7 else 32
    pc_map_end = pc_map_offset + pc_map_count * 32
    if not tu_count and not tu_offset:
        tu_offset = pc_map_end
    if (reloc_offset + reloc_count * reloc_stride != pc_map_offset or
            pc_map_end != tu_offset or
            tu_offset + tu_count * 32 != len(data)):
        raise ValueError("%s: invalid LAT native PC map" % path)

    exact_targets = set()
    pc_counts = collections.Counter()
    tu_heads = 0
    tu_members = 0
    for index in range(tb_count):
        guest_pc, _code, size, tb_flags = struct.unpack_from(
            "<QQII", data, tb_offset + index * tb_stride)
        exact_targets.add((guest_pc, tb_flags))
        pc_counts[guest_pc] += 1
        if size:
            tu_heads += 1
        else:
            tu_members += 1

    counts = collections.Counter()
    for index in range(reloc_count):
        offset = reloc_offset + index * reloc_stride
        code, addend, kind, target, slots, reserved = struct.unpack_from(
            "<QqIIII", data, offset)
        kind_name = RELOCATION_NAMES.get(kind, "kind_%d" % kind)
        increment(counts, "kind.%s" % kind_name)
        increment(counts, "kind_slots.%s.%d" % (kind_name, slots))
        if kind == 1:
            symbol = SYMBOL_NAMES.get(target, "symbol_%d" % target)
            increment(counts, "runtime.%s" % symbol)
        elif kind in (2, 4):
            source = SYMBOL_NAMES.get(reserved, "symbol_%d" % reserved)
            increment(counts, "target_source.%s.%s" % (kind_name, source))
            if (addend, target) in exact_targets:
                resolution = "exact"
            elif pc_counts[addend] == 1:
                resolution = "unique_pc"
            else:
                resolution = "missing"
            increment(counts, "target_resolution.%s.%s" %
                      (kind_name, resolution))
            if kind == 2 and slots == 2:
                insn0, insn1 = struct.unpack_from(
                    "<II", data, code_offset + code)
                increment(counts, "tb_pair_destination.r%d" % (insn1 & 0x1f))
                expected = ((insn0 & 0xfe00001f) == 0x1e00000c and
                            (insn1 & 0xfc0003e0) == 0x4c000180)
                increment(counts, "tb_pair_encoding.%s" %
                          ("pcaddu18i_jirl" if expected else "other"))

    return {
        "image": os.fspath(path),
        "flags": flags,
        "code_size": code_size,
        "tb_count": tb_count,
        "tu_heads": tu_heads,
        "tu_internal_members": tu_members,
        "tu_internal_percent": tu_members * 100.0 / tb_count if tb_count else 0,
        "relocation_count": reloc_count,
        "pc_map_count": pc_map_count,
        "counts": dict(sorted(counts.items())),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("images", nargs="+", help="native images or glob patterns")
    parser.add_argument("--output", type=argparse.FileType("w"), default="-")
    args = parser.parse_args()
    paths = []
    for pattern in args.images:
        paths.extend(glob.glob(pattern))
    if not paths:
        parser.error("no native images matched")
    result = [parse_image(path) for path in sorted({os.path.abspath(p)
                                                    for p in paths})]
    json.dump(result, args.output, indent=2)
    args.output.write("\n")


if __name__ == "__main__":
    main()
