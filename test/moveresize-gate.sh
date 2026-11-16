#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A framed Linux program moved by its own title bar (_NET_WM_MOVERESIZE) never
# leaves Wine's move loop running (patches/sg/7790; David 2026-11-15: Steam
# "got stuck in drag mode", followed the pointer, pulled the desktop back to
# itself, and after Alt+F4 the Start button took no clicks until Steam quit).
# In a shell desktop on Xvfb, a stand-in program (test/moveresize-client.c)
# framed by the taskbar (a stand-in sg-lockctl), for each way of asking:
#   gtk      it lets go of the pointer and asks: the frame moves with the drag,
#            and the loop ends at the release
#   keep     it keeps its press's grab (the frame cannot take the pointer): the
#            loop ends at the release all the same, and the frame stays put
#            when the pointer moves on
#   late     it asks only after the release (a tap): no loop
#   die      it exits mid-move: the loop ends
#   hide     Alt+F4 mid-move, and it only hides its window (as Steam does,
#            living on): the loop ends
# after each: no move loop and no capture left on the frame's thread; and at
# the end the Start button opens on one click.
#
#   WINE=/opt/wine-sg/bin/wine test/moveresize-gate.sh
#   mutants: SG_MUTANT_MOVESIZE_NO_RELEASE_WATCH (winex11 sg_embed.c),
#            SG_MUTANT_MOVESIZE_KEEPS_GOING (win32u defwnd.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DISP="${MOVERESIZE_DISPLAY:-:267}"
unset SESSION_MANAGER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb xdotool xwininfo cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -f /usr/include/X11/Xlib.h ] || { echo "SKIP: needs libx11-dev"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-moveresize.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
PIDS=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    [ -n "${XVFB:-}" ] && kill "$XVFB" 2>/dev/null
    rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/client" "$HERE/moveresize-client.c" -lX11 || { echo "FAIL  client did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/mrp.exe" "$HERE/moveresize-probe.c" || { echo "FAIL  probe did not build"; exit 1; }

Xvfb "$DISP" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
export DISPLAY="$DISP"
for i in $(seq 50); do xdotool getmouselocation >/dev/null 2>&1 && break; sleep 0.1; done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mrp.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

printf 'END\n' > "$T/list"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
"$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
PIDS="$PIDS $!"
sleep 6
mrp() { (cd "$WINEPREFIX/drive_c" && "$WINE" mrp.exe "$@" 2>/dev/null | tr -d '\r'); }
framed() {
    _k=0
    while [ $_k -lt 120 ]; do
        case "$(xwininfo -id "$1" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')" in
            ""|"(the root window)"*) ;; *) return 0 ;; esac
        sleep 0.25; _k=$((_k + 1))
    done
    return 1
}
rect() { mrp frame "$1" | sed -n 's/^rect=\([^ ]*\).*/\1/p'; }
idle() { mrp loop "$1" | grep -qx 'movesize=0 capture=0'; }

# client MODE [hide]: a new stand-in, framed; CL its log, NAME its title
client() {
    NAME="Mover-$1"; CL="$T/$1.log"; : > "$CL"
    "$T/client" "$CL" "$NAME" "$@" &
    CPID=$!; PIDS="$PIDS $CPID"
    for i in $(seq 50); do grep -q READY "$CL" && break; sleep 0.1; done
    XC=$(sed -n 's/^READY //p' "$CL")
    printf '%d shown - Mover\t%s\nEND\n' "$XC" "$NAME" > "$T/list"
    framed "$XC" || { fail "$NAME was not framed"; return 1; }
    sleep 1.5
    set -- $(rect "$NAME" | tr , ' '); PX=$(( $1 + 120 )); PY=$(( $2 + 100 ))
}
press_move() {
    xdotool mousemove "$PX" "$PY"; sleep 0.4
    xdotool mousedown 1; sleep 0.4
    for s in 1 2 3 4 5; do xdotool mousemove $(( PX + s * 20 )) $(( PY + s * 10 )); sleep 0.15; done
    sleep 0.4
}
release() { xdotool mouseup 1; sleep 1.2; }
away() { xdotool mousemove 900 80; sleep 1; }

# gtk: moves, then the loop ends
client gtk && {
    R0=$(rect "$NAME"); press_move; release; R1=$(rect "$NAME"); away; R2=$(rect "$NAME")
    if idle "$NAME" && [ "$R1" != "$R0" ] && [ "$R2" = "$R1" ]; then
        pass "gtk: the frame moves with the drag ($R0 -> $R1) and stays put after the release"
    else fail "gtk: $R0 -> $R1 -> $R2, $(mrp loop "$NAME")"; fi
    kill "$CPID" 2>/dev/null; sleep 1.5
}

# keep: the program keeps its grab; the loop ends at the release anyway
client keep && {
    press_move; release; R1=$(rect "$NAME"); away; R2=$(rect "$NAME")
    if idle "$NAME" && [ "$R2" = "$R1" ]; then
        pass "keep: the loop ends at the release though the program kept the pointer ($R1 stays $R2)"
    else fail "keep: the frame went on following the pointer: $R1 -> $R2, $(mrp loop "$NAME")"; fi
    kill "$CPID" 2>/dev/null; sleep 1.5
}

# late: asked after the release (a tap): nothing starts
client late && {
    R0=$(rect "$NAME"); xdotool mousemove "$PX" "$PY" click 1; sleep 1.2; away; R2=$(rect "$NAME")
    if idle "$NAME" && [ "$R2" = "$R0" ] && grep -q ASKED "$CL"; then
        pass "late: a move asked after the release starts no loop"
    else fail "late: $R0 -> $R2, $(mrp loop "$NAME"), $(tr '\n' ' ' < "$CL")"; fi
    kill "$CPID" 2>/dev/null; sleep 1.5
}

# die: the program exits mid-move
client keep && {
    press_move; kill -9 "$CPID" 2>/dev/null; sleep 1.5
    LOOP=$(mrp loop Mover-keep); release
    if idle Mover-keep || [ "$(mrp frame Mover-keep)" = "rect=none" ]; then
        pass "die: the program gone mid-move, the loop ends (during: $LOOP)"
    else fail "die: $(mrp loop Mover-keep) after its program died"; fi
    sleep 1.5
}

# hide: Alt+F4 mid-move, the program only hides (and lives on)
client keep hide && {
    press_move; xdotool key alt+F4; sleep 1.5
    HID=$(grep -c HIDDEN "$CL"); LOOP=$(mrp loop "$NAME"); R1=$(rect "$NAME")
    # the button still down: the hidden frame no longer follows the pointer
    xdotool mousemove $(( PX + 200 )) $(( PY + 150 )); sleep 1; R2=$(rect "$NAME")
    release; away
    if [ "$HID" -ge 1 ] && echo "$LOOP" | grep -q 'movesize=0' && [ "$R1" = "$R2" ]; then
        pass "hide: Alt+F4 mid-move hides it and ends the loop at once (its program lives on)"
    else fail "hide: hidden $HID, during: $LOOP, frame $R1 -> $R2 with the button down, after: $(mrp loop "$NAME")"; fi
}

# and the Start button opens on one click
xdotool mousemove 22 685; sleep 0.5; xdotool click 1; sleep 1.5
if mrp start | grep -qx 'start=1'; then pass "the Start button opens on one click"
else fail "the Start button did not open ($(mrp start), $(mrp loop "$NAME"))"; fi
xdotool key Escape

[ "$RC" = 0 ] && echo "moveresize: all checks passed"
exit $RC
