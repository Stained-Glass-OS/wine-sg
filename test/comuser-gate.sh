#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# combase: a COM class or AppID registered for the user (HKCU\Software\Classes) is found before the
# machine's, as on Windows (6029): per-user installs and packages register their servers there.
#   WINE=/opt/wine-sg/bin/wine test/comuser-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-comuser.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/pkgverb-probe.exe" "$HERE/pkgverb-probe.c" -lole32 -lshell32 -luuid &&
"$MINGW" -O2 -shared -o "$T/pkgverb.dll" "$HERE/pkgverb-cmd.c" -lole32 -luuid -lshell32 2>/dev/null ||
    { fail "the probes did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/pkgverb-probe.exe" "$T/pkgverb.dll" "$C/"

run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
U='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0001}'
M='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0002}'

out=$(run pkgverb-probe.exe create "$U")
expect "$out" create 80040154 "an unregistered class is not found"
run reg add "HKCU\\Software\\Classes\\CLSID\\$U\\InprocServer32" /ve /d 'C:\pkgverb.dll' /f >/dev/null
run reg add "HKCU\\Software\\Classes\\CLSID\\$U\\InprocServer32" /v ThreadingModel /d Apartment /f >/dev/null
out=$(run pkgverb-probe.exe create "$U")
expect "$out" create 00000000 "a class registered for the user is created"
run reg add "HKLM\\Software\\Classes\\CLSID\\$U\\InprocServer32" /ve /d 'C:\missing.dll' /f >/dev/null
out=$(run pkgverb-probe.exe create "$U")
expect "$out" create 00000000 "the user's registration comes before the machine's"
run reg add "HKLM\\Software\\Classes\\CLSID\\$M\\InprocServer32" /ve /d 'C:\pkgverb.dll' /f >/dev/null
run reg add "HKLM\\Software\\Classes\\CLSID\\$M\\InprocServer32" /v ThreadingModel /d Apartment /f >/dev/null
run reg add "HKCU\\Software\\Classes\\CLSID\\$M" /ve /d 'no server here' /f >/dev/null
out=$(run pkgverb-probe.exe create "$M")
expect "$out" create 00000000 "a user's key without the server's key leaves the machine's server"
run reg delete "HKCU\\Software\\Classes\\CLSID\\$U" /f >/dev/null
out=$(run pkgverb-probe.exe create "$U")
printf '%s\n' "$out" | grep -q '^create=' && ! printf '%s\n' "$out" | grep -qx 'create=00000000' &&
    pass "the machine's own registration again once the user's is gone (its DLL is missing)" ||
    fail "after the user's registration is gone: $(printf '%s' "$out" | tr '\n' ' ')"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
