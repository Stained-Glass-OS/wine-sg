#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll: RtlComputeImportTableHash (patches/sg/9702; test/imphash-probe.c, test/imphash-expect.py). Imported
# statically by Dependencies (lucasg), which did not start. The probe hashes its own file; the gate checks the
# hash against the import hash test/imphash-expect.py computes from the same file, 64- and 32-bit.
# Mutant: SG_MUTANT_IMPHASH_ZERO (dlls/ntdll/crypt.c).
#   WINE=/opt/wine-sg/bin/wine test/imphash-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-imphash.XXXXXX)
P=imphash-probe; LIBS=""
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/$P.exe" "$HERE/$P.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/${P}32.exe" "$HERE/$P.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for p in $P ${P}32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    (cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" </dev/null 2>/dev/null) | tr -d '\r' > "$T/$p.out"
    cat "$T/$p.out"
    want=$(python3 "$HERE/imphash-expect.py" "$T/$p.exe" | cut -d' ' -f1)
    got=$(sed -n 's/^HASH //p' "$T/$p.out")
    if [ -n "$want" ] && [ "$got" = "$want" ]; then echo "PASS  the hash is the file's import hash ($want)"
    else echo "FAIL  hash $got, the file's import hash is $want"; rc=1; fi
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
