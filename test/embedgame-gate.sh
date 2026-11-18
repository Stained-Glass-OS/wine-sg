#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A game started by Linux Steam under Proton is shown as Windows shows a full-screen game: its
# frame the whole monitor, no title bar, the game's window the monitor's size (David
# 2026-11-17: Skyrim, Proton 8.0, full screen 1920x1080, opened "really small" in its
# SgLinuxWindow frame until Win+Up). A stand-in (embedgame-app.c: an X top-level with no
# decorations, as winex11 makes a game's window) framed by the taskbar goes full screen each
# way Wine and Proton do:
#   fsstate    mapped small, then sized to the screen and _NET_WM_STATE_FULLSCREEN asked
#   stateonly  mapped small, the state asked first, sized to the screen after
#   resize     mapped small, then only XMoveResizeWindow to the whole screen
#   born       created already the screen's size at 0,0, no full-screen state
#   early      sized to the screen and the state asked at once, before it is framed
#   earlystate only the state asked at once, before it is framed, the sizing left to the window
#              manager (under sg-compositor only: its window manager acts on it; Xvfb has none)
# and back: sized to the screen (as resize), then back to 800x600 -- windowed again, its frame
# round it, not the monitor's, and the game's window 800x600.
# Checked each time: the frame covers the monitor, has no title bar, and the game's window is
# the monitor's size (the last size it was given, and its X geometry).
# Under Xvfb, or with SG_COMPOSITOR_BUILD under a headless sg-compositor, whose X window manager
# takes the game's requests before the shell frames it.
#   WINE=/opt/wine-sg/bin/wine [SG_COMPOSITOR_BUILD=<dir with sg-compositor>] test/embedgame-gate.sh
#   (mutant SG_MUTANT_EMBED_NO_LEGACY_FS, dlls/winex11.drv/sg_embed.c: resize and back fail)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
COMP="${SG_COMPOSITOR_BUILD:-}/sg-compositor"
unset SESSION_MANAGER
for t in xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$COMP" ] || command -v Xvfb >/dev/null || { echo "SKIP: neither sg-compositor nor Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-embedgame.XXXXXX); chmod 700 "$T"; mkdir -m 700 "$T/cx" "$T/run"
PIDS=; AP=
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$AP" ] && kill "$AP" 2>/dev/null
    for p in $PIDS; do kill "$p" 2>/dev/null; done; sleep 1
    for p in $PIDS; do kill -KILL "$p" 2>/dev/null; done   # the compositor waits on its child otherwise
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/app" "$HERE/embedgame-app.c" -lX11 || { echo "FAIL  embedgame-app did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/linuxembed-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
if [ -x "$COMP" ]; then
    # the session's compositor (headless), its X window manager taking the game's requests first
    XDG_RUNTIME_DIR="$T/cx" WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman \
        "$COMP" -L "$T/p.sock" -C "$T/c.sock" -U "$(id -u)" -- sh -c "echo \$DISPLAY > $T/disp; exec sleep 900" > "$T/comp.log" 2>&1 &
    PIDS="$PIDS $!"
    i=0; while [ ! -s "$T/disp" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
    DISPLAY=$(cat "$T/disp" 2>/dev/null)
    WHERE="sg-compositor"
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & PIDS="$PIDS $!"
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    DISPLAY=":$(cat "$T/display")"
    WHERE="Xvfb"
fi
export DISPLAY
[ -n "$DISPLAY" ] && [ "$DISPLAY" != ":" ] && [ "$DISPLAY" != ":0" ] || { echo "FAIL  no display"; exit 1; }
sleep 1
SW=$(xwininfo -root | awk '/Width:/ {print $2}'); SH=$(xwininfo -root | awk '/Height:/ {print $2}')
echo "      under $WHERE, the screen ${SW}x${SH}"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winewayland.drv=d" WINESERVER
export WAYLAND_DISPLAY=sg-no-wayland
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d x11 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d "${SW}x${SH}" /f >/dev/null 2>&1
"$WINESERVER" -w
printf 'END\n' > "$T/list"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
WINEDEBUG="${EMBED_DEBUG:--all}" "$WINE" explorer "/desktop=shell,${SW}x${SH}" > "$T/explorer.out" 2>&1 & PIDS="$PIDS $!"
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 4
cd "$WINEPREFIX/drive_c"
probe() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }

