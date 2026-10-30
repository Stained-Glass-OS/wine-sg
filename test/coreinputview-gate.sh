#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.ViewManagement.Core.CoreInputView (patches/sg/6210): React Native for Windows (OneDrive)
# asks for it when it makes a text input; the class did not exist and OneDrive's sign-in page never
# showed. One view per thread, held weakly; nothing occludes it; nothing is shown or hidden.
#   WINE=/opt/wine-sg/bin/wine test/coreinputview-gate.sh
# Mutant (dlls/windows.ui/coreinputview.c): SG_MUTANT_NO_CORE_INPUT_VIEW.
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
T=$(mktemp -d /var/tmp/sg-coreinputview.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/coreinputview-probe.exe" "$HERE/coreinputview-probe.c" -lruntimeobject -lole32 -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/coreinputview-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -q -- "$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2"; fi; }
check '^statics=00000000$' "CoreInputView's statics"
check '^view=00000000$' "GetForCurrentView"
check '^same=1 other=1 context=1$' "one view per thread; GetForUIContext gives the thread's"
check '^occlusions=1,0$' "no input panel occludes the view"
check '^show=0 hide=0 show3=0$' "no panel is shown or hidden"
check '^event=00000000,1$' "OcclusionsChanged accepted"
check '^weak=1,1$' "held weakly, resolves to the view"
check '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
