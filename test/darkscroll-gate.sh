#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Dark scroll bars by SetWindowTheme (patches/sg/1474): Windows' programs ask
# for them with SetWindowTheme( hwnd, L"DarkMode_Explorer", NULL ); our
# style carries DarkMode_Explorer::ScrollBar (theme/dark.py, from the Dark
# scheme's), and a window's own scroll bars follow its sub-app name. They
# were light: the class was not in the style and a window's scroll bars were
# drawn with the plain class. test/darkscroll-probe.c measures a list box's
# scroll bar with and without it, in the Light scheme.
#
#   WINE=/opt/wine-sg/bin/wine test/darkscroll-gate.sh
# Mutant: SG_MUTANT_NO_PART_SUBAPP (uxtheme system.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; XP=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-darkscroll.XXXXXX)
unset WAYLAND_DISPLAY
mkdir -p "$T/xdg"; chmod 700 "$T/xdg"; export XDG_RUNTIME_DIR="$T/xdg"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/darkscroll-probe.c" -luxtheme -luser32 -lgdi32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
sed 's/^/      /' "$T/o"
v() { sed -n "s/^$1 .*$2=\([0-9]*\).*/\1/p" "$T/o" | head -1; }
grep -q '^theme active=1' "$T/o" && pass "the visual style is on" || fail "no visual style: $(head -1 "$T/o")"
pt=$(v plain track); dt=$(v dark track); dh=$(v dark thumb)
[ -n "$pt" ] && [ "$pt" -ge 180 ] && pass "a plain list's scroll bar is light (track $pt)" || fail "the plain scroll bar: track '${pt:-none}'"
[ -n "$dt" ] && [ "$dt" -le 80 ] && pass "with DarkMode_Explorer, its track is dark ($dt)" || fail "the DarkMode_Explorer track: '${dt:-none}'"
[ -n "$dh" ] && [ -n "$dt" ] && [ "$dh" -ne "$dt" ] && [ "$dh" -le 160 ] && pass "and its thumb shows on it (thumb $dh)" \
    || fail "the DarkMode_Explorer thumb: '${dh:-none}' on '${dt:-none}'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
