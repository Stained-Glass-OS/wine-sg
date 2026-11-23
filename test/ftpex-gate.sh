#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wininet: FtpPutFileEx / FtpGetFileEx (patches/sg/9707; test/ftpex-probe.c, test/ftpex-server.py): spec stubs
# (the importing program did not load). The local name is Unicode, the remote name ANSI; a file goes up to
# and back from a small passive-mode FTP server on 127.0.0.1 (run the gate in a private network namespace).
# Mutant: SG_MUTANT_FTPEX_STUB (dlls/wininet/ftp.c).
#   WINE=/opt/wine-sg/bin/wine test/ftpex-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ftpex.XXXXXX)
P=ftpex-probe; LIBS="-lwininet"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/$P.exe" "$HERE/$P.c" $LIBS 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/${P}32.exe" "$HERE/$P.c" $LIBS 2>>"$T/cc.log"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
command -v python3 >/dev/null || { echo "SKIP: python3 missing"; exit 77; }
mkdir -p "$T/ftproot"
python3 "$HERE/ftpex-server.py" "$T/port" "$T/ftproot" & SP=$!
trap 'kill $SP 2>/dev/null; cleanup' EXIT INT TERM
i=0; while [ ! -s "$T/port" ] && [ $i -lt 50 ]; do python3 -c 'import time; time.sleep(0.1)'; i=$((i + 1)); done
PORT=$(cat "$T/port" 2>/dev/null)
[ -n "$PORT" ] || { echo "FAIL  the gate's FTP server did not start"; echo "RESULT: FAIL"; exit 1; }
rc=0
for p in $P ${P}32; do
    [ -f "$T/$p.exe" ] || continue
    echo "== $p"
    (cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" "$PORT" </dev/null 2>/dev/null) | tr -d '\r' > "$T/$p.out"
    cat "$T/$p.out"
    grep -qx 'RESULT: PASS' "$T/$p.out" || rc=1
done
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
