#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Machine-installed components whose Installer keys this account may only
# read -- a standard user's view, and every account's once SYSTEM has
# re-registered the product (Office's integrator) -- are still found
# (patches/sg/0701). Wine's msi asked for KEY_ALL_ACCESS and got nothing:
# Office could not locate its resource DLLs and every Word start ended in
# "Word was unable to start. (6)".
#
#   WINE=/opt/wine-sg/bin/wine test/msireadonly-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: needs $MINGW"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-msiro.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -mconsole -O2 -o "$T/probe.exe" "$HERE/msireadonly-probe.c" -lmsi -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
grep -q '^writable: state 3 path .*sg-msi-probe.dat' "$T/out" && pass "a machine component is found (INSTALLSTATE_LOCAL)" || fail "writable: $(head -1 "$T/out")"
grep -q '^all access now: 5$' "$T/out" && pass "its keys now refuse KEY_ALL_ACCESS (read-only for this account)" || fail "keys not read-only: $(sed -n 2p "$T/out")"
grep -q '^read-only: state 3 path .*sg-msi-probe.dat' "$T/out" && pass "MsiGetComponentPath still finds it, its path read" || fail "read-only: $(sed -n 3p "$T/out")"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
