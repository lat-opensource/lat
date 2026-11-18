# Build and install LATX for Loong64 WoW64

Build `wowlatx.dll` from the LATX source tree with GNU Make, Python 3,
Meson, Ninja and ordinary `loong64-w64-mingw32` llvm-mingw tools on `PATH`.
The build compiles LATX and Capstone from this checkout and cross-compiles
the bundled GLib. Meson downloads the pinned proxy-libintl source on the
first build.

Use Wine sources with ordinary Loong64 WoW64 support, including the exception
context and thread-control changes, and a matching Wine runtime. Configure
Wine for `--enable-archs=loong64,i386` and build the import libraries and
`winebuild` first. `winebuild` marks the PE DLLs for installation by Wine.

```sh
make -C /path/to/wine-build \
    tools/winebuild/winebuild \
    dlls/ntdll/loong64-windows/libntdll.a \
    dlls/wow64/loong64-windows/libwow64.a

make -j -C latxbuild/wow64 wowlatx.dll WINE_SRC=/path/to/wine \
    WINE_PE_BUILD=/path/to/wine-build OUT=/path/to/build-wowlatx
```

The DLL and generated build files go in `OUT`, which defaults to
`build-wowlatx` under the LATX root. Set `WINCC` to the full path of the
Loong64 MinGW Clang driver if the tools are outside `PATH`.

## Build the Unix-mode adapter

Use the `unix` target with the same Wine headers, PE import libraries and
MinGW compiler. Set `CC` to a LoongArch Linux compiler for the Unix adapter.

```sh
make -j -C latxbuild/wow64 unix WINE_SRC=/path/to/wine \
    WINE_PE_BUILD=/path/to/wine-build OUT=/path/to/build-wowlatx
```

This produces `wowlatx-unix.dll` and `wowlatx-unix.so` in `OUT`. When using
separate build hosts, build either file by passing its full output path as
the target. Build `liblatx-wow64.so` in a [native LATX build](../../docs/wow64.md).

## Install and run

Run `install-pe` or `install-unix` in `latxbuild/wow64` with the same build
arguments. For `install-unix`, set `LATX_UNIX_BUILD` to the directory containing
the native `liblatx-wow64.so` if it differs from `OUT`.

Both targets install the CPU DLL in `$(PREFIX)/lib/wine/loong64-windows`.
`install-unix` also installs `wowlatx-unix.so` and `liblatx-wow64.so` in
`$(PREFIX)/lib/wine/loongarch64-unix`. Set `PREFIX` to the Wine installation
prefix, which defaults to `/usr/local`. Use `WINE_PE_DIR` and `WINE_UNIX_DIR`
to override the directories and `DESTDIR` for package staging.

For an existing Wine prefix, run `wineboot -u` to copy the installed DLL into
`system32`. Move aside any copy of the prototype DLL that was installed there
manually. Wine includes the installed DLL when creating a new prefix.

Select the CPU DLL per launch. The PE DLL runs the translator inside the DLL.
The Unix-mode DLL calls the Unix adapter, which loads `liblatx-wow64.so`.

```sh
HODLL=wowlatx.dll wine program.exe
HODLL=wowlatx-unix.dll wine program.exe
```

Set `LATX_WOW64_PROVIDER` if the Unix adapter needs to load `liblatx-wow64.so`
from a different path.