MODES="fsstate stateonly resize born early"
[ "$WHERE" = sg-compositor ] && MODES="$MODES earlystate"
for MODE in ${EMBEDGAME_MODES:-$MODES}; do
    L="$T/app-$MODE.log"; : > "$L"
    "$T/app" "$MODE" "$L" & AP=$!
    i=0; while ! grep -q '^main ' "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    XID=$(awk '$1 == "main" {print $2; exit}' "$L")
    printf '%d shown - Game\tGame\nEND\n' "$XID" > "$T/list"
    sleep 9   # framed (the list is read each second), then full screen
    r=$(probe rect SgLinuxWindow Game)
    st=$(probe style SgLinuxWindow Game)
    last=$(awk '$1 == "size" {s = $2 " " $3} END {print s}' "$L")
    geo=$(xwininfo -id "$XID" 2>/dev/null | awk '/Width:/ {w = $2} /Height:/ {h = $2} END {print w " " h}')
    [ "$r" = "0 0 $SW $SH" ] && pass "$MODE: its frame covers the monitor ($r)" || fail "$MODE: its frame is $r, not the monitor (0 0 $SW $SH)"
    case "$st" in *caption=0*) pass "$MODE: no title bar ($st)";; *) fail "$MODE: its frame has a title bar ($st)";; esac
    [ "$last" = "$SW $SH" ] && [ "$geo" = "$SW $SH" ] && pass "$MODE: the game's window is the monitor's size" \
        || fail "$MODE: the game's window: last size given '$last', now '$geo' (sizes: $(awk '$1 == "size" {printf "%s %sx%s; ", $1, $2, $3}' "$L"))"
    kill "$AP" 2>/dev/null; wait "$AP" 2>/dev/null; AP=
    printf 'END\n' > "$T/list"
    sleep 3
done
L="$T/app-back.log"; : > "$L"
"$T/app" back "$L" & AP=$!
i=0; while ! grep -q '^main ' "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
XID=$(awk '$1 == "main" {print $2; exit}' "$L")
printf '%d shown - Game\tGame\nEND\n' "$XID" > "$T/list"
i=0; while ! grep -q "^size $SW $SH" "$L" && [ $i -lt 150 ]; do sleep 0.1; i=$((i + 1)); done
sleep 1
r=$(probe rect SgLinuxWindow Game)
grep -q '^windowed' "$L" && r="$r (too late: windowed already)"
[ "$r" = "0 0 $SW $SH" ] && pass "back: full screen first ($r)" || fail "back: before going windowed its frame is $r"
i=0; while ! grep -q '^windowed' "$L" && [ $i -lt 150 ]; do sleep 0.1; i=$((i + 1)); done
sleep 4
r=$(probe rect SgLinuxWindow Game)
geo=$(xwininfo -id "$XID" 2>/dev/null | awk '/Width:/ {w = $2} /Height:/ {h = $2} END {print w " " h}')
last=$(awk '$1 == "size" {s = $2 " " $3} END {print s}' "$L")
[ "$r" != "0 0 $SW $SH" ] && [ -n "$r" ] && pass "back: windowed again, its frame no longer the monitor ($r)" || fail "back: its frame after going windowed: $r"
[ "$geo" = "800 600" ] && [ "$last" = "800 600" ] && pass "back: the game's window keeps the 800x600 it asked for" \
    || fail "back: the game's window is '$geo' (last size given '$last'; sizes: $(awk '$1 == "size" {printf "%sx%s; ", $2, $3}' "$L"))"
kill "$AP" 2>/dev/null; wait "$AP" 2>/dev/null; AP=
printf 'END\n' > "$T/list"
sleep 2
"$WINE" probe.exe exists Shell_TrayWnd "" | grep -q exists=1 && pass "the shell is still running" || fail "the shell ended"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
