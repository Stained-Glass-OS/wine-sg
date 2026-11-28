#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# "How do you want to open this file?" offers each Linux app by its name (patches/sg/9517). Every
# Linux app's class (SG.LinuxApp.<desktop id>, sg-shell's sg-linuxapp) starts the same launcher
# program: the dialog, which tells apps apart by their program, showed one entry named after the
# launcher ("Sg-linuxapp64", no icon) for an .svg Inkscape and GIMP both open (linux1's QA VM,
# 2026-11-27). A class that says which app it is -- its Application key (ApplicationName,
# ApplicationIcon, AppUserModelID), as a packaged app's class does on Windows -- is an entry of its
# own, by that name; the one chosen with "Always use this app" is the user's choice.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxopenwith-gate.sh
#   mutant: SG_MUTANT_OPENWITH_BY_PROGRAM (dlls/shell32/openwith.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxopenwith.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxopenwith-probe.c" -lshell32 -lole32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cp "$WINEPREFIX/drive_c/windows/system32/notepad.exe" "$WINEPREFIX/drive_c/windows/system32/launcher.exe"
cd "$WINEPREFIX/drive_c"
timeout 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
grep -qx 'row Gate Draw' "$T/out" && grep -qx 'row Gate Paint' "$T/out" && pass "each Linux app is an entry of its own, by its name (Gate Draw, Gate Paint)" \
    || fail "rows: $(grep '^row' "$T/out" | tr '\n' '|')"
grep -qi '^row launcher' "$T/out" && fail "an entry named after the launcher program" || pass "none named after the launcher program"
grep -qx 'choice SG.LinuxApp.org.gate.Paint' "$T/out" && pass "the one chosen with Always use this app is the user's choice" || fail "choice: $(grep '^choice' "$T/out")"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
