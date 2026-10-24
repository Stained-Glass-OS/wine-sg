#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Other programs' icons in the shell (6083). Under Wine an HICON belongs to the process that made it, so
# explorer could not draw the class or WM_GETICON icon of another program's window ("Error retrieving icon
# frame"): the Alt+Tab switcher and Task View cards showed no program icons at all. They now use the icon of the
# window's program file, as the taskbar's buttons already did. And ExtractIconExW(file, 0, &large, &small, 1) on a
# file with no icons (conhost.exe: a console window's button) handed back whatever was on the stack, which the
# taskbar then drew as nothing; now NULL, so the button shows the generic program icon.
# The probe is a program whose icon is solid red; the switcher is opened with Alt+Tab on a shell desktop and the
# screen read for red where the switcher is.
#   WINE=/opt/wine-sg/bin/wine test/iconcross-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutants: SG_MUTANT_SWITCHER_FOREIGN_ICON (explorer), SG_MUTANT_EXTICON_GARBAGE (user32).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb xdotool import convert python3 $MINGW x86_64-w64-mingw32-windres; do
    command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }
done
T=$(mktemp -d /var/tmp/sg-iconcross.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

python3 - "$T/red.ico" <<'EOP'
import struct, sys
def image(n):
    mask = n * ((n + 31) // 32 * 4)
    head = struct.pack('<IiiHHIIiiII', 40, n, n * 2, 1, 32, 0, n * n * 4 + mask, 0, 0, 0, 0)
    return head + struct.pack('<BBBB', 0, 0, 255, 255) * (n * n) + b'\0' * mask
images = [image(16), image(32)]
out, off = struct.pack('<HHH', 0, 1, len(images)), 6 + 16 * len(images)
for n, data in zip((16, 32), images):
    out += struct.pack('<BBBBHHII', n, n, 0, 0, 1, 32, len(data), off)
    off += len(data)
open(sys.argv[1], 'wb').write(out + b''.join(images))
EOP
echo '1 ICON "red.ico"' > "$T/red.rc"
(cd "$T" && x86_64-w64-mingw32-windres red.rc -O coff -o red.o) &&
    TMPDIR=/var/tmp "$MINGW" -O1 -municode -mwindows -o "$T/probe.exe" "$HERE/iconcross-probe.c" "$T/red.o" \
        -lshell32 -lgdi32 2>"$T/cc.log" || { cat "$T/cc.log"; fail "probe did not build"; exit 1; }

if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"

# ExtractIconExW on a file without icons
out=$(timeout -s KILL 60 "$WINE" 'C:\probe.exe' extract 'C:\windows\system32\conhost.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'count=0' && pass "conhost.exe has no icons: ExtractIconExW returns 0" || fail "conhost count: $out"
printf '%s\n' "$out" | grep -qx 'large_null=1' && printf '%s\n' "$out" | grep -qx 'small_null=1' \
    && pass "and hands back no handles (NULL), not stack contents" || fail "handles for a file with no icons: $out"
out=$(timeout -s KILL 60 "$WINE" 'C:\probe.exe' extract 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -qx 'large_icon=1' && pass "a file with an icon still gives it" || fail "own icon: $out"

# the switcher
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' window >/dev/null 2>&1 &
i=0; while ! xdotool search --name 'Icon cross probe' >/dev/null 2>&1 && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
red() {   # red pixels above the taskbar and below the probe's own window (it is at the top left)
    convert "$1" -crop 1024x450+0+210 +repage -fx '(r>0.8 && g<0.25 && b<0.25) ? 1 : 0' -format '%[fx:int(mean*w*h+0.5)]' info: 2>/dev/null | tail -n 1
}
import -window root "$T/before.png"
xdotool keydown alt; sleep 0.4; xdotool key Tab; sleep 2
import -window root "$T/switcher.png"
xdotool keyup alt; sleep 1
b=$(red "$T/before.png"); s=$(red "$T/switcher.png")
echo "      red pixels: before $b, switcher open $s"
[ "${b:-x}" = 0 ] && pass "no red where the switcher opens before Alt+Tab" || fail "red before Alt+Tab: $b"
[ "${s:-0}" -ge 100 ] && pass "Alt+Tab: the probe's card carries its program's (red) icon ($s px)" ||
    fail "no program icon in the switcher ($s red px)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
