#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# combase: an interface's proxy/stub class registered for the user (HKCU\Software\Classes\Interface)
# is found before the machine's, as classes are (6029, 6208): OneDrive installs per user and its
# FileCoAuth server could not marshal IFileCoAuthNotify to the program, whose proxy is registered there.
#   WINE=/opt/wine-sg/bin/wine test/userps-gate.sh
# Mutant (dlls/combase/combase.c): SG_MUTANT_COM_NO_USER_CLASSES.
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
T=$(mktemp -d /var/tmp/sg-userps.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -municode -o "$T/userps-probe.exe" "$HERE/userps-probe.c" -lole32 -luuid ||
    { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/userps-probe.exe" "$C/"

run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
I='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0101}'
UP='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0102}'
MP='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0103}'

out=$(run userps-probe.exe "$I")
expect "$out" ps "80040155 clsid=-" "an interface no one registered has no proxy/stub class"
run reg add "HKCU\\Software\\Classes\\Interface\\$I\\ProxyStubClsid32" /ve /d "$UP" /f >/dev/null
out=$(run userps-probe.exe "$I")
expect "$out" ps "00000000 clsid=$UP" "an interface registered for the user: its proxy/stub class"
run reg add "HKLM\\Software\\Classes\\Interface\\$I\\ProxyStubClsid32" /ve /d "$MP" /f >/dev/null
out=$(run userps-probe.exe "$I")
expect "$out" ps "00000000 clsid=$UP" "the user's registration comes before the machine's"
run reg delete "HKCU\\Software\\Classes\\Interface\\$I" /f >/dev/null
out=$(run userps-probe.exe "$I")
expect "$out" ps "00000000 clsid=$MP" "the machine's once the user's is gone"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
