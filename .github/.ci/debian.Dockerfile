FROM ghcr.io/loong64/debian:trixie-slim
ARG TARGETARCH

ARG DEPENDENCIES="         \
        ccache             \
        curl               \
        gcc                \
        git                \
        g++                \
        libc6              \
        libdrm-dev         \
        libglib2.0-dev     \
        libkeyutils-dev    \
        libssl-dev         \
        lsb-release        \
        make               \
        meson              \
        ninja-build        \
        pkg-config         \
        python3-setuptools \
        xz-utils"

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update \
    && apt-get install -y ${DEPENDENCIES}

ARG RUNNER_ARCH
RUN --mount=type=bind,source=install-static-clang.sh,target=/usr/local/bin/install-static-clang \
    [ -n "${RUNNER_ARCH}" ] && install-static-clang

RUN git config --global --add safe.directory /io
