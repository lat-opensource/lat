# The floating container tag may omit LoongArch from its manifest.
FROM aosc/aosc-os:container-20260909-loongarch64@sha256:40fe33ba8393bc4420ad88cd3eedf40ff09531e430e58895609da4769a8ad1d2
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
        libssl-dev         \
        make               \
        meson              \
        ninja              \
        pkg-config         \
        python3-setuptools \
        xz"

ENV OMA_NO_CHECK_DBUS=1

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    oma install -y systemd && \
    oma install -y ${DEPENDENCIES}

RUN git config --global --add safe.directory /io
