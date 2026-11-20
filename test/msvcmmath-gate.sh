#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcm80/msvcm90 (wine-sg 8208, NET-HOST): __setusermatherr_m, which a C++/CLI module's startup calls to
# install its math error handler, was a stub: the handler was never called. It is now the C runtime's
# __setusermatherr (msvcr80/msvcr90): sqrt(-1) calls the handler (_DOMAIN, "sqrt") and returns its value.
# DoDllLanguageSupportValidation is callable (no refusal). 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/msvcmmath-gate.sh
# Baseline: the stub (handler not called).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
for a in x86_64 i686; do command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msvcmmath.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/msvcmmath-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    out=$(timeout -s KILL 120 "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    for m in msvcm80 msvcm90; do
        if printf '%s\n' "$out" | grep -qx "$m.dll=1 1 sqrt 42"; then echo "PASS  $a $m: sqrt(-1) called the handler (_DOMAIN, sqrt) and returned its 42"
        else echo "FAIL  $a $m: $(printf '%s\n' "$out" | grep "^$m.dll=" || echo none)"; RC=1; fi
    done
    printf '%s\n' "$out" | grep -qx 'done=1' && echo "PASS  $a: the probe ran to the end" || { echo "FAIL  $a: the probe did not finish"; RC=1; }
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
