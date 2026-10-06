#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The desktop's icons survive an erase-only repaint (1127). wineserver
# validates the desktop window as soon as it is erased outside its own
# BeginPaint (it "only gets erased"), so the WM_PAINT that draws explorer's
# icons never came after UpdateWindow / RDW_ERASENOW -- SetSysColors'
# repaint of everything is one. Under Xvfb, in the shell:
#   1. a program calls SetSysColors (the colours it had): the icons are still
#      on the desktop afterwards (the fix draws again what was erased; this
#      one also passed before it -- the erase there is not shown)
#   2. the shell's first start after a scale and look change (LogPixels 192,
#      the Rounded look, a wallpaper -- uxtheme sets the colours while the
#      taskbar is made) shows its icons within 20 s; it stalled for minutes
#      with an empty desktop (David's desktop agent, 2026-10-06: "stuck
#      repainting the pager")
#
#   WINE=/opt/wine-sg/bin/wine test/deskerase-gate.sh
#   Mutant: SG_MUTANT_DESKTOP_ERASE_ONLY (explorer desktop.c): 2 fails.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb import python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3-pil missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-deskerase.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] && echo "kept $T" || rm -rf "$T"' EXIT INT TERM
cat > "$T/probe.c" <<'EOF'
#include <windows.h>
/* SetSysColors with the colours there were: a repaint of everything (erase-only for the desktop) */
int main(void)
{
    INT el = COLOR_WINDOW;
    COLORREF c = GetSysColor( COLOR_WINDOW );
    return !SetSysColors( 1, &el, &c );
}
EOF
"$MINGW" -O2 -o "$T/probe.exe" "$T/probe.c" || { fail "the probe did not build"; exit 1; }
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
case "$DISPLAY" in :0|:0.0|:) echo "FAIL  refusing DISPLAY $DISPLAY"; exit 1;; esac
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '40 40 40' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
U=$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)
DESK=$("$WINE" winepath -u "C:\\users\\$U\\Desktop" 2>/dev/null | tr -d '\r')
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
sg_prefix_safe "$WINEPREFIX" || exit 1
r=$(realpath -m "$DESK")
case "$r" in "$T"/*|"$SG_GATE_HOME"/*) ;; *) echo "FAIL  the Desktop is outside the gate's folder: $DESK"; exit 1;; esac
mkdir -p "$DESK"; echo b > "$DESK/b.txt"
DUMP="$WINEPREFIX/drive_c/desk.txt"
has_b() { grep -q '^item .* b\.txt' "$DUMP" 2>/dev/null; }
start_shell() {
    rm -f "$DUMP"
    (cd "$WINEPREFIX/drive_c" && SG_DESKTOP_DUMP='C:\desk.txt' "$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &)
    i=0; while ! has_b && [ $i -lt $(($1 * 4)) ]; do sleep 0.25; i=$((i + 1)); done
    has_b
}
# the icon's middle: "r g b" there
px() { import -window root "$T/shot.png" 2>/dev/null
       python3 -c "from PIL import Image; print('%d %d %d' % Image.open('$T/shot.png').convert('RGB').getpixel(($1, $2)))"; }
bg() { [ "$1" = "40 40 40" ]; }

# --- 1. an erase-only repaint of a running shell -----------------------------------------
if start_shell 60; then
    sleep 3
    xy=$(tr -d '\r' < "$DUMP" | sed -n 's/^item [0-9]* \([0-9]*\),\([0-9]*\) file b\.txt$/\1 \2/p')
    set -- ${xy:-0 0}
    p0=$(px "$1" "$2")
    "$WINE" "$T/probe.exe"; sleep 3
    p1=$(px "$1" "$2")
    if bg "$p0"; then fail "the icon of b.txt was not drawn at $1,$2 to begin with ($p0)"
    elif bg "$p1"; then fail "after SetSysColors the icon of b.txt is gone: $p0 -> $p1 at $1,$2 (the wallpaper)"
    else pass "after SetSysColors the desktop's icons are still drawn ($p0 -> $p1 at $1,$2)"; fi
else
    fail "the shell did not come up (no b.txt on its desktop in 60 s)"
fi
"$WINESERVER" -k 2>/dev/null; sleep 2

# --- 2. the first start after a scale and look change -------------------------------------
python3 -c "from PIL import Image; Image.new('RGB', (64, 64), (235, 235, 235)).save('$WINEPREFIX/drive_c/light.bmp')"
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d 192 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d 'C:\light.bmp' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v WallpaperStyle /d 2 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Style' /v Rounded /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -k 2>/dev/null; sleep 2
s=$(date +%s)
if start_shell 20; then pass "after a scale and look change the shell's desktop is up with its icons in $(( $(date +%s) - s )) s"
else fail "after a scale and look change the shell's desktop has no icons after 20 s: '$(head -1 "$DUMP" 2>/dev/null)'"; fi

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
