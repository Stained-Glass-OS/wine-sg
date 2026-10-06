#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The desktop's rubber band and the icon under the pointer (patch 1260), as
# Windows' desktop draws them, read from the screen (X, and the desktop's
# compositor's screen) and the desktop's dump (SG_DESKTOP_DUMP):
#
#   rubber band   while the mouse is still held, the band is on the screen:
#                 the accent, translucent, over the wallpaper, with a solid
#                 one-pixel accent border; on release it is gone and the icons
#                 it touched are selected. With sg-deskcomp compositing the
#                 desktop (as on the installed system) too -- there it was not
#                 drawn at all, only the selection showed after release
#   hover         the icon under the pointer is lit (a light box behind icon
#                 and title, its border stronger); after the hover delay its
#                 infotip shows below the pointer: the full name (a long one
#                 is cut short under the icon), Type, Size, Date modified; off
#                 the icon, both go; the box is stronger on a selected icon
#   keyboard      Home, the arrows and Ctrl+arrow move the focus (and the
#                 selection), and the focused icon is marked
#   scale, look   at 200 % on a light wallpaper with the Rounded look: the
#                 band and the lit box drawn, the box's corners round
#
#   WINE=/opt/wine-sg/bin/wine test/deskhover-gate.sh   (SG_DESKCOMP=<sg-deskcomp>,
#   default /usr/libexec/stained-glass/sg-deskcomp; ARTIFACTS=DIR keeps the screenshots)
# Mutants (explorer desktop.c): SG_MUTANT_BAND_UNPUBLISHED (band missing under
# sg-deskcomp), SG_MUTANT_NO_HOT, SG_MUTANT_NO_INFOTIP.
set -u
unset DISPLAY XAUTHORITY
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
DESKCOMP="${SG_DESKCOMP:-/usr/libexec/stained-glass/sg-deskcomp}"
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run, xdotool, ImageMagick"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-deskhover.XXXXXX)
REAL_HOME=$(getent passwd "$(id -un)" | cut -d: -f6); REAL_HOME=${REAL_HOME:-$SG_REAL_HOME}
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.txt "$ARTIFACTS"/ 2>/dev/null
    [ -n "${KEEP:-}" ] && echo "kept $T $SG_GATE_HOME" || rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
# a known accent (0,120,215) and a plain dark wallpaper
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\DWM' /v AccentColor /t REG_DWORD /d 0x00D77800 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '40 40 40' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -w
U=$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)
DESK=$("$WINE" winepath -u "C:\\users\\$U\\Desktop" 2>/dev/null | tr -d '\r')
sg_prefix_safe "$WINEPREFIX" || exit 1
r=$(realpath -m "$DESK")
case "$r/" in "$REAL_HOME"/*) echo "FAIL  refusing to run: $DESK is $r, in the real home"; exit 1;; esac
case "$r" in "$T"/*|"$SG_GATE_HOME"/*) ;; *) echo "FAIL  the Desktop is outside the gate's folder: $DESK"; exit 1;; esac
mkdir -p "$DESK"
echo a > "$DESK/a.txt"
echo bb > "$DESK/b.txt"
echo long > "$DESK/A rather long file name that the desktop cuts short.txt"

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.0|"") echo "refusing DISPLAY \$DISPLAY"; exit 1;; esac
T="$T"; WINE="$WINE"; WINESERVER="$WINESERVER"; DESKCOMP="$DESKCOMP"; DUMP="$WINEPREFIX/drive_c/desk.txt"
. "$HERE/deskhover-steps.sh"
EOF
chmod +x "$T/session.sh"
# a display of its own, away from the low numbers other gates take with -a
N=$(( 400 + $$ % 500 ))
while [ -e "/tmp/.X$N-lock" ] || [ -e "/tmp/.X11-unix/X$N" ]; do N=$((N + 1)); done
timeout -s KILL 900 xvfb-run -n "$N" -s '-screen 0 1024x700x24' "$T/session.sh" > "$T/session.txt" 2>&1
cat "$T/results.txt" 2>/dev/null
RC=0
[ -s "$T/results.txt" ] || { echo "FAIL  no results: $(tail -5 "$T/session.txt")"; RC=1; }
grep -q '^FAIL' "$T/results.txt" 2>/dev/null && RC=1
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
