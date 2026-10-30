#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A console program's window shows the program's own icon (patches/sg/6201): conhost puts the icons of
# the image its console was made for (its title, the image's path, unless it is a shortcut's name) on
# its window, so the taskbar and the switcher show the program's icon, not a generic one. A console
# program with a red icon of its own, started in a new console, asks its console window for its icons.
# test/conicon-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/conicon-gate.sh
# Mutant (programs/conhost/window.c): SG_MUTANT_CONSOLE_NO_PROGRAM_ICON. (The shell side,
# programs/explorer sg_console_image, SG_MUTANT_SHELL_IGNORES_CONSOLE_IMAGE, is not gated here.)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
for need in Xvfb xdpyinfo "$MINGW" "$WINDRES" python3; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-conicon.XXXXXX)
XP=""
DPY="${CONICON_DPY:-265}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
# a 32x32 32-bit icon, all red
python3 - "$T/red.ico" <<'EOF'
import struct, sys
w = h = 32
pixels = b''.join(struct.pack('<BBBB', 0, 0, 255, 255) for _ in range(w * h))
mask = b'\0' * (4 * h)
bih = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, len(pixels) + len(mask), 0, 0, 0, 0)
image = bih + pixels + mask
head = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', w, h, 0, 0, 1, 32, len(image), 22)
open(sys.argv[1], 'wb').write(head + image)
EOF
printf '1 ICON "%s"\n' "$T/red.ico" > "$T/probe.rc"
"$WINDRES" "$T/probe.rc" -O coff -o "$T/probe.res" || { echo "FAIL  the icon resource did not build"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O2 -municode -mconsole -o "$T/conicon-probe.exe" "$HERE/conicon-probe.c" "$T/probe.res" -lgdi32 -luser32 -lshell32 \
    || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${CONICON_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/conicon-probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" start /wait conicon-probe.exe 'C:\result.txt' >"$T/run.log" 2>&1 </dev/null; [ -n "${CONICON_DEBUG:-}" ] && grep -iE "icon|title|conhost" "$T/run.log" | head -20)
out=$(tr -d '\r' < "$WINEPREFIX/drive_c/result.txt" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^window=1$' "the program has a console window"
chk '^image=1$' "the window names the program's image (SgConsoleImage) for the shell"
chk '^big=1$' "its big icon is the program's"
chk '^small=1$' "its small icon is the program's"
chk '^done=1$' "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
