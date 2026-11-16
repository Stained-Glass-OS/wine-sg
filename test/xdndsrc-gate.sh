#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Wine as an XDND source (patches/sg/7787): an OLE drag from a Wine program over
# another X program's window (a Linux program: David 2026-11-15, File Explorer
# into Kdenlive) speaks XDND to it, in a virtual desktop on Xvfb:
#
#   files to X    a CF_HDROP drag over a native XDND window lying over a Wine
#                 drop target gives the native program XdndEnter with
#                 text/uri-list, XdndPosition with XdndActionCopy, XdndDrop, and
#                 the file:// URIs of a Z:\ and a C:\ file (a space, an umlaut;
#                 "/" not escaped, links resolved) when it converts XdndSelection;
#                 DoDragDrop reports the copy;
#                 the Wine window beneath never hears of the drag
#   in a frame    the same into a native window inside a Wine window's X window
#                 (a Linux program in the taskbar's frame)
#   Esc           cancels: the native program gets XdndLeave, no drop,
#                 DoDragDrop says DRAGDROP_S_CANCEL
#   move          a program finishing with "move" on files still in place is
#                 reported as a copy (File Explorer would delete them); one that
#                 converted DELETE gets the move
#   refuse        a program that does not accept: XdndLeave, effect none
#   text          CF_UNICODETEXT arrives as text/plain;charset=utf-8, UTF-8
#   cursor        over the native window the pointer shows the drag's cursor (copy,
#                 no drop), not the window's own: X shows the cursor of the press's
#                 automatic grab, Wine's window, outside it
#   Wine to Wine  a drag onto another Wine process's window goes through OLE
#                 (a private format comes along), also after passing over the
#                 native window, which then got XdndLeave
#
#   WINE=/opt/wine-sg/bin/wine test/xdndsrc-gate.sh
#   mutants: SG_MUTANT_DRAG_NO_XDND (ole32), SG_MUTANT_DRAG_NO_WINE_MARK,
#            SG_MUTANT_DRAG_NO_LEAVE, SG_MUTANT_DRAG_MOVE_UNCHECKED (winex11),
#            SG_MUTANT_URI_SLASH_ESCAPED (winex11 clipboard.c, 7789)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DISP="${XDNDSRC_DISPLAY:-:263}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -f /usr/include/X11/extensions/Xfixes.h ] || { echo "SKIP: needs libxfixes-dev"; exit 77; }
for t in Xvfb xdotool gcc python3; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -f /usr/include/X11/Xlib.h ] || { echo "SKIP: needs libx11-dev"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-xdndsrc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${WINEDEBUG:--all}" WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
PIDS=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    for p in $PIDS; do kill "$p" 2>/dev/null; done
    [ -n "${XVFB:-}" ] && kill "$XVFB" 2>/dev/null
    [ -n "${KEEP:-}" ] && { echo "kept $T"; return; }
    rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

"$MINGW" -municode -mwindows -O2 -o "$T/xdndsrc-probe.exe" "$HERE/xdndsrc-probe.c" -lole32 -luuid -lshell32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
gcc -O2 -o "$T/xdndsrc-target" "$HERE/xdndsrc-target.c" -lX11 || { echo "FAIL  target did not build"; exit 1; }
gcc -O2 -o "$T/cursor" "$HERE/xdndsrc-cursor.c" -lX11 -lXfixes || { echo "FAIL  cursor tool did not build"; exit 1; }

Xvfb "$DISP" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
export DISPLAY="$DISP"
for i in $(seq 50); do xdotool getmouselocation >/dev/null 2>&1 && break; sleep 0.1; done

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/xdndsrc-probe.exe" "$WINEPREFIX/drive_c/"

# the files: one on Z: (the Unix tree), one on C:
mkdir -p "$T/z dir"
printf 'z\n' > "$T/z dir/über z.txt"
mkdir -p "$WINEPREFIX/drive_c/users/Public"
printf 'c\n' > "$WINEPREFIX/drive_c/users/Public/c file.txt"
ZWIN="Z:$(printf '%s' "$T/z dir/über z.txt" | tr '/' '\\')"
CWIN='C:\users\Public\c file.txt'
ZREAL=$(readlink -f "$T/z dir/über z.txt")
CREAL=$(readlink -f "$WINEPREFIX/drive_c/users/Public/c file.txt")

D="C:\\"
ALOG="$WINEPREFIX/drive_c/a.log"; TLOG="$WINEPREFIX/drive_c/t.log"; BLOG="$WINEPREFIX/drive_c/b.log"
: > "$ALOG"; : > "$TLOG"; : > "$BLOG"
N1="$T/n1.log"; N2="$T/n2.log"
: > "$N1"; : > "$N2"
echo copy > "$N1.mode"; echo copy > "$N2.mode"

DESK="/desktop=xdndsrc,1024x768"
XDNDSRC_LOG='C:\b.log' "$WINE" explorer "$DESK" 'C:\xdndsrc-probe.exe' target 400 20 500 600 >/dev/null 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 150); do grep -q READY "$BLOG" 2>/dev/null && break; sleep 0.2; done
XDNDSRC_LOG='C:\a.log' "$WINE" explorer "$DESK" 'C:\xdndsrc-probe.exe' source 20 20 files "$ZWIN" "$CWIN" >/dev/null 2>&1 &
PIDS="$PIDS $!"
XDNDSRC_LOG='C:\t.log' "$WINE" explorer "$DESK" 'C:\xdndsrc-probe.exe' source 20 260 text 'Grüße ✓ SG' >/dev/null 2>&1 &
PIDS="$PIDS $!"
for i in $(seq 150); do grep -q READY "$ALOG" 2>/dev/null && grep -q READY "$TLOG" 2>/dev/null && break; sleep 0.2; done
grep -q "REGISTER hr=0x0" "$BLOG" && grep -q READY "$ALOG" && grep -q READY "$TLOG" \
    || { fail "the Wine windows did not come up"; cat "$ALOG" "$BLOG" "$TLOG"; exit 1; }
