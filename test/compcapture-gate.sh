#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Screen capture for Windows programs under the session's compositor (wine-sg
# 7857 winex11, 7856 graphicscapture; DXVK's duplication: sg-image
# dxvk/patches/0012): the pixels come from the compositor's own screen capture
# for ordinary programs -- what any program gets, with the lock screen and
# consent prompts blacked out -- not from the X server.
#
# A private headless sg-compositor (pixman; its own XDG_RUNTIME_DIR, never the
# person's) with XWayland, Wine's shell desktop and sg-deskcomp, a window of
# four colours in one process (test/compcapture-probe.c), and a red Linux
# window (xterm). Another process reads the screen through
#   - GDI (GetDC(NULL) + BitBlt), 64- and 32-bit; 10000 GetPixel calls under 5
#     seconds (the picture reused for a frame) and a change seen right after,
#   - DXGI Desktop Duplication (Wine's dxgi; and DXVK's with SG_DXVK_DIR),
#   - Windows.Graphics.Capture: a monitor item, a window item, no frame while
#     nothing changes, the pointer drawn with IsCursorCaptureEnabled, the
#     yellow border (sg-compositor's capture border) on the screen round the
#     captured window while the session runs -- not in any capture -- and
#     gone after it,
# and must see the four colours AND the Linux window red (only the
# compositor's picture has it); while the session is locked (LOCK on the
# control socket) GDI, DXGI and WGC read black; after UNLOCK, the picture
# again. Under a plain X server (Xvfb, no compositor) GDI still reads the
# window (the X server's picture, as before).
#
#   WINE=/opt/wine-sg/bin/wine test/compcapture-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   SG_COMPOSITOR=... (default /usr/bin/sg-compositor; the border needs sg73), SG_DESKCOMP=...
#   SG_DXVK_DIR=<dir with DXVK's x64 dxgi.dll and d3d11.dll> to test DXVK's too
# Mutants (wine-sg): SG_MUTANT_CAPTURE_X_ONLY (sg_wlcapture.c, the X server's
# picture as before), SG_MUTANT_CAPTURE_RB_SWAPPED (sg_wlcapture.c),
# SG_MUTANT_CAPTURE_CACHE_FOREVER (sg_wlcapture.c, the picture never renewed),
# SG_MUTANT_WGC_UNSUPPORTED, SG_MUTANT_WGC_ALWAYS_NEW, SG_MUTANT_WGC_NO_CURSOR,
# SG_MUTANT_WGC_NO_BORDER (graphicscapture); DXVK's (sg-image dxvk/patches/0012,
# built with the define and given as SG_DXVK_DIR): SG_MUTANT_DXVKDUP_RB_SWAPPED,
# _NO_HELD_CHECK, _ALWAYS_NEW, _DIRTY_EMPTY, _FORMATS_IGNORED.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
COMP="${SG_COMPOSITOR:-/usr/bin/sg-compositor}"
DESKCOMP="${SG_DESKCOMP:-/usr/libexec/stained-glass/sg-deskcomp}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc Xwayland xterm xdotool python3 Xvfb grim convert; do
    command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x "$COMP" ] || { echo "SKIP: no compositor at $COMP"; exit 77; }

T=$(mktemp -d /var/tmp/sg-compcapture.XXXXXX)
CP="" XP=""
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$CP" ] && kill "$CP" 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O2 -o "$T/probe-$a.exe" "$HERE/compcapture-probe.c" \
        -ld3d11 -ldxgi -lruntimeobject -luuid -luser32 -lgdi32 || { echo "FAIL  the probe did not build ($a)"; exit 1; }
done
# the compositor's control socket is the gate's own (the capture border)
export SG_LOCK_CONTROL="$T/ctl.sock"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISPLAY="" WAYLAND_DISPLAY="" timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# every program on the shell's desktop (the session's), as in the session
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '0 128 128' /f >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c/cap"
mkdir -p "$C/dxvk"
cp "$T/probe-x86_64.exe" "$C/probe.exe"; cp "$T/probe-i686.exe" "$C/probe32.exe"
if [ -n "${SG_DXVK_DIR:-}" ]; then cp "$SG_DXVK_DIR/dxgi.dll" "$SG_DXVK_DIR/d3d11.dll" "$T/probe-x86_64.exe" "$C/dxvk/"; fi

