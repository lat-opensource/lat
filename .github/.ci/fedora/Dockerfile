FROM ghcr.io/fedora-remix-loongarch/mini-image:f43
ARG TARGETARCH

ARG DEPENDENCIES="         \
        gcc                \
        g++                \
        make               \
        git                \
        ninja-build        \
        meson              \
        openssl-devel      \
        glib2-devel        \
        tar"

RUN --mount=type=cache,target=/var/cache/dnf,sharing=locked \
    dnf update -y \
    && dnf install -y ${DEPENDENCIES}

RUN git config --global --add safe.directory /io
