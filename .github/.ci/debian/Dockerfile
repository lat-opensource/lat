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
        ninja-build        \
        pkg-config         \
        python3-setuptools \
        xz-utils"

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update \
    && apt-get install -y ${DEPENDENCIES}

RUN git config --global --add safe.directory /io
