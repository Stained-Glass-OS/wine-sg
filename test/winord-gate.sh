#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows' ordinals of SHELL32, COMCTL32 and SHLWAPI (patches/sg/7703, 7705). Programs import many
# of their functions by ordinal only; Wine left the ordinals it does not declare to winebuild,
# which numbered Wine's named exports into them, so such an import called an unrelated function
# (Teams: SHLWAPI 633). The probe checks every ordinal of Geoff Chappell's public lists: no
# function Wine exports under another name may sit on it (64- and 32-bit).
#
#   WINE=/opt/wine-sg/bin/wine test/winord-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winord.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -I"$HERE" -o "$T/p64.exe" "$HERE/winord-probe.c" || { fail "probe did not build"; exit 1; }
"$MINGW32" -O2 -I"$HERE" -o "$T/p32.exe" "$HERE/winord-probe.c" 2>/dev/null || true
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"; cp "$T"/p*.exe "$C/"
for b in 64 32; do
    [ -f "$C/p$b.exe" ] || continue
    out=$(cd "$C" && timeout -s KILL 60 "$WINE" "p$b.exe" 2>/dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep -qx "Mismatches=0"; then pass "$b-bit: every Windows ordinal is its own function or a stub"
    else fail "$b-bit: $(printf '%s\n' "$out" | grep -c '^Mismatch=') ordinals lead to another function, e.g. $(printf '%s\n' "$out" | grep '^Mismatch=' | head -5 | tr '\n' ' ')"; fi
done
[ "$RC" = 0 ] && echo "winord gate: PASS" || echo "winord gate: FAIL"
exit "$RC"
