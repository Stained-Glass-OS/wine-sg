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
# The source tree must always be exactly the tarball plus the *current* series.
# It used to be patched only when first unpacked, so a patch added or changed
# later never reached an existing tree -- and a local build quietly shipped
# without it. The series is fingerprinted; when the fingerprint changes, the
# patched tree is rebuilt in a scratch directory and only files whose content
# differs are copied over. That keeps the object tree incremental and correct:
# a file a removed patch reverts gets a fresh mtime and is recompiled, which a
# plain re-extract (restoring the tarball's old mtimes) would not do.
series_fingerprint() {
    {
        cat "$HERE/wine-version"
        while read -r p; do
            [[ -z "$p" || "$p" == \#* ]] && continue
            echo "== $p"
            cat "$HERE/patches/$p"
        done < "$HERE/patches/series"
    } | sha256sum | cut -d' ' -f1
}

# Unpack the tarball and apply the series into directory $1.
prepare_tree() {
    local dest=$1 tmp
    tmp=$(mktemp -d "$BUILD_DIR/unpack.XXXXXX")
    tar -C "$tmp" -xf "$TARBALL"
    while read -r p; do
        [[ -z "$p" || "$p" == \#* ]] && continue
        log "  $p"
        patch -d "$tmp/wine-$WINE_VERSION" -p1 -s -i "$HERE/patches/$p"
    done < "$HERE/patches/series"
    regenerate_theme_images "$tmp/wine-$WINE_VERSION"
    mv "$tmp/wine-$WINE_VERSION" "$dest"
    rmdir "$tmp"
}

# The Light visual style ships pre-rendered .bmp/.cur/.ico, and a normal (non
# maintainer-mode) build packs those rather than re-rendering from the SVGs. Our
# purple recolour (patches/sg/0031) edits the SVGs, so those images must be
# regenerated from them, exactly as tools/buildimage does in maintainer mode --
# otherwise the theme would build blue. RSVG is passed as the bare name because
# tools/buildimage only adds rsvg-convert's -o flag when RSVG equals
# "rsvg-convert" (a full path silently drops it and every render fails).
regenerate_theme_images() {
    local tree=$1 dir="$1/dlls/light.msstyles" svg base ext
    [[ -d "$dir" ]] || return 0
    if ! command -v rsvg-convert >/dev/null || ! command -v icotool >/dev/null || ! command -v convert >/dev/null; then
        log "  WARNING: rsvg-convert/icotool/convert missing; Light theme stays blue (see make deps)"
        return 0
    fi
    log "  rendering the purple Light theme images"
    for svg in "$dir"/*.svg; do
        base=${svg%.svg}
        for ext in bmp cur ico; do
            [[ -f "$base.$ext" ]] || continue
            CONVERT=convert ICOTOOL=icotool RSVG=rsvg-convert \
                perl "$tree/tools/buildimage" "$svg" "$base.$ext" >/dev/null 2>&1 \
                || log "    WARNING: could not render $(basename "$base.$ext")"
        done
    done
}

FINGERPRINT=$(series_fingerprint)
if [[ ! -d "$SRC_DIR" ]]; then
    log "unpacking and applying patches"
    prepare_tree "$SRC_DIR"
    echo "$FINGERPRINT" > "$SRC_DIR/.sg-series"
elif [[ "$(cat "$SRC_DIR/.sg-series" 2>/dev/null)" != "$FINGERPRINT" ]]; then
    log "patch series changed; refreshing the source tree"
    NEW_DIR="$BUILD_DIR/wine-$WINE_VERSION.new"
    rm -rf "$NEW_DIR"
    prepare_tree "$NEW_DIR"
    # diff -rq names every file that differs or exists on one side only; Wine's
    # tree has no spaces in its paths, so its output parses cleanly. It exits 1
    # when it finds differences -- the very case this is for -- which under
    # `set -e -o pipefail` would abort the script mid-refresh, so collect its
    # list first and tolerate that status.
    CHANGES="$BUILD_DIR/series-changes.txt"
    diff -rq -x .sg-series "$NEW_DIR" "$SRC_DIR" > "$CHANGES" || true
    while read -r line; do
        case $line in
            "Files $NEW_DIR/"*" differ")
                rel=${line#"Files $NEW_DIR/"}; rel=${rel%% and *}
                cp "$NEW_DIR/$rel" "$SRC_DIR/$rel"; log "  updated $rel" ;;
            "Only in $NEW_DIR"*)
                rel=${line#"Only in $NEW_DIR"}; rel=${rel#/}; rel=${rel/: //}; rel=${rel#/}
                mkdir -p "$(dirname "$SRC_DIR/$rel")"
                cp -r "$NEW_DIR/$rel" "$SRC_DIR/$rel"; log "  added $rel" ;;
            "Only in $SRC_DIR"*)
                rel=${line#"Only in $SRC_DIR"}; rel=${rel#/}; rel=${rel/: //}; rel=${rel#/}
                rm -rf "${SRC_DIR:?}/$rel"; log "  removed $rel" ;;
        esac
    done < "$CHANGES"
    rm -rf "$NEW_DIR" "$CHANGES"
    echo "$FINGERPRINT" > "$SRC_DIR/.sg-series"
fi

# --- configure -------------------------------------------------------------
# --enable-archs is the reason this repo exists. --without-oss and
# --without-netapi mirror Debian's choices: OSSv4 is not present on Debian and
# libnetapi is not packaged at all. Unlike Debian we keep sane, because S3
# (TWAIN imaging) needs it.
# A previously configured object tree is reused as-is -- that is what makes
# rebuilds and CI caches cheap. But configure bakes absolute paths into the
# generated Makefile, so a tree restored at a different path (a CI cache moved
# between repos, a renamed checkout) would fail in confusing ways. Detect that
# and reconfigure rather than letting it fail later.
mkdir -p "$OBJ_DIR"
if [[ -f "$OBJ_DIR/Makefile" ]] && ! grep -qF "$SRC_DIR" "$OBJ_DIR/Makefile"; then
    log "object tree was configured for a different path; reconfiguring"
    rm -f "$OBJ_DIR/Makefile"
fi
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

    # Strip debug info unless asked not to. Wine builds with -g by default and
    # the result is enormous: 1.5G installed, of which about 1.1G is DWARF.
    # Stripped it is 454M -- for *both* architectures, where Debian's Wine
    # needs 717M (amd64) plus 601M (i386) to cover the same ground.
    #
    # Debian does not strip its Wine, so this is a deliberate divergence. It
    # costs symbolised winedbg backtraces. If you are chasing a crash inside
    # Wine, rebuild with STRIP=0 rather than guessing.
    #
    # PE files need the matching mingw strip, not the host one; the Unix side
    # is ordinary ELF. Only --strip-debug there, so Wine's own exported
    # symbols survive.
    if [[ "${STRIP:-1}" != "0" ]]; then
        local_root="${DESTDIR}${PREFIX}"
        log "stripping debug info"
        pe_names=( -name '*.dll' -o -name '*.exe' -o -name '*.drv' -o -name '*.sys'
                   -o -name '*.ocx' -o -name '*.acm' -o -name '*.cpl' -o -name '*.tlb' )
        find "$local_root/lib/wine/x86_64-windows" -type f \( "${pe_names[@]}" \) \
            -exec x86_64-w64-mingw32-strip {} + 2>/dev/null || true
        find "$local_root/lib/wine/i386-windows" -type f \( "${pe_names[@]}" \) \
            -exec i686-w64-mingw32-strip {} + 2>/dev/null || true
        find "$local_root/lib/wine/x86_64-unix" -name '*.so' \
            -exec strip --strip-debug {} + 2>/dev/null || true
        log "installed size: $(du -sh "$local_root" | cut -f1)"
    else
        log "STRIP=0: keeping debug info (installed size will be around 1.5G)"
    fi
fi
