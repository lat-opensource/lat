#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
if [ "$#" -ne 4 ] && [ "$#" -ne 5 ]; then
    echo "usage: $0 provider.so test.c include-directory host-state.S [mode]" >&2
    exit 2
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cc -O2 -g -pthread -UNDEBUG -I"$3" "$2" "$4" -ldl -o "$work/wow64-provider"
if [ "$#" -eq 5 ]; then
    "$work/wow64-provider" "$1" "$5"
else
    "$work/wow64-provider" "$1"
fi
