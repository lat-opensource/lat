FROM ghcr.io/loong64/aosc-os-container:latest
ARG TARGETARCH

ARG DEPENDENCIES="         \
        ccache             \
        curl               \
        gcc                \
        git                \
        libc6              \
        libdrm-dev         \
        libglib2.0-dev     \
        libssl-dev         \
        make               \
        ninja              \
        pkg-config         \
        python3-setuptools \
        xz"

ENV OMA_NO_CHECK_DBUS=1

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    oma install -y ${DEPENDENCIES}

RUN git config --global --add safe.directory /io
