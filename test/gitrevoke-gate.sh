#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An object of a single-threaded apartment, revoked from the global interface table by another thread,
# is released on its own apartment's thread (7823): .NET's finalizer dropping an STA object it held
# through the table (WinForms' AgileComPointer, an mshtml element) released it on the finalizer thread
# and the object crashed (Devolutions RDM 2025.1 at start).
#   A: the STA runs: revoke S_OK, the object released on its STA;
#   B: the STA uninitialized and ended first, C: it ended without uninitializing, D: it tears down
#      while the revoke runs: no crash, the object released (wherever its teardown left it);
#   and the run gets to its end.
#   WINE=/opt/wine-sg/bin/wine test/gitrevoke-gate.sh   (mutant SG_MUTANT_GIT_RELEASE_HERE, combase marshal.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-gitrevoke.XXXXXX)
export XDG_RUNTIME_DIR="$T/xdg" WAYLAND_DISPLAY=sg-no-wayland
mkdir -m 700 "$XDG_RUNTIME_DIR"
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T" 2>/dev/null; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/gitrevoke-probe.c" -lole32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" ABCD 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'A: revoke 0, released yes (on its STA)' \
    && pass "A: revoked from another thread while the STA runs: released on its STA" \
    || fail "A: $(printf '%s\n' "$out" | grep '^A:' || echo none) (wanted: revoke 0, released on its STA)"
for m in B C D; do
    printf '%s\n' "$out" | grep -q "^$m: revoke .*, released yes" && pass "$m: $(printf '%s\n' "$out" | grep "^$m:" | cut -c4-)" \
        || fail "$m: $(printf '%s\n' "$out" | grep "^$m:" || echo 'no line (crashed?)')"
done
printf '%s\n' "$out" | grep -qx done && pass "the probe ran to its end (no crash)" || fail "the probe did not finish"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
