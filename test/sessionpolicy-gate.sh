#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What Teams asks at start (patches/sg/7706): Windows.System.RemoteDesktop.InteractiveSession
# (IsRemote is SM_REMOTESESSION), Windows.System.Profile.WindowsIntegrityPolicy (S mode: never
# here) and kernel32 WerRegisterAdditionalProcess/WerUnregisterAdditionalProcess (WebView2's
# embedded browser registers its renderers; missing, it aborted Teams).
#
#   WINE=/opt/wine-sg/bin/wine test/sessionpolicy-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sessionpolicy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/p.exe" "$HERE/sessionpolicy-probe.c" -lruntimeobject -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"; cp "$T/p.exe" "$C/"
run() { (cd "$C" && timeout -s KILL 60 "$WINE" p.exe "$@" 2>/dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
out=$(run session)
expect "$out" Factory 0x00000000 "InteractiveSession activates"
remote=$(printf '%s\n' "$out" | sed -n 's/^SystemRemote=//p')
expect "$out" IsRemote "0x00000000 $remote" "IsRemote is what GetSystemMetrics(SM_REMOTESESSION) says"
out=$(run integrity)
expect "$out" Factory 0x00000000 "WindowsIntegrityPolicy activates"
for i in 1 2 3 4; do expect "$out" "Prop$i" "0x00000000 0" "integrity policy property $i is false (no S mode)"; done
expect "$out" AddPolicyChanged 0x00000000 "PolicyChanged can be subscribed"
expect "$out" RemovePolicyChanged 0x00000000 "... and unsubscribed"
out=$(run wer)
expect "$out" Reg0 0x80070057 "WerRegisterAdditionalProcess refuses process 0"
expect "$out" Reg 0x00000000 "it registers a process"
expect "$out" RegAgain 0x00000000 "... again (another thread to capture)"
expect "$out" Unreg 0x00000000 "WerUnregisterAdditionalProcess removes it"
expect "$out" UnregAgain 0x80070490 "... once (then ERROR_NOT_FOUND)"
[ "$RC" = 0 ] && echo "sessionpolicy gate: PASS" || echo "sessionpolicy gate: FAIL"
exit "$RC"