# --- under the compositor ------------------------------------------------------
cat > "$T/inside.sh" <<EOF
#!/bin/sh
cd "$C"
"$WINE" explorer /desktop=shell,1280x720 > "$T/explorer.out" 2>&1 &
sleep 8
[ -x "$DESKCOMP" ] && { "$DESKCOMP" </dev/null > "$T/deskcomp.out" 2>&1 & sleep 3; }
"$WINE" probe.exe show </dev/null >/dev/null 2>"$T/show.err" &
xterm -geometry 20x8 -bg '#ff0000' -fg '#ff0000' -e sleep 300 &
sleep 7
# the Linux window's centre, as X has it (the Wine desktop is at 0,0 of the layout)
eval "\$(xdotool search --sync --onlyvisible --class xterm getwindowgeometry --shell 2>/dev/null | head -5)"
LX=\$((\${X:-0} + \${WIDTH:-0} / 2)); LY=\$((\${Y:-0} + \${HEIGHT:-0} / 2))
echo "linux \$LX \$LY" > "$T/out.txt"
run() { timeout -s KILL 90 "$WINE" "\$@" 2>>"$T/probe.err" </dev/null; }
for api in gdi dxgi wgcmon wgcwin wgcstatic wgccursor; do run probe.exe \$api nowin \$LX \$LY >> "$T/out.txt"; done
run probe.exe pixloop >> "$T/out.txt"
for api in gdi dxgi wgcmon; do run probe32.exe \$api nowin \$LX \$LY | sed 's/^/x32 /' >> "$T/out.txt"; done
if [ -f dxvk/probe-x86_64.exe ]; then
    ( cd dxvk && WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;dxgi,d3d11=n" timeout -s KILL 90 "$WINE" probe-x86_64.exe dxgi nowin \$LX \$LY 2>/dev/null </dev/null | sed 's/^dxgi/dxvk/' >> "$T/out.txt" )
fi
# the capture border: on the screen while a WGC session runs (the privileged
# capture sees the screen), never in a capture, gone after it
pix() { WAYLAND_DISPLAY="\$1" grim "$T/b.png" 2>/dev/null && convert "$T/b.png" -crop "1x1+\$2+\$3" txt:- | sed -n 's/.*\(#[0-9A-F]\{6\}\).*/\1/p' | head -1; }
run probe.exe wgchold nowin >/dev/null &
HOLD=\$!
sleep 4
echo "border on \$(pix "$T/priv.sock" 100 175) public \$(pix "\$WAYLAND_DISPLAY" 100 175)" >> "$T/out.txt"
wait \$HOLD
sleep 1
echo "border after \$(pix "$T/priv.sock" 100 175)" >> "$T/out.txt"
ctl() { python3 -c "import socket; s=socket.socket(socket.AF_UNIX); s.connect('$T/ctl.sock'); s.sendall(b'\$1\\\\n'); print(s.recv(64).decode().strip())"; }
echo "ctl \$(ctl LOCK)" >> "$T/out.txt"; sleep 2
for api in gdi dxgi wgcmon; do run probe.exe \$api nowin \$LX \$LY | sed 's/^/locked /' >> "$T/out.txt"; done
echo "ctl \$(ctl UNLOCK)" >> "$T/out.txt"; sleep 2
run probe.exe gdi nowin \$LX \$LY | sed 's/^/unlocked /' >> "$T/out.txt"
echo done > "$T/finished"
EOF
WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=1 \
    "$COMP" -L "$T/priv.sock" -C "$T/ctl.sock" -U "$(id -u)" -- sh -c "sh $T/inside.sh; sleep 2" > "$T/comp.log" 2>&1 &
CP=$!
i=0; while [ ! -f "$T/finished" ] && [ $i -lt 900 ]; do sleep 0.5; i=$((i + 1)); done
kill "$CP" 2>/dev/null; CP=""
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
sed 's/^/      /' "$T/out.txt"

PAT="q0=FF0000 q1=00FF00 q2=0000FF q3=FFFFFF"
BLACK="q0=000000 q1=000000 q2=000000 q3=000000"
read -r _ LX LY < "$T/out.txt" 2>/dev/null
line() { grep "^$1 " "$T/out.txt" | head -1; }
check() {   # PREFIX NAME: the pattern, and the Linux window red
    l=$(line "$1")
    case "$l" in *"$PAT"*) pass "$2: another process's window, read back";; *) fail "$2: ${l:-no answer}";; esac
    case "$l" in *"at$LX,$LY=FF0000"*) pass "$2: the Linux window too (the compositor's picture)";; *) fail "$2: no Linux window at $LX,$LY: $l";; esac
}
[ "${LX:-0}" -gt 0 ] && pass "a Linux window is on the screen at $LX,$LY" || fail "no Linux window found"
check gdi "GDI (screen DC)"
check dxgi "DXGI Desktop Duplication (Wine's dxgi)"
check "wgcmon size=1280x720 wgcmon" "Windows.Graphics.Capture, a monitor (1280x720)"
case "$(line "wgcwin size=400x300 wgcwin")" in *"$PAT"*) pass "Windows.Graphics.Capture, a window (400x300)";; *) fail "WGC window: $(line wgcwin)";; esac
case "$(line wgcstatic)" in "wgcstatic new=0"*) pass "WGC: no new frame while nothing changes";; *) fail "WGC: $(line wgcstatic)";; esac
case "$(line wgccursor)" in "wgccursor dark=0"*|"") fail "WGC: the pointer is not drawn: $(line wgccursor)";; *) pass "WGC: the pointer is drawn (IsCursorCaptureEnabled)";; esac
RULES="held=ok dirty=ok timeout=ok formats=ok"
case "$(line dxgirules)" in *"$RULES"*) pass "DXGI duplication's rules (a held frame, the dirty rectangle, a timeout while nothing changes, the formats list)";; *) fail "DXGI rules: $(line dxgirules)";; esac
pl=$(line pixloop)
ms=$(echo "$pl" | sed -n 's/^pixloop ms=\([0-9]*\).*/\1/p')
[ -n "$ms" ] && [ "$ms" -lt 5000 ] && pass "10000 GetPixel calls on the screen in $ms ms (a capture a frame, not a call)" || fail "GetPixel loop: ${pl:-no answer}"
case "$pl" in *"before=FF0000 after=00FF00"*) pass "...and a change on the screen is seen on the next read";; *) fail "GetPixel after a change: $pl";; esac
check "x32 gdi" "GDI, a 32-bit program"
check "x32 dxgi" "DXGI, a 32-bit program"
check "x32 wgcmon size=1280x720 wgcmon" "WGC, a 32-bit program"
if [ -n "${SG_DXVK_DIR:-}" ]; then
    check dxvk "DXGI Desktop Duplication (DXVK's dxgi)"
    case "$(line dxvkrules)" in *"$RULES"*) pass "DXVK's duplication follows the same rules";; *) fail "DXVK rules: $(line dxvkrules)";; esac
