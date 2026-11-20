#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Dead keys compose with the next character in layouts that are not the X server's (wine-sg 8204, KBD-HKL).
# 6000 made a dead key's ToUnicodeEx return -1, but the next key gave its own character: French ^ then e
# typed "e", not "ê". Now the layout's dead-key table (built from the X compose rules) composes them, as
# Windows' layouts do: ^ e -> ê; ^ x -> "^x" (both); ^ space -> ^; flag 4 leaves the waiting dead key as it
# was; German dead acute then a -> á; TranslateMessage posts WM_DEADCHAR ^ then WM_CHAR ê.
# Headless: its own Xvfb (US map), so French and German come from win32u's tables.
#   WINE=/opt/wine-sg/bin/wine test/kbddead-gate.sh
# Mutants: SG_MUTANT_KBD_NO_COMPOSE (winex11.drv/xkbtables.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
for need in Xvfb "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/fr ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }
[ -f /usr/share/X11/locale/en_US.UTF-8/Compose ] || { echo "SKIP: no X compose rules (libx11-data)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbddead.XXXXXX)
cleanup() { [ -n "$XP" ] && kill "$XP" 2>/dev/null; WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/kbddead-probe.c" -luser32 || { echo "FAIL  probe did not build"; exit 1; }
unset WAYLAND_DISPLAY
Xvfb -displayfd 3 -screen 0 640x480x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
[ -s "$T/display" ] || { echo "SKIP: Xvfb did not start"; exit 77; }
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then printf 'PASS  %s\n' "$3"
    else printf 'FAIL  %s: wanted %s=%s, got %s\n' "$3" "$1" "$2" "$(printf '%s\n' "$out" | grep "^$1=" || echo none)"; RC=1; fi
}
expect layouts 1 "French and German layouts loaded"
expect fr.dead "-1 005e" "French ^ is dead (-1)"
expect fr.e "1 00ea" "then e: ê"
expect fr.dead2 "-1 005e" "^ again"
expect fr.x "2 005e 0078" "then x (no x with ^): ^ and x"
expect fr.dead3 "-1 005e" "^ again"
expect fr.space "1 005e" "then space: the ^ itself"
expect fr.dead4 "-1 005e" "^ with flag 4 (the keyboard state kept)"
expect fr.e4 "1 0065" "then e: plain e (nothing was left waiting)"
expect fr.plain "1 0065" "e with nothing waiting: e"
expect de.dead "-1 00b4" "German dead acute"
expect de.a "1 00e1" "then a: á"
expect translate "dead 005e,char 00ea" "TranslateMessage: WM_DEADCHAR ^, then WM_CHAR ê"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
