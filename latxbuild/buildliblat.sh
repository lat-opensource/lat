#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

configure_build=0
optimization=1
configuration_option=0
debug=0
usage()
{
    echo "Usage: $0 [-c] [-O 0|1|2|3] [-r|-d]"
    echo "  -c  configure build64 (or LIBLAT_BUILD_DIR) before building"
    echo "  -O  LAT optimization level (default: 1)"
    echo "  -r  release build (default)"
    echo "  -d  debug build"
}
while getopts 'cO:rdh' option; do
    case "$option" in
        c) configure_build=1 ;;
        O) optimization=$OPTARG; configuration_option=1 ;;
        r) debug=0; configuration_option=1 ;;
        d) debug=1; configuration_option=1 ;;
        h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))
if [ "$#" -ne 0 ]; then
    usage >&2
    exit 2
fi
case "$optimization" in
    0|1|2|3) ;;
    *) echo "Unsupported optimization level: $optimization" >&2; exit 2 ;;
esac
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LIBLAT_BUILD_DIR:-"$source_dir/build64"}
mkdir -p "$build_dir"
cd "$build_dir"
if [ "$configure_build" -eq 1 ]; then
    set -- --target-list=x86_64-linux-user --enable-latx --enable-kzt \
        --enable-liblat "--optimize-O$optimization" \
        --extra-ldflags=-ldl --disable-docs
    if [ "$debug" -eq 1 ]; then
        set -- "$@" --enable-debug
    fi
    "$source_dir/configure" "$@"
elif [ "$configuration_option" -eq 1 ]; then
    echo "Use -c when changing optimization or debug options" >&2
    exit 2
elif ! grep -q "^CONFIG_BUILD_LIBLAT=y$" config-host.mak; then
    echo "Build directory is not configured for liblat; run with -c" >&2
    exit 2
fi
if command -v ninja >/dev/null 2>&1; then
    ninja
else
    make -j "$(getconf _NPROCESSORS_ONLN)"
fi
