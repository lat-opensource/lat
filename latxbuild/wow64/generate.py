# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
LATX = ROOT / "target/i386/latx"
TRACE_DIRS = (
    "", "accel/tcg", "crypto", "hw/core", "linux-user", "qapi", "qom",
    "target/i386", "target/i386/latx", "util",
)


def run(script, *args, cwd=None):
    subprocess.run([sys.executable, "-B", str(script), *map(str, args)],
                   cwd=cwd, check=True)


def generate_headers(output):
    trace = output / "trace"
    for folder in ("include", "ir2", "qapi", "trace"):
        (output / folder).mkdir(parents=True, exist_ok=True)

    shutil.copyfile(LATX / "inst_template.json", output / "inst_template.json")
    run(LATX / "convert.py", cwd=output)
    run(ROOT / "scripts/qapi-gen.py", "-o", output / "qapi", "-b",
        ROOT / "qapi/qapi-schema.json")

    tracetool = ROOT / "scripts/tracetool.py"
    for directory in TRACE_DIRS:
        group = directory.replace("/", "_").replace("-", "_") or "root"
        run(tracetool, "--backends=nop", f"--group={group}", "--format=h",
            ROOT / directory / "trace-events", trace / f"trace-{group}.h")

    for name, fmt in (
        ("generated-tcg-tracers.h", "tcg-h"),
        ("generated-helpers.h", "tcg-helper-h"),
        ("generated-helpers-wrappers.h", "tcg-helper-wrapper-h"),
    ):
        run(tracetool, "--backends=nop", "--group=root", f"--format={fmt}",
            ROOT / "trace-events", trace / name)


def generate_cross_file(output, compiler):
    # The compiler may be named through a symlink that selects the PE ABI.
    # Keep that basename when resolving it from PATH.
    cc = shutil.which(compiler)
    if not cc or not cc.endswith("clang"):
        raise SystemExit(f"Expected a Loong64 MinGW clang compiler, got {compiler}")
    subprocess.run([cc, "-fsyntax-only", "-include",
                    str(Path(__file__).with_name("config-host.h")), "-x", "c", "-"],
                   input=b"", check=True)
    prefix = str(Path(cc).absolute())[:-len("clang")]
    binaries = {"c": "clang", "cpp": "clang++", "ar": "ar", "strip": "strip",
                "windres": "windres"}
    lines = ["[binaries]"]
    lines.extend(f"{key} = {prefix + tool!r}" for key, tool in binaries.items())
    lines.extend((
        "", "[host_machine]", "system = 'windows'", "cpu_family = 'loongarch64'",
        "cpu = 'loongarch64'", "endian = 'little'",
        "", "[properties]", "needs_exe_wrapper = true",
    ))
    contents = "\n".join(lines) + "\n"
    if not output.exists() or output.read_text() != contents:
        output.write_text(contents)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--compiler", help="generate a Meson cross file for the compiler")
    args = parser.parse_args()
    if args.compiler:
        generate_cross_file(args.output.resolve(), args.compiler)
    else:
        generate_headers(args.output.resolve())


if __name__ == "__main__":
    main()