fi
case "$(line "border on")" in "border on #FFD800 public #FF0000"*) pass "WGC: the yellow border on the screen round the captured window, not in a capture";; *) fail "WGC border: $(line "border on")";; esac
case "$(line "border after")" in "border after #FF0000"*) pass "WGC: the border goes with the capture";; *) fail "WGC border after: $(line "border after")";; esac
grep -q '^ctl OK locked' "$T/out.txt" || fail "the session did not lock: $(grep '^ctl' "$T/out.txt" | head -1)"
for api in gdi dxgi "wgcmon size=1280x720 wgcmon"; do
    case "$(line "locked $api")" in *"$BLACK"*) pass "locked: ${api%% *} reads black (the compositor's rule for capture)";; *) fail "locked: ${api%% *}: $(line "locked $api")";; esac
done
case "$(line "unlocked gdi")" in *"$PAT"*) pass "unlocked: the picture again";; *) fail "after unlocking: $(line "unlocked gdi")";; esac

# --- a plain X server: the X server's picture, as before --------------------------
DPY=278
while [ -e "/tmp/.X${DPY}-lock" ] && [ "$DPY" -lt 279 ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
sleep 2
(
    export DISPLAY=":$DPY"; unset WAYLAND_DISPLAY
    cd "$C" || exit 1
    "$WINE" explorer /desktop=shell,1280x800 >/dev/null 2>&1 &
    sleep 6
    timeout -s KILL 90 "$WINE" probe.exe gdi 2>/dev/null </dev/null
) > "$T/xvfb.txt"
case "$(cat "$T/xvfb.txt")" in *"$PAT"*) pass "without a compositor, GDI reads the X server as before";; *) fail "plain X: $(cat "$T/xvfb.txt")";; esac

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
