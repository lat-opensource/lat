FROM ghcr.io/fedora-remix-loongarch/mini-image:f43
ARG TARGETARCH

ARG DEPENDENCIES="         \
        ccache             \
        curl               \
        gcc                \
        git                \
        g++                \
        make               \
        ninja-build        \
        meson              \
        openssl-devel      \
        glib2-devel        \
        tar                \
        xz"

RUN --mount=type=cache,target=/var/cache/dnf,sharing=locked \
    dnf update -y \
    && dnf install -y ${DEPENDENCIES}

ARG RUNNER_ARCH
RUN --mount=type=bind,source=install-static-clang.sh,target=/usr/local/bin/install-static-clang \
    [ -n "${RUNNER_ARCH}" ] && install-static-clang

RUN git config --global --add safe.directory /io
