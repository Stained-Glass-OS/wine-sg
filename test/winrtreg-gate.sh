#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Each Windows Runtime class is registered to the DLL that implements it (patches/sg/7504). 6536
# made widl register runtimeclasses that classes.idl only forward-declares (Windows.Media.Playback),
# but its "declared in this file" test compared against a name that follows the preprocessor's
# #line, so a forward declaration in any #included IDL counted too: windows.networking.connectivity
# (whose IDL forward-declares Windows.Foundation.Uri, Networking.HostName and EndpointPair) took
# those classes over, being registered after iertutil and windows.networking.hostname. Uri's
# factory then failed (CreateUri E_FAIL), and with it MSIX/winget installs (AddPackageAsync
# E_INVALIDARG). widl now registers a forward declaration only from the file it compiles.
# Checks a fresh prefix's ActivatableClassId DllPaths, that Uri works, and that wineboot -u
# repairs a prefix registered by the faulty build.
#
#   WINE=/opt/wine-sg/bin/wine test/winrtreg-gate.sh
# Mutant: SG_MUTANT_TYPEREF_ANY_FILE (tools/widl/parser.y; rebuild widl and the affected DLLs).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winrtreg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/msix-probe.exe" "$HERE/msix-probe.c" -lruntimeobject -lshlwapi || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/msix-probe.exe" "$WINEPREFIX/drive_c/"
K='HKLM\Software\Microsoft\WindowsRuntime\ActivatableClassId'
dllpath() { timeout -s KILL 60 "$WINE" reg query "$K\\$1" /v DllPath ${2:-} 2>/dev/null | tr -d '\r' | sed -n 's/.*REG_SZ *//p' | sed 's/.*\\//'; }
check() {  # class expected-dll
    for view in "" "/reg:32"; do
        got=$(dllpath "$1" "$view")
        [ "$got" = "$2" ] && pass "$1 -> $2 ${view:-(64-bit view)}" || fail "$1 ${view:-(64-bit view)}: DllPath is '$got', not $2"
    done
}
check Windows.Foundation.Uri iertutil.dll
check Windows.Networking.HostName windows.networking.hostname.dll
check Windows.Networking.EndpointPair windows.networking.hostname.dll
check Windows.Media.Playback.MediaPlayer windows.media.playback.mediaplayer.dll
check Windows.Media.Playback.BackgroundMediaPlayer windows.media.playback.backgroundmediaplayer.dll
uri() { timeout -s KILL 60 "$WINE" 'C:\msix-probe.exe' uri 'C:\users\x\app.msix' 2>/dev/null | tr -d '\r'; }
uri | grep -qx 'AbsoluteUri=file:///C:/users/x/app.msix' && pass "Windows.Foundation.Uri works (file: URI from a DOS path)" \
    || fail "Uri: $(uri | tr '\n' ' ')"

# a prefix registered by the faulty build: wineboot -u puts the right DLLs back
for c in Windows.Foundation.Uri Windows.Networking.HostName Windows.Networking.EndpointPair; do
    for view in "" "/reg:32"; do
        timeout -s KILL 60 "$WINE" reg add "$K\\$c" /v DllPath /d 'C:\windows\system32\windows.networking.connectivity.dll' /f $view >/dev/null 2>&1
    done
done
"$WINESERVER" -w
[ "$(dllpath Windows.Foundation.Uri)" = windows.networking.connectivity.dll ] || fail "could not set up the faulty registration"
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1
"$WINESERVER" -w
for c in Windows.Foundation.Uri:iertutil.dll Windows.Networking.HostName:windows.networking.hostname.dll Windows.Networking.EndpointPair:windows.networking.hostname.dll; do
    cl=${c%%:*}; want=${c#*:}
    a=$(dllpath "$cl"); b=$(dllpath "$cl" /reg:32)
    [ "$a" = "$want" ] && [ "$b" = "$want" ] && pass "wineboot -u repairs $cl -> $want" || fail "after wineboot -u $cl is '$a' / '$b'"
done
uri | grep -qx 'AbsoluteUri=file:///C:/users/x/app.msix' && pass "...and Uri works again" || fail "Uri after update: $(uri | tr '\n' ' ')"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
