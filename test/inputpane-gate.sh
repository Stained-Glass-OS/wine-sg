#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The input pane and view settings (patches/sg/1663), in the shell's
# desktop (Xvfb): test/inputpane-probe.c reads UIViewSettings'
# UserInteractionMode (tablet mode on and off) and drives InputPane:
# TryShow starts the on-screen keyboard (App Paths\osk.exe points at the
# probe as fakeosk.exe, which opens an OSKMainClass window), Showing and
# Hiding are raised with its rectangle, OccludedRect follows it, TryHide
# hides it. These were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/inputpane-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_PANE_NEVER_SHOWS (windows.ui/inputpane.c),
# SG_MUTANT_ALWAYS_MOUSE (windows.ui/uiviewsettings.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-inputpane.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/inputpane-probe.exe" "$HERE/inputpane-probe.c" -mwindows -luser32 -lole32 -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/inputpane-probe.exe" "$WINEPREFIX/drive_c/"
cp "$T/inputpane-probe.exe" "$WINEPREFIX/drive_c/fakeosk.exe"
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\osk.exe' /ve /d 'C:\fakeosk.exe' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
timeout -s KILL 180 "$WINE" inputpane-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
