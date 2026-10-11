#!/usr/bin/env python3
"""Compute the deterministic identity shared by AOT v2 build products."""

import argparse
import hashlib
from pathlib import Path


SOURCE_ROOTS = (
    "accel/tcg",
    "crypto",
    "fpu",
    "include",
    "linux-user",
    "target/i386",
    "tcg",
    "tools/latc/aot-v2",
    "tools/latc/cfg",
    "tools/latc/compiler",
    "tools/latc/format",
    "tools/latc/latcd",
    "tools/latc/native",
    "tools/latc/scripts",
)
SOURCE_FILES = ("configure", "meson.build", "VERSION")
SOURCE_SUFFIXES = {
    ".c", ".h", ".S", ".inc", ".py", ".sh", ".map", ".build",
    ".txt", ".json", ".mak",
}
EXCLUDED_PARTS = {"build", "__pycache__", "lat", "tests"}
GENERATED_SOURCE_PATHS = {
    "target/i386/latx/include/format-table.h",
    "target/i386/latx/include/ir2-name.h",
    "target/i386/latx/include/ir2-opcode.h",
    "target/i386/latx/include/la-append.h",
    "target/i386/latx/ir2/la-append.c",
}


def source_files(repository: Path):
    for name in SOURCE_FILES:
        path = repository / name
        if path.is_file():
            yield path
    for name in SOURCE_ROOTS:
        root = repository / name
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            relative = path.relative_to(repository)
            if any(part in EXCLUDED_PARTS or part.startswith("build")
                   for part in relative.parts):
                continue
            if relative.as_posix() in GENERATED_SOURCE_PATHS:
                continue
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                yield path


def paths_for(repository: Path) -> list[Path]:
    return sorted(set(source_files(repository)),
                  key=lambda path: path.relative_to(repository).as_posix())


def source_identity(repository: Path) -> str:
    digest = hashlib.sha256()
    paths = paths_for(repository)
    if not paths:
        raise ValueError(f"no AOT v2 sources found below {repository}")
    for path in paths:
        relative = path.relative_to(repository).as_posix().encode()
        data = path.read_bytes()
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def codegen_configuration(path: Path) -> bytes:
    values = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if (key == "ARCH" or key.startswith(("CONFIG_LATX", "CONFIG_LOONGARCH",
                                            "CONFIG_CAPSTONE")) or
                key in ("CONFIG_DEBUG_TCG", "CONFIG_TCG_INTERPRETER")):
            if key in values:
                raise ValueError(f"duplicate codegen configuration key: {key}")
            values[key] = value
    if not values.get("ARCH"):
        raise ValueError(f"missing codegen host architecture: {path}")
    return "".join(f"{key}={values[key]}\n" for key in sorted(values)).encode()


def compute(repository: Path, config: Path | None = None,
            source_id: str | None = None) -> str:
    identity = source_id or source_identity(repository)
    if len(identity) != 64 or any(c not in "0123456789abcdef" for c in identity):
        raise ValueError("source identity must be a lowercase SHA256 digest")
    if config is None:
        config = repository / "tools/latc/aot-v2/codegen-default.mak"
    if not config.is_file():
        raise ValueError(f"missing codegen configuration: {config}")
    digest = hashlib.sha256(b"LAT-AOT-V2-CODEGEN-CONFIG-1\0")
    digest.update(bytes.fromhex(identity))
    digest.update(codegen_configuration(config))
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--header", type=Path)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--source-only", action="store_true")
    parser.add_argument("--source-id")
    parser.add_argument("repository", type=Path)
    args = parser.parse_args()
    repository = args.repository.resolve()
    if args.list:
        for path in paths_for(repository):
            print(path.relative_to(repository).as_posix())
        return
    identity = (source_identity(repository) if args.source_only else
                compute(repository, args.config, args.source_id))
    if args.header:
        contents = (
            "#ifndef LATC_BUILD_ID_GENERATED_H\n"
            "#define LATC_BUILD_ID_GENERATED_H\n"
            f'#define LATC_BUILD_ID "{identity}"\n'
            "#endif\n"
        )
        if not args.header.exists() or args.header.read_text() != contents:
            args.header.write_text(contents)
        return
    print(identity)


if __name__ == "__main__":
    main()
