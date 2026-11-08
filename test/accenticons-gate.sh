#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The icons we draw follow the accent colour; programs' own icons never change
# (wine-sg 6269: theme/icons.py draws the parts that follow it in the key hue,
# user32 turns that hue into the accent's for our own modules' icons):
#
#   api       for two accents and the default: shell32's folder (ExtractIconEx),
#             user32's information icon (LoadIcon, shared: the same handle takes
#             the new accent), the shell's image list (the folder's entry, at the
#             same index), explorer.exe's own icon (File Explorer's, which its
#             windows' taskbar buttons take -- explorer.exe had none: a generic
#             window icon); a program's own icon in the very same purple: untouched
#   desktop   a folder on the desktop, read from the screen: the accent's hue;
#             a new accent (WM_SETTINGCHANGE "ImmersiveColorSet"): the new hue,
#             without a restart
#
#   WINE=/opt/wine-sg/bin/wine test/accenticons-gate.sh   (ARTIFACTS=DIR keeps the screenshots)
# Mutants: SG_MUTANT_ICON_ACCENT_FIXED (user32/cursoricon.c), SG_MUTANT_SHELL_ICONS_KEEP_ACCENT
# (shell32/iconcache.c), SG_MUTANT_DESKTOP_KEEPS_ICONS (explorer/desktop.c),
# SG_MUTANT_ICONS_NO_KEY (theme/icons.py: the drawings without the key hue).
set -u
unset DISPLAY XAUTHORITY
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run, ImageMagick"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v x86_64-w64-mingw32-windres >/dev/null || { echo "SKIP: needs mingw-w64"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-accenticons.XXXXXX)
REAL_HOME=$(getent passwd "$(id -un)" | cut -d: -f6); REAL_HOME=${REAL_HOME:-$SG_REAL_HOME}
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.txt "$ARTIFACTS"/ 2>/dev/null
    [ -n "${KEEP:-}" ] && echo "kept $T $SG_GATE_HOME" || rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

# the probe, with an icon of its own all in the key purple
python3 - "$T/own.ico" <<'PY'
import struct, sys
w = h = 32
data = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0) + bytes([0xE2, 0x2B, 0x8A, 0xFF]) * (w * h) + bytes(4 * h)
open(sys.argv[1], 'wb').write(struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', w, h, 0, 0, 1, 32, len(data), 22) + data)
PY
printf '1 ICON "own.ico"\n' > "$T/own.rc"
(cd "$T" && TMPDIR=/var/tmp x86_64-w64-mingw32-windres own.rc -O coff -o own.res) &&
    TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/accenticons.exe" "$HERE/accenticons-probe.c" "$T/own.res" \
        -lshell32 -lcomctl32 -lole32 -luser32 -lgdi32 -ladvapi32 -lm || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '40 40 40' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -w
U=$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)
DESK=$("$WINE" winepath -u "C:\\users\\$U\\Desktop" 2>/dev/null | tr -d '\r')
sg_prefix_safe "$WINEPREFIX" || exit 1
r=$(realpath -m "$DESK")
case "$r/" in "$REAL_HOME"/*) echo "FAIL  refusing to run: $DESK is $r, in the real home"; exit 1;; esac
case "$r" in "$T"/*|"$SG_GATE_HOME"/*) ;; *) echo "FAIL  the Desktop is outside the gate's folder: $DESK"; exit 1;; esac
mkdir -p "$DESK/Projects"
cp "$T/accenticons.exe" "$WINEPREFIX/drive_c/"

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.0|"") echo "refusing DISPLAY \$DISPLAY"; exit 1;; esac
T="$T"; WINE="$WINE"; WINESERVER="$WINESERVER"; DUMP="$WINEPREFIX/drive_c/desk.txt"; C="$WINEPREFIX/drive_c"
R="\$T/results.txt"; : > "\$R"
pass() { printf 'PASS  %s\n' "\$*" >> "\$R"; }
fail() { printf 'FAIL  %s\n' "\$*" >> "\$R"; }
# api
(cd "\$C" && timeout -s KILL 120 "\$WINE" accenticons.exe api 2>/dev/null </dev/null | tr -d '\r') > "\$T/api.txt"
sed -n 's/^ok  /PASS /p; s/^FAIL /FAIL /p; s/^accent /NOTE  accent /p' "\$T/api.txt" >> "\$R"
grep -q '^RESULT: ' "\$T/api.txt" || fail "api: the probe did not finish"
"\$WINESERVER" -w
# desktop
(cd "\$C" && "\$WINE" accenticons.exe accent 0078d7 >/dev/null 2>&1)
"\$WINESERVER" -w
cd "\$C"
export SG_DESKTOP_DUMP='C:\\desk.txt'
"\$WINE" explorer /desktop=shell,1024x700 > "\$T/explorer.txt" 2>&1 &
i=0; while ! grep -q '^item .* Projects\$' "\$DUMP" 2>/dev/null && [ \$i -lt 240 ]; do sleep 0.25; i=\$((i + 1)); done
sleep 3
idx=\$(tr -d '\r' < "\$DUMP" | grep '^item .* Projects\$' | head -1 | cut -d' ' -f2)
rect=\$(tr -d '\r' < "\$DUMP" | grep "^box \$idx " | head -1 | cut -d' ' -f5 | tr ',' ' ')
[ -n "\$rect" ] || { fail "desktop: no Projects icon in the desktop's dump"; exit 0; }
hue() {  # the mean hue of the strongly coloured pixels in the icon's rectangle
    import -window root "\$T/\$1.png" 2>/dev/null
    python3 -c "
import sys, math, colorsys
from PIL import Image
l, t, r, b = [int(v) for v in sys.argv[2:6]]
im = Image.open(sys.argv[1]).convert('RGB')
sx = sy = n = 0
for y in range(t, b):
    for x in range(l, r):
        h, s, v = colorsys.rgb_to_hsv(*[c / 255 for c in im.getpixel((x, y))])
        if s > 0.45 and v > 0.25:
            sx += math.cos(h * 2 * math.pi); sy += math.sin(h * 2 * math.pi); n += 1
print(-1 if n < 20 else round(math.degrees(math.atan2(sy, sx)) % 360))" "\$T/\$1.png" \$rect
}
near() { python3 -c "import sys; a, b = float(sys.argv[1]), float(sys.argv[2]); d = abs(a - b); d = min(d, 360 - d); sys.exit(0 if a >= 0 and d <= 14 else 1)" "\$1" "\$2"; }
h1=\$(hue blue)
near "\$h1" 207 && pass "desktop: the folder in the accent (hue \$h1, want 207)" || fail "desktop: the folder in the accent (hue \$h1, want 207)"
(cd "\$C" && "\$WINE" accenticons.exe accent 10893e >/dev/null 2>&1)
sleep 4
h2=\$(hue green)
near "\$h2" 143 && pass "desktop: a new accent, the folder follows it without a restart (hue \$h2, want 143)" || fail "desktop: a new accent, the folder follows it without a restart (hue \$h2, want 143)"
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
