#!/bin/sh
set -eu

script=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/latc-native-direct-test.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

printf guest >"$work/guest"
cat >"$work/stage1" <<'EOF'
#!/bin/sh
printf native-image >"$LATC_NATIVE_IMAGE_OUT"
EOF
chmod +x "$work/stage1"

cat >"$work/latc" <<'EOF'
#!/bin/sh
set -eu
command=$1
shift
case "$command" in
    emit-static-tbset)
        test "$1" = "$FAKE_GUEST"
        test "$2" = -o
        printf 'static-tbset\n' >"$3"
        printf 'emit-static-tbset\n' >>"$FAKE_COMPILE_COUNT"
        exit 0
        ;;
    compile) printf 'compile\n' >>"$FAKE_COMPILE_COUNT" ;;
    *) exit 2 ;;
esac
output=
tbset=
while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ]; then
        output=$2
    elif [ "$1" = --tbset ]; then
        tbset=$2
    fi
    shift
done
test -n "$output"
test "$(cat "$tbset")" = "$FAKE_TBSET_CONTENT"
cp "$FAKE_STAGE1" "$output"
chmod +x "$output"
EOF
chmod +x "$work/latc"

for mode in static explicit; do
    set --
    content=static-tbset
    if [ "$mode" = explicit ]; then
        content=explicit-tbset
        printf '%s\n' "$content" >"$work/input.tbset"
        set -- "$work/input.tbset"
    fi
    FAKE_STAGE1="$work/stage1" FAKE_COMPILE_COUNT="$work/$mode-count" \
        FAKE_GUEST="$work/guest" FAKE_TBSET_CONTENT="$content" \
        LATC_NATIVE_IMAGE_OUT="$work/$mode-native-image" \
        "$script" "$work/latc" "$work/runner" "$work/guest" \
        "$work/unused-bundle" "$@"

    test "$(cat "$work/$mode-native-image")" = native-image
    if [ "$mode" = static ]; then
        printf 'emit-static-tbset\ncompile\n' >"$work/expected"
    else
        printf 'compile\n' >"$work/expected"
    fi
    cmp "$work/expected" "$work/$mode-count"
    test ! -e "$work/unused-bundle"
done
printf 'test-compile-aot-native-direct: PASS\n'
