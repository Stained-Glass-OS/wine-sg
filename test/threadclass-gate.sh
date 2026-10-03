#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's second thread can register a window class and make a window
# with it (patches/sg/0775): 0770 made every program's later threads remake
# the desktop's data, and under the session's compositor their classes were
# refused -- .NET's "Failed to create system events window thread" (MeediOS).
# Under Xvfb the desktop's window id is not handed to programs as the
# session's compositor does, so this passes either way here; its mutant
# SG_MUTANT_DESKTOP_REMADE_EVERYWHERE fails it on the installed system
# (run the probe there: QA VM).
#
#   WINE=/opt/wine-sg/bin/wine test/threadclass-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-threadclass.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/threadclass-probe.exe" "$HERE/threadclass-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/threadclass-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > /dev/null 2>&1 &
sleep 6
"$WINE" threadclass-probe.exe > "$T/probe.out" 2>/dev/null
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > /dev/null 2>&1
tr -d '\r' < "$T/probe.out" | sed 's/^/      /'
grep -q '^main OK' "$T/probe.out" && echo "PASS  the main thread registers a class and makes a window" || { echo "FAIL  main thread: $(cat "$T/probe.out")"; RC=1; }
grep -q '^thread OK' "$T/probe.out" && echo "PASS  so does a second thread (as .NET's SystemEvents)" || { echo "FAIL  second thread: $(cat "$T/probe.out")"; RC=1; }
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
