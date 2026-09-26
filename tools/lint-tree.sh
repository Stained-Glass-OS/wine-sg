#!/usr/bin/env bash
# Print the path of a Wine source tree with the whole series applied, for
# make lint's trademark scan of Wine's own resources. Cached by the series'
# hash under ${TMPDIR:-/var/tmp}; the tarball is build.sh's (fetched and
# checked the same way if it is not there yet).
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
. "$HERE/wine-version"
TARBALL="$HERE/build/wine-$WINE_VERSION.tar.xz"
key=$( { cat "$HERE/wine-version" "$HERE/patches/series"; cat "$HERE"/patches/*/*.patch; } | sha256sum | cut -c1-16 )
dest="${TMPDIR:-/var/tmp}/sg-lint-tree-$key"
if [[ ! -f "$dest/.done" ]]; then
    mkdir -p "$HERE/build"
    [[ -f "$TARBALL" ]] || curl -fsSL -o "$TARBALL" "$WINE_TARBALL_URL"
    echo "$WINE_SHA256  $TARBALL" | sha256sum -c - >/dev/null
    rm -rf "$dest"; mkdir -p "$dest"
    tar -C "$dest" -xf "$TARBALL"
    while read -r p; do
        [[ -z "$p" || "$p" == \#* ]] && continue
        patch -d "$dest/wine-$WINE_VERSION" -p1 -s -i "$HERE/patches/$p" >&2
    done < "$HERE/patches/series"
    touch "$dest/.done"
fi
echo "$dest/wine-$WINE_VERSION"
