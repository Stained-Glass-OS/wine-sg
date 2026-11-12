#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The accent reaches what was still drawn in the default purple (wine-sg 7746-7748):
#
#   theme    our visual style's images and colours: a check box under the pointer drawn in an accent set
#            before the program started; the system colours the shell applies (the
#            selection's, the hot track's); a new accent, live: both follow it (7746)
#   icons    an icon of a program of our shell (in usr/libexec/stained-glass) takes the
#            accent; the same icon in a program elsewhere keeps its purple (7747)
#   title    a window's shared class icon takes a new accent on WM_SETTINGCHANGE
#            "ImmersiveColorSet", without a restart (7747)
#   notepad  its selection, read from the screen, is a tint of the accent, not of the
#            purple (wine/sg_accent.h, 7748)
#   pins     a program of our shell pinned to the taskbar: its icon, read from the screen,
#            in the accent; a new accent (as Settings announces it): the new one (7747)
#
#   WINE=/opt/wine-sg/bin/wine test/accentall-gate.sh   (ARTIFACTS=DIR keeps the screenshots)
# Mutants: SG_MUTANT_THEME_ACCENT_FIXED (uxtheme/msstyles.c), SG_MUTANT_THEME_KEEPS_ACCENT
# (uxtheme/system.c), SG_MUTANT_SHELL_ICONS_FIXED (user32/cursoricon.c, shell32/iconcache.c),
# SG_MUTANT_TITLE_ICONS_KEEP, SG_MUTANT_SMALL_ICON_KEEP (user32/cursoricon.c), SG_MUTANT_WINE_SHADE_FIXED
# (programs/notepad/main.c), SG_MUTANT_PINS_KEEP_ACCENT (explorer/systray.c).
set -u
unset DISPLAY XAUTHORITY
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v xdotool >/dev/null ||
    { echo "SKIP: needs xvfb-run, ImageMagick, xdotool"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v x86_64-w64-mingw32-windres >/dev/null || { echo "SKIP: needs mingw-w64"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-accentall.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.txt "$ARTIFACTS"/ 2>/dev/null
    [ -n "${KEEP:-}" ] && echo "kept $T $SG_GATE_HOME" || rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

# the probe, with an icon of its own all in the icons' key purple (BlueViolet)
python3 - "$T/own.ico" <<'PY'
import struct, sys
w = h = 32
data = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0) + bytes([0xE2, 0x2B, 0x8A, 0xFF]) * (w * h) + bytes(4 * h)
open(sys.argv[1], 'wb').write(struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', w, h, 0, 0, 1, 32, len(data), 22) + data)
PY
printf '1 ICON "own.ico"\n' > "$T/own.rc"
(cd "$T" && TMPDIR=/var/tmp x86_64-w64-mingw32-windres own.rc -O coff -o own.res) &&
    TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/accentall.exe" "$HERE/accentall-probe.c" "$T/own.res" \
        -luxtheme -lshell32 -luser32 -lgdi32 -ladvapi32 -lm || { echo "FAIL  the probe did not build"; exit 1; }
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -municode -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 ||
    { echo "FAIL  the taskbar probe did not build"; exit 1; }

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
C="$WINEPREFIX/drive_c"
OURS="$C/gate/usr/libexec/stained-glass/shell"
mkdir -p "$OURS"
cp "$T/accentall.exe" "$T/taskbar-probe.exe" "$C/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\Stained Glass\Taskbar' /v DefaultPins /t REG_MULTI_SZ /d 'Gate|C:\gate\usr\libexec\stained-glass\shell\accentall.exe' /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/accentall.exe" "$OURS/"
printf 'The accent colour reaches the selection.\r\nA second line of text to select.\r\n' > "$C/sel.txt"

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.0|"") echo "refusing DISPLAY \$DISPLAY"; exit 1;; esac
T="$T"; WINE="$WINE"; WINESERVER="$WINESERVER"; C="$C"
R="\$T/results.txt"; : > "\$R"
pass() { printf 'PASS  %s\n' "\$*" >> "\$R"; }
fail() { printf 'FAIL  %s\n' "\$*" >> "\$R"; }
probe() {  # NAME ARGS...: run the probe, its ok/FAIL lines into the results
    name=\$1; shift
    (cd "\$C" && timeout -s KILL 120 "\$WINE" "\$@" 2>/dev/null </dev/null | tr -d '\r') > "\$T/\$name.txt"
    sed -n 's/^ok  /PASS /p; s/^FAIL /FAIL /p; s/^accent /NOTE  accent /p' "\$T/\$name.txt" >> "\$R"
    grep -q '^RESULT: ' "\$T/\$name.txt" || fail "\$name: the probe did not finish"
    "\$WINESERVER" -w
}
# theme: the accent set before the program starts
(cd "\$C" && "\$WINE" accentall.exe accent 10893e >/dev/null 2>&1); "\$WINESERVER" -w
probe theme accentall.exe theme
# icons
(cd "\$C" && "\$WINE" accentall.exe accent 10893e >/dev/null 2>&1); "\$WINESERVER" -w
probe icons accentall.exe icons 'C:\\gate\\usr\\libexec\\stained-glass\\shell'
# title: run from our shell's folder
probe title 'C:\\gate\\usr\\libexec\\stained-glass\\shell\\accentall.exe' title
# notepad: select all, read the selection's colour from the screen
(cd "\$C" && "\$WINE" accentall.exe accent 10893e >/dev/null 2>&1); "\$WINESERVER" -w
(cd "\$C" && "\$WINE" notepad.exe 'C:\\sel.txt' >/dev/null 2>&1 &)
i=0; while [ -z "\$(xdotool search --name 'sel' 2>/dev/null | head -1)" ] && [ \$i -lt 120 ]; do sleep 0.5; i=\$((i + 1)); done
w=\$(xdotool search --name 'sel' 2>/dev/null | head -1)
[ -n "\$w" ] || { fail "notepad: no window"; exit 0; }
sleep 3
xdotool windowactivate --sync "\$w" 2>/dev/null; xdotool windowfocus "\$w" 2>/dev/null; sleep 1
xdotool key --window "\$w" ctrl+a; sleep 2
import -window root "\$T/notepad.png" 2>/dev/null
h=\$(python3 -c "
import sys, math, colorsys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
sx = sy = n = 0
W, H = im.size
for y in range(0, H, 2):
    for x in range(0, W, 2):
        h, s, v = colorsys.rgb_to_hsv(*[c / 255 for c in im.getpixel((x, y))])
        if 0.08 <= s <= 0.6 and v >= 0.7:
            sx += math.cos(h * 2 * math.pi); sy += math.sin(h * 2 * math.pi); n += 1
print(-1 if n < 200 else round(math.degrees(math.atan2(sy, sx)) % 360))" "\$T/notepad.png")
python3 -c "import sys; a = float(sys.argv[1]); d = abs(a - 143); d = min(d, 360 - d); sys.exit(0 if a >= 0 and d <= 16 else 1)" "\$h" &&
    pass "notepad: the selection is a tint of the accent (hue \$h, want 143)" ||
    fail "notepad: the selection is a tint of the accent (hue \$h, want 143; the purple's is 265)"
(cd "\$C" && "\$WINE" taskkill /im notepad.exe >/dev/null 2>&1); "\$WINESERVER" -w
# pins: the taskbar's pinned program of our shell, before and after a new accent
cd "\$C"
"\$WINE" explorer /desktop=shell,1024x700 > "\$T/explorer.txt" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "\$T/explorer.txt" 2>/dev/null && [ \$i -lt 120 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 4
rect=\$("\$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | sed -n 's/^pin=//p' | head -1)
[ -n "\$rect" ] || { fail "pins: no pinned program on the taskbar"; exit 0; }
pinhue() {
    import -window root "\$T/\$1.png" 2>/dev/null
    python3 -c "
import sys, math, colorsys
from PIL import Image
l, t, r, b = [int(v) for v in sys.argv[2].split(',')]
im = Image.open(sys.argv[1]).convert('RGB')
sx = sy = n = 0
for y in range(t, b):
    for x in range(l, r):
        h, s, v = colorsys.rgb_to_hsv(*[c / 255 for c in im.getpixel((x, y))])
        if s > 0.45 and v > 0.25:
            sx += math.cos(h * 2 * math.pi); sy += math.sin(h * 2 * math.pi); n += 1
print(-1 if n < 20 else round(math.degrees(math.atan2(sy, sx)) % 360))" "\$T/\$1.png" "\$2"
}
near() { python3 -c "import sys; a, b = float(sys.argv[1]), float(sys.argv[2]); d = abs(a - b); d = min(d, 360 - d); sys.exit(0 if a >= 0 and d <= 14 else 1)" "\$1" "\$2"; }
h1=\$(pinhue pin-green "\$rect")
near "\$h1" 143 && pass "pins: a pinned program of our shell, in the accent (hue \$h1, want 143)" || fail "pins: a pinned program of our shell, in the accent (hue \$h1, want 143)"
"\$WINE" accentall.exe announce ca5010 >/dev/null 2>&1
sleep 5
h2=\$(pinhue pin-orange "\$rect")
near "\$h2" 21 && pass "pins: a new accent, the pin's icon follows it without a restart (hue \$h2, want 21)" || fail "pins: a new accent, the pin's icon follows it without a restart (hue \$h2, want 21)"
EOF
chmod +x "$T/session.sh"
N=$(( 400 + $$ % 500 ))
while [ -e "/tmp/.X$N-lock" ] || [ -e "/tmp/.X11-unix/X$N" ]; do N=$((N + 1)); done
timeout -s KILL 900 xvfb-run -n "$N" -s '-screen 0 1024x700x24' "$T/session.sh" > "$T/session.txt" 2>&1
cat "$T/results.txt" 2>/dev/null
RC=0
[ -s "$T/results.txt" ] || { echo "FAIL  no results: $(tail -5 "$T/session.txt")"; RC=1; }
grep -q '^FAIL' "$T/results.txt" 2>/dev/null && RC=1
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
