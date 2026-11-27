#!/usr/bin/env python3

import struct
import sys


ITERATIONS = 4096
RECORD_SIZE = 24
ALL_FLAGS = 0x8D5
NO_AF = 0x8C5
ALL_SETCC = (1 << 64) - 1
OF_CF_SETCC = (0xFF << 0) | (0xFF << 8)


def records(path):
    data = open(path, "rb").read()
    if len(data) % RECORD_SIZE:
        raise ValueError(f"{path}: incomplete record stream ({len(data)} bytes)")
    return [struct.unpack_from("<QQQ", data, offset)
            for offset in range(0, len(data), RECORD_SIZE)]


def compare_family(reference, candidate, cursor, name, operations, masks):
    failures = []
    for iteration in range(ITERATIONS):
        for op_index, operation in enumerate(operations):
            ref_setcc, ref_flags, ref_result = reference[cursor]
            lat_setcc, lat_flags, lat_result = candidate[cursor]
            flag_mask, setcc_mask = masks(iteration, op_index)
            flag_diff = (ref_flags ^ lat_flags) & flag_mask
            setcc_diff = (ref_setcc ^ lat_setcc) & setcc_mask
            if ref_result != lat_result or flag_diff or setcc_diff:
                failures.append(
                    (iteration, operation, ref_result, lat_result,
                     flag_diff, setcc_diff))
                if len(failures) == 8:
                    return cursor + 1, failures
            cursor += 1
    return cursor, failures


def shift_masks(iteration, op_index):
    widths = (64, 64, 64, 32, 16, 8)
    width = widths[op_index]
    count = (ITERATIONS - iteration) & (0x3F if width == 64 else 0x1F)
    if count == 0:
        return ALL_FLAGS, ALL_SETCC

    flag_mask = NO_AF
    setcc_mask = ALL_SETCC
    if count != 1:
        flag_mask &= ~0x800
        setcc_mask &= ~((0xFF << 0) | (0xFF << 48) | (0xFF << 56))
    if count > width:
        flag_mask &= ~1
        setcc_mask &= ~(0xFF << 8)
    return flag_mask, setcc_mask


def rotate_masks(iteration, op_index):
    widths = (64, 64, 32, 16)
    width = widths[op_index]
    raw_count = ((ITERATIONS - iteration) &
                 (0x3F if width == 64 else 0x1F))
    count = raw_count if width in (32, 64) else raw_count % width
    flag_mask = ALL_FLAGS
    setcc_mask = ALL_SETCC
    if count != 1:
        flag_mask &= ~0x800
        setcc_mask &= ~((0xFF << 0) | (0xFF << 48) | (0xFF << 56))
    return flag_mask, setcc_mask


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} REFERENCE LATX")

    reference = records(sys.argv[1])
    candidate = records(sys.argv[2])
    if len(reference) != len(candidate):
        raise SystemExit(
            f"record count differs: reference={len(reference)} "
            f"latx={len(candidate)}")

    all_masks = lambda iteration, op_index: (ALL_FLAGS, ALL_SETCC)
    no_af_positions = lambda positions: (
        lambda iteration, op_index:
        (NO_AF, ALL_SETCC) if op_index in positions else
        (ALL_FLAGS, ALL_SETCC))
    families = [
        ("alu64",
         ("add", "sub", "cmp", "test", "adc-c0", "adc-c1",
          "sbb-c0", "sbb-c1", "inc", "dec", "and", "or", "xor",
          "neg"),
         no_af_positions((3, 10, 11, 12))),
        ("alu32", ("add", "sub", "cmp", "adc", "sbb", "and", "or",
                   "xor", "neg", "inc", "dec"),
         no_af_positions((5, 6, 7))),
        ("alu16", ("add", "sub", "cmp", "adc", "sbb", "and", "or",
                   "xor", "neg", "inc", "dec"),
         no_af_positions((5, 6, 7))),
        ("alu8", ("add", "sub", "cmp", "adc", "sbb", "and", "or",
                  "xor", "neg", "inc", "dec"),
         no_af_positions((5, 6, 7))),
        ("imul", ("imul64", "imul32", "imul16"),
         lambda iteration, op_index: (0x801, OF_CF_SETCC)),
        ("cmov", ("o64", "b64", "e64", "s64", "p64", "a64", "g64",
                  "l64", "o32", "b32", "e32", "s32", "p32", "a32",
                  "g32", "l32"), all_masks),
        ("shift", ("shl64", "shr64", "sar64", "shl32", "shr16",
                   "sar8"), shift_masks),
        ("rotate", ("rol64", "ror64", "rol32", "ror16"), rotate_masks),
        ("bt", ("bt-positive", "bt-negative"),
         lambda iteration, op_index: (1, 0xFF << 8)),
        ("ucomis", ("ucomisd", "ucomiss"), all_masks),
    ]

    cursor = 0
    failed = False
    for name, operations, masks in families:
        cursor, failures = compare_family(
            reference, candidate, cursor, name, operations, masks)
        if failures:
            failed = True
            for iteration, operation, ref_result, lat_result, flag_diff, setcc_diff in failures:
                print(f"FAIL {name}/{operation} iteration={iteration} "
                      f"result={ref_result:#x}/{lat_result:#x} "
                      f"flag_diff={flag_diff:#x} setcc_diff={setcc_diff:#x}")
        else:
            print(f"PASS {name}: {ITERATIONS * len(operations)} cases")

    if cursor != len(reference):
        raise SystemExit(
            f"unparsed records: parsed={cursor} total={len(reference)}")
    if failed:
        raise SystemExit(1)
    print(f"PASS total: {cursor} defined-result/flag/condition cases")


if __name__ == "__main__":
    main()