sleep 1

# the native programs: one over the Wine target, one inside its X window
"$T/xdndsrc-target" "$N1" 450 100 200 150 &
PIDS="$PIDS $!"
"$T/xdndsrc-target" "$N2" 50 350 200 150 sg-xdnd-wine-target &
PIDS="$PIDS $!"
for i in $(seq 100); do grep -q READY "$N1" && grep -q READY "$N2" && break; sleep 0.1; done
grep -q READY "$N1" && grep -q READY "$N2" || { fail "native targets did not come up: $(cat "$N1" "$N2")"; exit 1; }
sleep 0.5

SRCWIN=$(xdotool search --name '^sg-xdnd-source$' | head -1)
[ -n "$SRCWIN" ] || { fail "no X window for the source"; exit 1; }
result_count() { grep -c '^RESULT' "$1"; }
# drag LOG SX SY EX EY [esc|via X Y]
drag() {
    log=$1; sx=$2; sy=$3; ex=$4; ey=$5; shift 5
    before=$(result_count "$log")
    xdotool mousemove "$sx" "$sy"; sleep 0.3
    xdotool mousedown 1; sleep 0.4
    xdotool mousemove $(( sx + 10 )) $(( sy + 10 )); sleep 0.3
    if [ "${1:-}" = via ]; then
        xdotool mousemove "$2" "$3"; sleep 0.4
        xdotool mousemove $(( $2 + 4 )) $(( $3 + 3 )); sleep 0.6
    fi
    xdotool mousemove "$ex" "$ey"; sleep 0.4
    xdotool mousemove $(( ex + 3 )) $(( ey + 2 )); sleep 0.6
    case "${1:-}" in sample*) SAMPLE=$("$T/cursor") ;; esac
    if [ "${1:-}" = esc ] || [ "${1:-}" = sampleesc ]; then
        # the keyboard goes to the X focus: the source's window (with no window
        # manager it might still be PointerRoot, the native window under the pointer)
        xdotool windowfocus "$SRCWIN"; sleep 0.3
        xdotool key Escape; sleep 0.5
    fi
    xdotool mouseup 1
    for i in $(seq 75); do [ "$(result_count "$log")" -gt "$before" ] && break; sleep 0.2; done
    sleep 0.5
    LAST=$(grep '^RESULT' "$log" | tail -1)
}
count() { grep -c "^$2" "$1"; }

