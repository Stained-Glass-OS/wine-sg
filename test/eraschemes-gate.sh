#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Horizon and Glass looks' controls (patches/sg/0743, theme/eras.py).
# David: make each look complete -- buttons, check boxes, scroll bars, menus
# and the system colours too. light.msstyles carries two more colour schemes,
# generated from Light by eras.py, and uxtheme takes them while Style Frame
# (0742) is 1 or 2: Horizon's beige face, dark blue button edges, green
# check marks and radio dots, lavender-blue scroll bar thumbs; Glass's grey
# glossy buttons, blue radio dots, sky-blue selection. Frame 0 keeps the
# Classic scheme. In dark mode the controls take the Dark scheme whatever
# the frames' look (1177): the era schemes are light only. The probe draws
# the parts on a green window; the checks read the screen and the system
# colours.
#
#   WINE=/opt/wine-sg/bin/wine test/eraschemes-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
unset DISPLAY WAYLAND_DISPLAY
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run import convert; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-eraschemes.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
printf '1 24 "%s"\n' "$HERE/theme-gallery.manifest" > "$T/p.rc"
"$WINDRES" "$T/p.rc" -O coff -o "$T/p.o" &&
"$MINGW" -O2 -municode -o "$T/eraschemes-probe.exe" "$HERE/eraschemes-probe.c" "$T/p.o" -luxtheme -lgdi32 -luser32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/eraschemes-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Style' /v Frame /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
P() { "$WINE" eraschemes-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
look() {
    P colour > "$T/\$1.colour"
    "$WINE" eraschemes-probe.exe show & pid=\$!
    sleep 4; import -window root "$T/\$1.png"
    kill \$pid; sleep 1
}
look horizon
"$WINE" reg add 'HKCU\\Software\\Stained Glass\\Style' /v Frame /t REG_DWORD /d 2 /f >/dev/null 2>&1; P notify; sleep 2
look glass
"$WINE" reg add 'HKCU\\Software\\Stained Glass\\Style' /v Frame /t REG_DWORD /d 0 /f >/dev/null 2>&1; P notify; sleep 2
look classic
# Horizon in dark mode: the controls are dark (the era schemes are light only)
"$WINE" reg add 'HKCU\\Software\\Stained Glass\\Style' /v Frame /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize' /v AppsUseLightTheme /t REG_DWORD /d 0 /f >/dev/null 2>&1
P notify; sleep 2
P colour > "$T/horizondark.colour"
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > "$T/session.out" 2>&1

px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
is() { px "$1" "$2" "$3" | awk -F, "{ r = \$1; g = \$2; b = \$3; exit !($4) }"; }
val() { sed -n "s/^$2=//p" "$T/$1.colour"; }
# any pixel of a box that is so
some() {   # some NAME X0 Y0 X1 Y1 CONDITION
    convert "$T/$1.png" -crop "$(($4 - $2))x$(($5 - $3))+$2+$3" txt:- 2>/dev/null | sed -n 's/.*(\([0-9]*\),\([0-9]*\),\([0-9]*\).*/\1 \2 \3/p' \
        | awk "{ r = \$1; g = \$2; b = \$3; if ($6) { found = 1; exit } } END { exit !found }"
}
for s in horizon glass classic; do [ -f "$T/$s.png" ] || { fail "no screenshot $s: $(tail -3 "$T/session.out")"; echo "RESULT: FAIL"; exit 1; }; done

[ "$(val horizon colour)" = Horizon ] && pass "Frame 1: the Horizon scheme" || fail "Frame 1: scheme $(val horizon colour)"
[ "$(val horizon btnface)" = 236,233,216 ] && [ "$(val horizon highlight)" = 49,106,197 ] && [ "$(val horizon menubar)" = 236,233,216 ] \
    && pass "Horizon's system colours: a beige face and menu bar, a blue selection" \
    || fail "Horizon colours: face $(val horizon btnface) highlight $(val horizon highlight) menu bar $(val horizon menubar)"
is horizon 80 20 'b > r + 50 && r < 60 && b < 160' && is horizon 80 35 'r > 225 && g > 225' \
    && pass "a push button with a dark blue edge on a light face ($(px horizon 80 20), $(px horizon 80 35))" \
    || fail "Horizon button: edge $(px horizon 80 20) face $(px horizon 80 35)"
some horizon 180 20 193 33 'g > r + 60 && g > b + 60' && pass "a green check mark" || fail "Horizon check box: no green"
some horizon 220 20 233 33 'g > r + 60 && g > b + 60' && pass "a green radio dot" || fail "Horizon radio button: no green"
is horizon 268 50 'b > r + 25 && b > 200' && pass "a lavender-blue scroll bar thumb ($(px horizon 268 50))" || fail "Horizon thumb: $(px horizon 268 50)"

[ "$(val glass colour)" = Glass ] && pass "Frame 2: the Glass scheme" || fail "Frame 2: scheme $(val glass colour)"
[ "$(val horizondark colour)" = Dark ] && pass "Frame 1 in dark mode: the Dark scheme for the controls (wine-sg 1177)" \
    || fail "Frame 1 in dark mode: scheme $(val horizondark colour) (mutant SG_MUTANT_ERA_LIGHT_IN_DARK keeps Horizon)"
[ "$(val glass btnface)" = 240,240,240 ] && [ "$(val glass highlight)" = 51,153,255 ] \
    && pass "Glass's system colours: a grey face, a sky-blue selection" || fail "Glass colours: face $(val glass btnface) highlight $(val glass highlight)"
is glass 80 20 'r < 140 && r > 80 && b - r < 15 && r - b < 15' && is glass 80 26 'r > 225' && is glass 80 44 'r < 222 && r > 190' \
    && pass "a grey push button, light above its middle and darker below ($(px glass 80 26) / $(px glass 80 44))" \
    || fail "Glass button: edge $(px glass 80 20) top $(px glass 80 26) bottom $(px glass 80 44)"
some glass 220 20 233 33 'b > r + 80 && b > g + 20' && pass "a blue radio dot" || fail "Glass radio button: no blue"
some glass 180 20 193 33 'b > r + 40 && r < 90' && pass "a dark blue check mark" || fail "Glass check box: no dark blue"

[ "$(val classic colour)" != Horizon ] && [ "$(val classic colour)" != Glass ] && [ "$(val classic highlight)" = 112,48,192 ] \
    && pass "Frame 0: the Classic scheme again ($(val classic colour), the purple selection)" \
    || fail "Frame 0: scheme $(val classic colour) highlight $(val classic highlight)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
