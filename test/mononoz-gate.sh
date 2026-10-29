#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Wine Mono without a drive for Wine's data directory (patches/sg/6185): in a prefix with no Z: the
# shared Wine Mono (/usr/share/wine/mono, the package's) is reached as \\?\unix\...; mscoree refused
# such paths, so .NET programs did not start and every wineboot -u started appwiz's "Wine Mono
# Installer" download dialog and waited for it -- for ever on a display nobody answers (a prefix update).
#   WINE=/opt/wine-sg/bin/wine test/mononoz-gate.sh
# Mutant (dlls/mscoree/metahost.c): SG_MUTANT_MONO_NEEDS_DRIVE.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
MCS=""
for d in "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    f=$(ls "$d"/wine-mono-*/lib/mono/4.5/mcs.exe 2>/dev/null | head -1)
    [ -n "$f" ] && MCS=$f
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mononoz.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
unset WAYLAND_DISPLAY
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
i=0; while ! DISPLAY=":$DPY" xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
printf 'class P { static int Main() { System.Console.WriteLine("hello " + System.Environment.Version.Major); return 7; } }\n' > "$T/hello.cs"
(cd "$T" && DISPLAY= timeout -s KILL 300 "$WINE" "$MCS" -out:hello.exe hello.cs >/dev/null 2>&1 </dev/null)
[ -f "$T/hello.exe" ] || { echo "FAIL  mcs did not build the program"; exit 1; }
cp "$T/hello.exe" "$WINEPREFIX/drive_c/"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
RC=0
# a prefix update on a display: nothing to answer, it finishes
DISPLAY=":$DPY" timeout -s KILL 150 "$WINE" wineboot -u >/dev/null 2>&1; urc=$?
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
echo "      update rc=$urc"
if [ $urc = 0 ]; then echo "PASS  wineboot -u without Z: finishes on a display (no add-on download dialog)"
else echo "FAIL  wineboot -u without Z: did not finish (rc $urc)"; RC=1; fi
out=$(cd "$WINEPREFIX/drive_c" && DISPLAY= timeout -s KILL 120 "$WINE" 'C:\hello.exe' 2>/dev/null </dev/null | tr -d '\r')
echo "      run: $out"
if [ "$out" = "hello 4" ]; then echo "PASS  a .NET program runs without Z:"
else echo "FAIL  a .NET program without Z: (got '$out')"; RC=1; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
