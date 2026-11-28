#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program asking for attention flashes its taskbar button (patches/sg/9513), as a
# Windows program's FlashWindowEx does: a chat's new message, a download done while it is
# behind. Its window is framed by the shell (sg_embed.c); it asks with its urgency hint (ICCCM
# WM_HINTS: GTK's gtk_window_set_urgency_hint) or with _NET_WM_STATE_DEMANDS_ATTENTION asked of
# the window manager (Qt's QApplication::alert, Electron's flashFrame). A real xterm, Notepad
# in front:
#   - urgent: its button turns orange; no longer urgent: it stops;
#   - DEMANDS_ATTENTION asked: orange again; brought to the front: it stops.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxattention-gate.sh
#   mutant: SG_MUTANT_EMBED_NO_ATTENTION (dlls/winex11.drv/sg_embed.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xdotool xterm xwininfo import cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxattention.XXXXXX); XP=; XT=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; [ -n "$XT" ] && kill "$XT" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/netwm-attention" -x c - -lX11 <<'EOF' || { echo "FAIL  helper did not build"; exit 1; }
/* netwm-attention WINDOW 1|0: _NET_WM_STATE_DEMANDS_ATTENTION asked of the window manager (EWMH) */
#include <stdlib.h>
#include <X11/Xlib.h>
int main( int argc, char **argv )
{
    Display *d = XOpenDisplay( NULL );
    XEvent ev = { 0 };
    if (argc < 3 || !d) return 2;
    ev.xclient.type = ClientMessage;
    ev.xclient.window = strtoul( argv[1], NULL, 0 );
    ev.xclient.message_type = XInternAtom( d, "_NET_WM_STATE", False );
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = atoi( argv[2] );
    ev.xclient.data.l[1] = XInternAtom( d, "_NET_WM_STATE_DEMANDS_ATTENTION", False );
    ev.xclient.data.l[3] = 1;
    XSendEvent( d, DefaultRootWindow( d ), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev );
    XCloseDisplay( d );
    return 0;
}
EOF
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/fglock-probe.exe" "$HERE/fglock-probe.c" -luser32 -lgdi32 || { echo "FAIL  button probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$T/fglock-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
xterm -geometry 50x12+120+120 -bg '#30a060' -fg white -title 'Linux Terminal' -e sh -c "cd '$T'; exec sh" & XT=$!
i=0; while [ -z "$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
XID=$(xwininfo -root -tree | awk '/"Linux Terminal"/ {print $1; exit}')
printf '%d shown - XTerm\tLinux Terminal\nEND\n' "$XID" > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 5
cd "$WINEPREFIX/drive_c"
k=0; while [ $k -lt 80 ]; do
    case "$(xwininfo -id "$XID" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')" in ""|"(the root window)"*) ;; *) break ;; esac
    sleep 0.25; k=$((k + 1)); done
[ $k -lt 80 ] && pass "the Linux window is framed" || fail "the Linux window was not framed"
"$WINE" notepad >/dev/null 2>&1 &
sleep 4
"$WINE" probe.exe activate Notepad "" >/dev/null 2>&1; sleep 1.5
pix() { import -window root "$T/shot.png" 2>/dev/null
        python3 -c "from PIL import Image; import sys; print(*Image.open(sys.argv[1]).convert('RGB').getpixel((int(sys.argv[2]), int(sys.argv[3]))))" "$T/shot.png" "$1" "$2"; }
orange() { set -- $1; [ "$1" -ge 200 ] && [ "$2" -ge 60 ] && [ "$2" -le 150 ] && [ "$3" -le 80 ]; }
bpoint() { set -- $("$WINE" fglock-probe.exe button 'Linux Terminal' 2>/dev/null | tr -d '\r'); [ $# = 4 ] && echo "$(( $1 + $3 - 6 )) $(( $2 + 5 ))"; }
# lit at some sample within 1.5 s (it blinks, then stays lit)
lit() { _l=0; for _k in 1 2 3 4 5 6; do orange "$(pix $1)" && _l=1; sleep 0.25; done; [ $_l = 1 ]; }
bp=$(bpoint)
if [ -z "$bp" ]; then fail "no taskbar button for the Linux window"
else
    lit "$bp" && fail "its button orange before it asked ($(pix $bp))" || pass "its button is plain before it asks"
    xdotool set_window --urgency 1 "$XID"; sleep 0.5
    lit "$bp" && pass "urgent (WM_HINTS), its button flashes orange" || fail "urgent: its button never orange ($(pix $bp))"
    xdotool set_window --urgency 0 "$XID"; sleep 1
    orange "$(pix $bp)" && fail "no longer urgent, its button still orange" || pass "no longer urgent, it stops"
    "$T/netwm-attention" "$XID" 1; sleep 0.5
    lit "$bp" && pass "_NET_WM_STATE_DEMANDS_ATTENTION asked: orange" || fail "DEMANDS_ATTENTION: never orange ($(pix $bp))"
    sleep 3.5
    orange "$(pix $bp)" && pass "after its blinks it stays orange" || fail "after its blinks: $(pix $bp)"
    "$WINE" probe.exe activate SgLinuxWindow 'Linux Terminal' >/dev/null 2>&1; sleep 1.5
    orange "$(pix $bp)" && fail "in front, its button still orange" || pass "brought to the front, it stops"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
