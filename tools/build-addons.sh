#!/usr/bin/env bash
# build-addons.sh SRC_DIR OBJ_DIR -- the command-line programs Windows ships
# in System32 that are not Wine's: curl.exe (since Windows 10 1803, curl with
# Schannel, Windows' own TLS) and tar.exe (bsdtar, libarchive's). Built from
# their upstream sources (tools/addons-version: versions and checksums) with
# the mingw cross compilers, statically, both architectures; zlib for curl's
# --compressed and tar's gzip/zip. They are marked as Wine builtins
# (winebuild --builtin) and put where Wine's own programs are, so wineboot
# copies them into C:\windows\system32 and syswow64 like any of Wine's.
# A failure fails the build: an update must not quietly drop them.
#
# Licences: curl (MIT-style "curl" licence), libarchive (BSD-2-Clause), zlib
# (zlib licence): see debian/copyright.
#
# The results: OBJ_DIR/programs/{curl,tar}/{x86_64,i386}-windows/*.exe (a build
# tree's wine finds them there, as it finds Wine's programs); build.sh installs
# them into lib/wine/<arch>-windows.
set -euo pipefail
SRC_DIR=$(cd -- "$1" && pwd) OBJ_DIR=$(cd -- "$2" && pwd)
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$HERE/addons-version"
DL=${ADDONS_DL:-$(dirname "$OBJ_DIR")/addons-dl}
WORK=$OBJ_DIR/addons
JOBS=${JOBS:-$(nproc)}
log() { echo "[wine-sg addons] $*"; }

fetch() {   # URL SHA256
    local f="$DL/${1##*/}"
    mkdir -p "$DL"
    if [[ ! -f "$f" ]]; then
        log "fetching ${1##*/}"
        curl -fsSL -o "$f.tmp" "$1"
        mv "$f.tmp" "$f"
    fi
    echo "$2  $f" | sha256sum -c - >/dev/null || { echo "checksum mismatch: $f" >&2; exit 1; }
}
fetch "$ZLIB_URL" "$ZLIB_SHA256"
fetch "$CURL_URL" "$CURL_SHA256"
fetch "$LIBARCHIVE_URL" "$LIBARCHIVE_SHA256"

STAMP=$(cat "$HERE/addons-version" "$HERE/build-addons.sh" | sha256sum | cut -d' ' -f1)
done_for() { [[ -f "$OBJ_DIR/programs/$1/$2-windows/$1.exe" && "$(cat "$WORK/$2.$1.stamp" 2>/dev/null)" == "$STAMP" ]]; }

for arch in x86_64 i386; do
    host=$arch-w64-mingw32; [[ $arch == i386 ]] && host=i686-w64-mingw32
    if done_for curl $arch && done_for tar $arch; then log "$arch: up to date"; continue; fi
    P=$WORK/$arch
    rm -rf "$P"; mkdir -p "$P"
    log "$arch: zlib $ZLIB_VERSION"
    tar -C "$P" -xzf "$DL/${ZLIB_URL##*/}"
    make -C "$P/zlib-$ZLIB_VERSION" -f win32/Makefile.gcc PREFIX=$host- libz.a >"$P/zlib.log" 2>&1 \
        || { tail -20 "$P/zlib.log"; exit 1; }
    mkdir -p "$P/z/include" "$P/z/lib"
    cp "$P/zlib-$ZLIB_VERSION"/{zlib.h,zconf.h} "$P/z/include/"
    cp "$P/zlib-$ZLIB_VERSION/libz.a" "$P/z/lib/"

    log "$arch: curl $CURL_VERSION (Schannel)"
    tar -C "$P" -xzf "$DL/${CURL_URL##*/}"
    (cd "$P/curl-$CURL_VERSION" && ./configure --host=$host --disable-shared --enable-static \
        --with-schannel --enable-sspi --with-winidn --with-zlib="$P/z" \
        --without-libpsl --without-brotli --without-zstd --without-nghttp2 --without-libssh2 \
        --without-libidn2 --disable-ldap --disable-ldaps --disable-docs \
        CFLAGS="-O2" LDFLAGS="-static -s" >"$P/curl-configure.log" 2>&1 \
        && make -j"$JOBS" >"$P/curl-make.log" 2>&1) || { tail -30 "$P"/curl-*.log; exit 1; }

    log "$arch: bsdtar $LIBARCHIVE_VERSION"
    tar -C "$P" -xJf "$DL/${LIBARCHIVE_URL##*/}"
    (cd "$P/libarchive-$LIBARCHIVE_VERSION" && ./configure --host=$host --disable-shared --enable-static \
        --enable-bsdtar=static --disable-bsdcpio --disable-bsdcat --disable-bsdunzip --disable-acl --disable-xattr \
        --without-openssl --without-xml2 --without-expat --without-lzma --without-bz2lib --without-zstd \
        --without-lz4 --without-libb2 --without-mbedtls --without-nettle --without-cng \
        CPPFLAGS="-I$P/z/include" LDFLAGS="-L$P/z/lib -static -s" CFLAGS="-O2" \
        >"$P/tar-configure.log" 2>&1 \
        && make -j"$JOBS" bsdtar.exe >"$P/tar-make.log" 2>&1) || { tail -30 "$P"/tar-*.log; exit 1; }

    for prog in curl tar; do
        case $prog in
            curl) exe=$P/curl-$CURL_VERSION/src/curl.exe ;;
            tar)  exe=$P/libarchive-$LIBARCHIVE_VERSION/bsdtar.exe ;;
        esac
        out=$OBJ_DIR/programs/$prog/$arch-windows
        mkdir -p "$out"
        cp "$exe" "$out/$prog.exe"
        "$OBJ_DIR/tools/winebuild/winebuild" --builtin "$out/$prog.exe"
        echo "$STAMP" > "$WORK/$arch.$prog.stamp"
    done
    log "$arch: done"
done
