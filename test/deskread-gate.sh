#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# explorer (patch 5953): the desktop explorer /desktop=NAME creates -- the shell's -- is opened with the
# standard rights, so a program the shell starts inherits a desktop handle with READ_CONTROL and can
# read the desktop's security (GetSecurityInfo), as one opening a desktop by name always could.
# Chromium's sandbox needs it: Dropbox 274 crashed at every login (qa4 2026-10-22).
# Mutant SG_MUTANT_DESKREAD (programs/explorer/desktop.c): DESKTOP_ALL_ACCESS alone.
#
#   WINE=/opt/wine-sg/bin/wine test/deskread-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-deskread.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -mwindows -o "$T/deskread-probe.exe" "$HERE/deskread-probe.c" -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/deskread-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
"$WINE" explorer /desktop=sgdeskread,800x600 'C:\\deskread-probe.exe' >/dev/null 2>&1 &
i=0; while [ \$i -lt 60 ] && ! grep -q '^RESULT' "$WINEPREFIX/drive_c/deskread.txt" 2>/dev/null; do sleep 1; i=\$((i+1)); done
"$WINESERVER" -k
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$WINEPREFIX/drive_c/deskread.txt" 2>/dev/null || { echo "FAIL  the probe wrote nothing"; exit 1; }
grep -q '^RESULT: PASS' "$WINEPREFIX/drive_c/deskread.txt"
