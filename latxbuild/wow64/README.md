# Build the Loong64 WoW64 DLL

Build `wowlatx.dll` from the LATX source tree with GNU Make, Python 3,
Meson, Ninja and ordinary `loong64-w64-mingw32` llvm-mingw tools on `PATH`.
The build compiles LATX and Capstone from this checkout and cross-compiles
the bundled GLib. Meson downloads the pinned proxy-libintl source on the
first build.

Use Hangover Wine sources with the Loong64 exception-context header and a
Wine build configured for `--enable-archs=loong64,i386`. Build the import
libraries and `winebuild` in that directory first. `winebuild` marks the
PE DLL so Wine can load it from its `loong64-windows` directory.

```sh
make -C /path/to/wine-build \
    tools/winebuild/winebuild \
    dlls/ntdll/loong64-windows/libntdll.a \
    dlls/wow64/loong64-windows/libwow64.a

make -j -C latxbuild/wow64 wowlatx.dll WINE_SRC=/path/to/hangover/wine \
    WINE_PE_BUILD=/path/to/wine-build OUT=/path/to/build-wowlatx
```

The DLL and generated build files go in `OUT`, which defaults to
`build-wowlatx` under the LATX root. Set `WINCC` to the full path of the
Loong64 MinGW Clang driver if the tools are outside `PATH`.

## Build the Unix-mode adapter

Use the `unix` target with the same Wine headers, PE import libraries and
MinGW compiler. Set `CC` to a LoongArch Linux compiler for the Unix adapter.

```sh
make -j -C latxbuild/wow64 unix WINE_SRC=/path/to/hangover/wine \
    WINE_PE_BUILD=/path/to/wine-build OUT=/path/to/build-wowlatx
```

This produces `wowlatx-unix.dll` and `wowlatx-unix.so` in `OUT`. When using
separate build hosts, build either file by passing its full output path as
the target. Build `liblatx-wow64.so` in a [native LATX build](../../docs/wow64.md).
Hangover installs the three files and selects the mode with `HODLL`.
