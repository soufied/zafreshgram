#!/usr/bin/env bash

set -euo pipefail

API_ID="API_ID"
API_HASH="API_HASH"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

IMAGE=zafreshgram:centos_env
BIN="out/install/usr/local/bin/zaFreshGram"

CCACHE_HOST_DIR="${CCACHE_HOST_DIR:-$HOME/.cache/zafreshgram-ccache}"

die() { echo "Error: $*" >&2; exit 1; }

usage() {
    sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

image_has_lto_or_debug() {
    docker image inspect "$IMAGE" --format '{{range .Config.Env}}{{println .}}{{end}}' 2>/dev/null \
        | grep '^CFLAGS=' \
        | grep -qE -- '-flto|-gdwarf|-g '
}

REBUILD=0
PACKAGE=1
for arg in "$@"; do
    case "$arg" in
        --rebuild-image) REBUILD=1 ;;
        --no-pkg)        PACKAGE=0 ;;
        -h|--help)       usage; exit 0 ;;
        *)               die "unknown option: $arg (see --help)" ;;
    esac
done

[[ "$(id -u)" -ne 0 ]] || die "do not run as root (makepkg refuses to run as root)"
[[ -n "$API_ID" && -n "$API_HASH" ]] || die "API_ID and API_HASH must be set at the top of $0"

TTY_FLAG=()
[[ -t 1 ]] && TTY_FLAG=(-t)

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    NEED_IMAGE=1
    REASON="image does not exist"
elif [[ "$REBUILD" == "1" ]]; then
    NEED_IMAGE=1
    REASON="--rebuild-image given"
elif image_has_lto_or_debug; then
    NEED_IMAGE=1
    REASON="existing image was built with LTO or debug info"
else
    NEED_IMAGE=0
fi

if [[ "$NEED_IMAGE" == "1" ]]; then
    echo ">> Building Docker image (LTO=0, DEBUG=0): $REASON"
    LTO=0 DEBUG=0 ./Telegram/build/prepare/linux.sh
else
    echo ">> Image $IMAGE is up to date (no LTO, no debug info), skipping."
fi

mkdir -p "$CCACHE_HOST_DIR"

LINK_FLAGS=(-D CMAKE_EXE_LINKER_FLAGS_RELEASE=-s)
if [[ -n "${LLD:-}" ]]; then
    LINK_FLAGS+=(-D CMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -D CMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld)
fi

echo ">> Building zaFreshGram (stripped, ccache: $CCACHE_HOST_DIR)..."
docker run --rm "${TTY_FLAG[@]}" \
    -u "$(id -u)" \
    -v "$ROOT:/usr/src/tdesktop" \
    -v "$CCACHE_HOST_DIR:/var/cache/ccache" \
    "$IMAGE" \
    /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
    -D TDESKTOP_API_ID="$API_ID" \
    -D TDESKTOP_API_HASH="$API_HASH" \
    "${LINK_FLAGS[@]}"

[[ -f "$BIN" ]] || die "binary not found: $BIN"

if file "$BIN" | grep -q "not stripped"; then
    echo ">> Binary is not stripped, stripping..."
    strip --strip-all "$BIN"
fi

if [[ -d out && -n "${CLEAN_BUILD:-}" ]]; then
    echo ">> Removing intermediate files in out/ (keeping out/install)..."
    find out -mindepth 1 -maxdepth 1 ! -name install -exec rm -rf {} +
fi

if [[ "$PACKAGE" == "1" ]]; then
    [[ -f PKGBUILD ]] || die "PKGBUILD not found in $ROOT"
    echo ">> Packaging with makepkg (installs the package, cleans work dirs)..."
    makepkg -sfic
else
    echo ">> Skipping makepkg (--no-pkg)."
fi

echo
echo "Done."
echo "  Binary: $ROOT/$BIN"
file "$BIN" | sed 's/^/  /'
echo "  Size:   $(du -h "$BIN" | cut -f1)"
echo "  out/:   $(du -sh out | cut -f1)"
echo "  ccache: $(du -sh "$CCACHE_HOST_DIR" | cut -f1)"