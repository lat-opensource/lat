# Building LATU with Quicktrans

Quicktrans is a Git submodule at `target/i386/latx/quicktrans`.
The submodule tracks the `temp1` branch for explicit remote updates, while
normal checkout uses the exact commit recorded by LATU.

Clone the integration branch and initialize submodules:

```sh
git clone --branch temp2 --recurse-submodules git@github.com:rmjskhy/lat.git
cd lat
./latxbuild/build64.sh -c -f
```

For an existing checkout:

```sh
git submodule sync --recursive
git submodule update --init --recursive
./latxbuild/build64.sh -c -f
```

Run the LoongArch build with the usual LATU build dependencies installed.
Access to both the LATU and Quicktrans repositories is required. The relative
Quicktrans URL uses the same host and authentication method as LATU's origin.
Do not use `git submodule update --remote` for a reproducible build: it selects
the current branch tip instead of the pinned commit.

Quicktrans is compiled by `-f` but disabled at runtime by default. Enable it with:

```sh
LATX_FAST_TRANSLATOR=1 ./build64/latx-x86_64 /path/to/x86_64-program
```

Strict mode is enabled by default when Quicktrans is selected.
The checkout is now a real submodule, not a symlink to `/mnt/disk_a/quicktrans`.
Changes in that independent repository do not automatically change this checkout.
