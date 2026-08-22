FROM aosc/aosc-os:container
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

ARG RUNNER_ARCH
RUN --mount=type=bind,source=install-static-clang.sh,target=/usr/local/bin/install-static-clang \
    [ -n "${RUNNER_ARCH}" ] && install-static-clang

RUN git config --global --add safe.directory /io
