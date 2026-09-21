#!/usr/bin/env bash
# Build Wine for Stained Glass: one amd64 binary that runs both 64-bit and
# 32-bit Windows applications, with no i386 Linux libraries anywhere.
#
# The whole point is --enable-archs=i386,x86_64 ("new WoW64"). Neither Debian's
# nor WineHQ's packaged Wine is built that way, which is why we build our own.
# See ADR 0002 and ADR 0005 in the stained-glass repo.
set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$HERE/wine-version"

BUILD_DIR="${BUILD_DIR:-$HERE/build}"
SRC_DIR="$BUILD_DIR/wine-$WINE_VERSION"
OBJ_DIR="$BUILD_DIR/obj"
PREFIX="${PREFIX:-/opt/wine-sg}"
DESTDIR="${DESTDIR:-}"
JOBS="${JOBS:-$(nproc)}"

log() { echo "[wine-sg] $*"; }

mkdir -p "$BUILD_DIR"

# --- fetch -----------------------------------------------------------------
TARBALL="$BUILD_DIR/wine-$WINE_VERSION.tar.xz"
if [[ ! -f "$TARBALL" ]]; then
    log "fetching Wine $WINE_VERSION"
    curl -fsSL -o "$TARBALL.tmp" "$WINE_TARBALL_URL"
    mv "$TARBALL.tmp" "$TARBALL"
fi

log "verifying tarball checksum"
echo "$WINE_SHA256  $TARBALL" | sha256sum -c - >/dev/null

# --- unpack and patch ------------------------------------------------------
if [[ ! -d "$SRC_DIR" ]]; then
    log "unpacking"
    tar -C "$BUILD_DIR" -xf "$TARBALL"

    log "applying patches"
    while read -r p; do
        [[ -z "$p" || "$p" == \#* ]] && continue
        log "  $p"
        patch -d "$SRC_DIR" -p1 -s -i "$HERE/patches/$p"
    done < "$HERE/patches/series"
fi

# --- configure -------------------------------------------------------------
# --enable-archs is the reason this repo exists. --without-oss and
# --without-netapi mirror Debian's choices: OSSv4 is not present on Debian and
# libnetapi is not packaged at all. Unlike Debian we keep sane, because S3
# (TWAIN imaging) needs it.
mkdir -p "$OBJ_DIR"
if [[ ! -f "$OBJ_DIR/Makefile" ]]; then
    log "configuring (archs: i386,x86_64)"
    (cd "$OBJ_DIR" && "$SRC_DIR/configure" \
        --enable-archs=i386,x86_64 \
        --prefix="$PREFIX" \
        --with-mingw \
        --with-sane \
        --without-oss \
        --without-netapi \
        --disable-tests)
fi

# --- build -----------------------------------------------------------------
log "building with $JOBS jobs (this takes roughly 12 minutes on 12 cores)"
make -C "$OBJ_DIR" -j"$JOBS"

log "build complete"
if [[ -n "${DO_INSTALL:-}" ]]; then
    log "installing to ${DESTDIR}${PREFIX}"
    make -C "$OBJ_DIR" install ${DESTDIR:+DESTDIR="$DESTDIR"}
fi
