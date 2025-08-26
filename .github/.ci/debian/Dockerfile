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

RUN echo "deb http://snapshot.debian.org/archive/debian-ports/20250801T194242Z/ sid main"  > /etc/apt/sources.list && \
    ( echo 'quiet "true";'; \
      echo 'APT::Get::Assume-Yes "true";'; \
      echo 'APT::Install-Recommends "false";'; \
      echo 'Acquire::Check-Valid-Until "false";'; \
      echo 'Acquire::Retries "5";'; \
    ) > /etc/apt/apt.conf.d/99cpython-portable && \
    rm -f /etc/apt/sources.list.d/*

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update \
    && apt-get install -y ${DEPENDENCIES}

RUN git config --global --add safe.directory /io
