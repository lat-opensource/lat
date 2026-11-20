# Shared by no-LBT regression runners. Fixtures can be built on x86 Linux
# and copied to a LoongArch host without a cross compiler.
no_lbt_build_guest()
{
    source=$1
    output=$2
    shift 2
    if [ -n "${LATX_NO_LBT_GUEST_DIR:-}" ]; then
        fixture="$LATX_NO_LBT_GUEST_DIR/$(basename "$output")"
        test -x "$fixture" || { echo "Missing guest fixture: $fixture"; exit 1; }
        cp "$fixture" "$output"
    elif command -v clang-19 >/dev/null 2>&1; then
        clang-19 --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
            -Wl,--build-id=none "$@" "$source" -o "$output"
    elif command -v clang >/dev/null 2>&1; then
        clang --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -static \
            -Wl,--build-id=none "$@" "$source" -o "$output"
    elif [ "$(uname -m)" = x86_64 ]; then
        cc -nostdlib -static -no-pie -Wl,--build-id=none \
            "$@" "$source" -o "$output"
    else
        echo "SKIP: need clang/LLD or LATX_NO_LBT_GUEST_DIR"
        exit 77
    fi
}
