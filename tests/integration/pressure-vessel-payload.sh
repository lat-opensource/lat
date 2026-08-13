#!/bin/sh
set -eu

printf 'LD_LIBRARY_PATH=%s\n' "${LD_LIBRARY_PATH-}"
printf 'PATH=%s\n' "${PATH-}"
printf 'XDG_DATA_DIRS=%s\n' "${XDG_DATA_DIRS-}"
printf 'LD_PRELOAD=%s\n' "${LD_PRELOAD-}"
