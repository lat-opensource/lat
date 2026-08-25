FROM ghcr.io/fedora-remix-loongarch/mini-image:f43
ARG TARGETARCH

ARG DEPENDENCIES="         \
        clang              \
        make               \
        git                \
        ninja-build        \
        meson              \
        openssl-devel      \
        glib2-devel"

RUN --mount=type=cache,target=/var/cache/dnf,sharing=locked \
    dnf update -y \
    && dnf install -y ${DEPENDENCIES} \
    && clang --version \
    && clang++ --version

# LATX's GCC-oriented configure script passes -export-dynamic.  Translate just
# that linker argument for this isolated clang image without changing shared
# configure behaviour used by the existing GCC jobs.
RUN printf '%s\n' \
        '#!/bin/bash' \
        'args=()' \
        'for arg in "$@"; do' \
        '  case "$arg" in' \
        '    -export-dynamic) args+=("-Wl,-export-dynamic");;' \
        '    *) args+=("$arg");;' \
        '  esac' \
        'done' \
        'compiler="${0##*/}"' \
        'exec "/usr/bin/${compiler}" "${args[@]}"' \
        > /usr/local/bin/clang \
    && cp /usr/local/bin/clang /usr/local/bin/clang++ \
    && chmod 0755 /usr/local/bin/clang /usr/local/bin/clang++

RUN git config --global --add safe.directory /io