# the paths of the uri list as a GLib program reads them (g_filename_from_uri
# refuses an escaped "/"): each a resolved path, no symbolic link (C: is one)
uris() {
    python3 -I -c '
import sys, os, urllib.parse
for line in open(sys.argv[1], "rb").read().decode().split("\r\n"):
    if line.startswith("file:///"):
        if "%2f" in line.lower():
            print("ESCAPED-SLASH")
        print(urllib.parse.unquote(line[7:]))
' "$1"
}

# 1. Wine to Wine (another process): through OLE
drag "$ALOG" 120 120 800 150
if grep -qxF "WINEDROP ole=1 files=$ZWIN|$CWIN" "$BLOG" && echo "$LAST" | grep -q 'effect=1$' && [ "$(count "$N1" ENTER)" = 0 ]; then
    pass "Wine to Wine: OLE drop with the private format, both files, copy ($LAST)"
else fail "Wine to Wine: $LAST / $(tail -2 "$BLOG") / n1 enters $(count "$N1" ENTER)"; fi
B_ENTERS=$(count "$BLOG" WINEENTER)

# 2. files onto the native window over the Wine target
drag "$ALOG" 120 120 550 175
if [ "$(count "$N1" ENTER)" = 1 ] && grep -q '^TYPE text/uri-list$' "$N1" && grep -q '^ACTION XdndActionCopy$' "$N1" \
   && grep -q '^DATA text/uri-list$' "$N1" && grep -q '^FINISHED XdndActionCopy$' "$N1"; then
    pass "native: XdndEnter text/uri-list, XdndPosition copy, XdndDrop, data, XdndFinished"
else fail "native protocol: $(tr '\n' ' ' < "$N1")"; fi
GOT=$(uris "$N1.data" | tr '\n' '|')
if [ "$GOT" = "$ZREAL|$CREAL|" ]; then pass "native: file:// URIs of the Z: and C: files ($GOT)"
else fail "native URIs: got '$GOT' want '$ZREAL|$CREAL|' (raw: $(cat "$N1.data" 2>/dev/null))"; fi
if echo "$LAST" | grep -q 'hr=0x40100 effect=1$'; then pass "native: DoDragDrop says drop, copy ($LAST)"
else fail "native: DoDragDrop $LAST"; fi
if [ "$(count "$BLOG" WINEENTER)" = "$B_ENTERS" ] && [ "$(count "$BLOG" WINEDROP)" = 1 ]; then
    pass "native: the Wine window beneath never heard of the drag"
else fail "native: the Wine window beneath got $(tail -3 "$BLOG" | tr '\n' ' ')"; fi

# 3. into the native window inside the Wine window's X window
drag "$ALOG" 120 120 550 440
GOT=$(uris "$N2.data" 2>/dev/null | tr '\n' '|')
if grep -q '^DROP$' "$N2" && [ "$GOT" = "$ZREAL|$CREAL|" ] && echo "$LAST" | grep -q 'effect=1$' \
   && [ "$(count "$BLOG" WINEENTER)" = "$B_ENTERS" ]; then
    pass "framed native: drop with both URIs, copy, the frame's Wine window untouched"
else fail "framed native: $(tr '\n' ' ' < "$N2") / $GOT / $LAST / b enters $(count "$BLOG" WINEENTER)"; fi

# 4. Esc cancels
L1=$(count "$N1" LEAVE); D1=$(count "$N1" DROP)
drag "$ALOG" 120 120 550 175 esc
if [ "$(count "$N1" LEAVE)" -gt "$L1" ] && [ "$(count "$N1" DROP)" = "$D1" ] && echo "$LAST" | grep -q 'hr=0x40101 effect=0$'; then
    pass "Esc: XdndLeave, no drop, DRAGDROP_S_CANCEL ($LAST)"
else fail "Esc: leaves $L1->$(count "$N1" LEAVE) drops $D1->$(count "$N1" DROP) $LAST"; fi

