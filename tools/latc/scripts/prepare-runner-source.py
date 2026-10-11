#!/usr/bin/env python3
"""Install latc runner adapters into a matching full LAT source tree."""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from aot_v2_sources import load_runner_overlays


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    if new and new in text:
        return
    if old not in text:
        if not new:
            return
        raise SystemExit(f"expected text not found in {path}: {old!r}")
    path.write_text(text.replace(old, new, 1))


def runner_source_map(latc_root: Path,
                      without_aot_v2: bool) -> dict[str, Path]:
    repository_root = latc_root.parents[1]
    try:
        return load_runner_overlays(
            repository_root, latc_root, without_aot_v2
        )
    except (OSError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(f"invalid AOT v2 source manifest: {error}") from error


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("lat_source", type=Path)
    parser.add_argument("--without-aot-v2", action="store_true")
    args = parser.parse_args()
    source = args.lat_source.resolve()
    latc_root = Path(__file__).resolve().parents[1]
    source_map = runner_source_map(latc_root, args.without_aot_v2)
    if not (source / ".latc-staging").is_file():
        raise SystemExit(
            "refusing to modify a LAT checkout: prepare a staging copy with "
            "build-runner.sh"
        )

    for relative, adapter in source_map.items():
        destination = source / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(adapter, destination)
    shutil.copy2(latc_root / "aot-v2/include/latc-build-id.h",
                 source / "include/latc-build-id.h")

    replace_once(source / "linux-user/meson.build",
                 "  'main.c',\n",
                 "  'main.c',\n  'latc-bundle-loader.c',\n")
    if "  'guest-elf-map.c',\n" not in (
            source / "linux-user/meson.build").read_text():
        replace_once(source / "linux-user/meson.build",
                     "  'latc-bundle-loader.c',\n",
                     "  'latc-bundle-loader.c',\n"
                     "  'guest-elf-map.c',\n"
                     "  'latcd-client.c',\n"
                     "  'latc-aot-v2-runner.c',\n")
    replace_once(source / "linux-user/meson.build",
                 "  'latcd-client.c',\n",
                 "  'latcd-client.c',\n  'auto-config.c',\n")
    if (source / "meson.build").is_file():
        replace_once(source / "meson.build",
                     "config_host_data.set_quoted('CONFIG_BINDIR', get_option('prefix') / get_option('bindir'))\n",
                     "config_host_data.set_quoted('CONFIG_BINDIR', get_option('prefix') / get_option('bindir'))\n"
                     "config_host_data.set_quoted('CONFIG_LATC_LIBDIR', get_option('prefix') / get_option('libdir'))\n")
    replace_once(source / "target/i386/latx/sbt/meson.build",
                 "  'aot.c',\n", "  'aot.c',\n  'latc_native_export.c',\n")
    # The minimal static linux-user runner uses neither OpenSSL nor zlib.
    replace_once(source / "configure",
                 '  QEMU_LDFLAGS="-lcrypto -lz $QEMU_LDFLAGS"\n', "")


if __name__ == "__main__":
    main()
