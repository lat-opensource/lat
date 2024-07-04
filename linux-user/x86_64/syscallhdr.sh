#!/bin/sh
# SPDX-License-Identifier: GPL-2.0

in="$1"
out="$2"
my_abis=`echo "($3)" | tr ',' '|'`
prefix="$4"
offset="$5"

fileguard=LINUX_USER_X86_64_`basename "$out" | sed \
    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
    -e 's/[^A-Z0-9_]/_/g' -e 's/__/_/g'`
grep -E "^[0-9A-Fa-fXx]+[[:space:]]+${my_abis}" "$in" | sort -n | (
    echo "#ifndef ${fileguard}"
    echo "#define ${fileguard} 1"
    echo ""

    while read nr abi name entry ; do
    if [ -z "$offset" ]; then
        echo "#define TARGET_NR_${prefix}${name} $nr"
    else
        echo "#define TARGET_NR_${prefix}${name} ($offset + $nr)"
        fi
    done

    echo ""
    echo "#endif /* ${fileguard} */"
) > "$out"
#syscall_64_to_32_map.h
out=$(dirname "$out")/"syscall_64_to_32_map.h"
in=`echo $in | sed 's/x86_64/i386/g;s/_64.tbl/_32.tbl/g'`
my_abis=`echo "($3)" |sed 's/,64/,i386/g' | tr ',' '|'`

fileguard=LINUX_USER_X86_64_MAPS_`basename "$out" | sed \
    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
    -e 's/[^A-Z0-9_]/_/g' -e 's/__/_/g'`
grep -E "^[0-9A-Fa-fXx]+[[:space:]]+${my_abis}" "$in" | sort -n | (
    echo "#ifndef ${fileguard}"
    echo "#define ${fileguard} 1"
    echo ""
    echo "static inline void INIT_SYSCALL_64_TO_32(void)"
    echo "{"
    while read nr abi name entry ; do
        echo "#ifdef TARGET_NR_${prefix}${name}"
        echo "syscall_64_to_32[TARGET32_TARGET_NR_${prefix}${name}] = TARGET_NR_${prefix}${name};"
        echo "#else"
        echo "syscall_64_to_32[TARGET32_TARGET_NR_${prefix}${name}] = TARGET32_TARGET_NR_${prefix}${name};"
        echo "#endif"
    done
    echo "}"

    echo ""
    echo "#endif /* ${fileguard} */"
) > "$out"

# syscall_target_32_nr.h
out=$(dirname "$out")/"syscall_target_32_nr.h"

fileguard=LINUX_USER_I386_TARGET32_`basename "$out" | sed \
    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
    -e 's/[^A-Z0-9_]/_/g' -e 's/__/_/g'`
grep -E "^[0-9A-Fa-fXx]+[[:space:]]+${my_abis}" "$in" | sort -n | (
    echo "#ifndef ${fileguard}"
    echo "#define ${fileguard} 1"
    echo ""

    while read nr abi name entry ; do
    if [ -z "$offset" ]; then
        echo "#define TARGET32_TARGET_NR_${prefix}${name} $nr"
    else
        echo "#define TARGET32_TARGET_NR_${prefix}${name} ($offset + $nr)"
        fi
    maplen="TARGET32_TARGET_NR_${prefix}${name}"
    done

    echo "#define TARGET32_TARGET_NR_LATX_LAST $maplen"
    echo ""
    echo "#endif /* ${fileguard} */"
) > "$out"