# 5. move on files still in place: a copy
echo move > "$N1.mode"
drag "$ALOG" 120 120 550 175
if grep -q '^FINISHED XdndActionMove$' "$N1" && echo "$LAST" | grep -q 'effect=1$' && [ -f "$ZREAL" ] && [ -f "$CREAL" ]; then
    pass "move on files still there: reported as a copy ($LAST)"
else fail "move unchecked: $LAST"; fi

# 6. move after DELETE: a move
echo movedel > "$N1.mode"
drag "$ALOG" 120 120 550 175
if grep -q '^DELETE$' "$N1" && echo "$LAST" | grep -q 'effect=2$'; then
    pass "move with DELETE asked: a move ($LAST)"
else fail "move with DELETE: $LAST / $(tail -3 "$N1" | tr '\n' ' ')"; fi

# 7. refused
echo refuse > "$N1.mode"
L1=$(count "$N1" LEAVE); D1=$(count "$N1" DROP)
drag "$ALOG" 120 120 550 175
if [ "$(count "$N1" LEAVE)" -gt "$L1" ] && [ "$(count "$N1" DROP)" = "$D1" ] && echo "$LAST" | grep -q 'effect=0$'; then
    pass "refused: XdndLeave, no drop, effect none ($LAST)"
else fail "refused: $LAST leaves $L1->$(count "$N1" LEAVE)"; fi

# 8. text
echo copy > "$N1.mode"
drag "$TLOG" 120 360 550 175
if grep -q '^TYPE text/plain;charset=utf-8$' "$N1" && grep -q '^DATA text/plain;charset=utf-8$' "$N1" \
   && [ "$(cat "$N1.data")" = 'Grüße ✓ SG' ] && echo "$LAST" | grep -q 'effect=1$'; then
    pass "text: UTF-8 text/plain ($(cat "$N1.data"))"
else fail "text: '$(cat "$N1.data" 2>/dev/null)' $LAST"; fi

# 9. over the native window, then back to Wine
L1=$(count "$N1" LEAVE); D1=$(count "$N1" DROP)
drag "$ALOG" 120 120 800 150 via 550 175
if [ "$(count "$BLOG" WINEDROP)" = 2 ] && tail -1 "$BLOG" | grep -q '^WINEDROP ole=1 ' && [ "$(count "$N1" LEAVE)" -gt "$L1" ] \
   && [ "$(count "$N1" DROP)" = "$D1" ] && echo "$LAST" | grep -q 'effect=1$'; then
    pass "native then Wine: XdndLeave, OLE drop on the Wine window ($LAST)"
else fail "native then Wine: $LAST / $(tail -2 "$BLOG" | tr '\n' ' ') / leaves $L1->$(count "$N1" LEAVE)"; fi

# 10. the drag's cursor over the native window is the drag's own, as Wine
# shows it over its windows: the copy cursor where the program accepts (as over
# the Wine drop target), the no-drop one where it refuses (as over a Wine
# window that takes no drops) -- not the native window's cursor or a stale one
xdotool mousemove 550 175; sleep 0.5; OWN=$("$T/cursor")
drag "$ALOG" 120 120 800 150 sampleesc; WCOPY=$SAMPLE
drag "$ALOG" 120 120 120 400 sampleesc; WNONE=$SAMPLE
echo copy > "$N1.mode"
drag "$ALOG" 120 120 550 175 sample; ACC=$SAMPLE
echo refuse > "$N1.mode"
drag "$ALOG" 120 120 550 175 sample; REF=$SAMPLE
echo copy > "$N1.mode"
if [ "$ACC" = "$WCOPY" ] && [ "$REF" = "$WNONE" ] && [ "$ACC" != "$OWN" ] && [ "$WCOPY" != "$WNONE" ]; then
    pass "the drag's cursor over the native window: copy where it accepts ($ACC), no drop where it refuses ($REF)"
else fail "drag cursor over the native window: accepting $ACC (want $WCOPY), refusing $REF (want $WNONE), its own $OWN"; fi

[ "$RC" = 0 ] && echo "xdndsrc: all checks passed"
exit $RC
