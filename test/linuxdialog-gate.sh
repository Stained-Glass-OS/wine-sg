#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program's dialog is its window's owned window (patches/sg/9515), as a Windows dialog is
# its program window's. Its frame and its window's are the shell's (sg_embed.c); a dialog
# (WM_TRANSIENT_FOR: an Open or Save dialog, Preferences, About) was framed as a window of its own:
# a taskbar button of its own, and behind its window at that window's first click -- a modal
# dialog out of sight, its program looking hung. A real X program, its window and its dialog:
#   - the dialog's frame is owned by its window's frame; one taskbar button, the window's;
#   - its window brought forward, the dialog stays above it;
#   - its window minimized, the dialog goes with it, and comes back with it.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxdialog-gate.sh
#   mutant: SG_MUTANT_EMBED_DIALOG_UNOWNED (programs/explorer/systray.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb xwininfo xdotool import cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxdialog.XXXXXX); XP=; XC=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; [ -n "$XC" ] && kill "$XC" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
cc -O2 -o "$T/xclient" "$HERE/linuxdialog-xclient.c" -lX11 || { echo "FAIL  X program did not build"; exit 1; }
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
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
printf 'END\n' > "$T/list"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! xwininfo -root -children 2>/dev/null | grep -q '"shell - Wine Desktop"' && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
"$T/xclient" & XC=$!
xid() { xwininfo -root -tree 2>/dev/null | awk -v n="\"$1\"" 'index($0, n) { print $1; exit }'; }
i=0; while [ -z "$(xid 'Gate Main')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
MAIN=$(xid 'Gate Main')
TAB=$(printf '\t')
printf '%d shown - GateDialogApp%sGate Main\nEND\n' "$MAIN" "$TAB" > "$T/list"
sleep 4    # the window framed before its dialog comes, as a program's Open dialog comes later
i=0; while [ -z "$(xid 'Gate Dialog')" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
DLG=$(xid 'Gate Dialog')
printf '%d shown - GateDialogApp%sGate Main\n%d shown focused GateDialogApp%sGate Dialog\nEND\n' "$MAIN" "$TAB" "$DLG" "$TAB" > "$T/list"
sleep 5
cd "$WINEPREFIX/drive_c"
P() { "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'; }
framed() { case "$(xwininfo -id "$1" -tree 2>/dev/null | sed -n 's/.*Parent window id: [^ ]* //p')" in ""|"(the root window)"*) return 1 ;; esac; return 0; }
framed "$MAIN" && framed "$DLG" && pass "the window and its dialog are framed" || fail "framed: main $(framed "$MAIN" && echo yes || echo no), dialog $(framed "$DLG" && echo yes || echo no)"
b1=$("$WINE" fglock-probe.exe button 'Gate Main' 2>/dev/null | tr -d '\r')
b2=$("$WINE" fglock-probe.exe button 'Gate Dialog' 2>/dev/null | tr -d '\r')
[ -n "$b1" ] && [ "$b1" != none ] && [ "$b2" = none ] && pass "one taskbar button, the window's (the dialog has none of its own)" || fail "buttons: main '$b1' dialog '$b2'"
# the person clicks the window (its frame's title bar), as when they go back to it
R=$(P rect SgLinuxWindow 'Gate Main')
set -- $R
xdotool mousemove $(( ($1 + $3) / 2 )) $(( $2 + 10 )) click 1 2>/dev/null; sleep 1.5
fg=$(P foreground SgLinuxWindow 'Gate Main'); fgd=$(P foreground SgLinuxWindow 'Gate Dialog')
# as drawn: the middle of the dialog, where it overlaps its window -- the dialog's colour (a03060),
# not the window's (3060a0)
D=$(P rect SgLinuxWindow 'Gate Dialog'); set -- $D
px=$(import -window root -crop 1x1+$(( ($1 + $3) / 2 ))+$(( ($2 + $4) / 2 + 10 )) -depth 8 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-Fa-f]\{6\}')
case "$(P zabove SgLinuxWindow 'Gate Dialog' SgLinuxWindow 'Gate Main') $px" in
    "above=1 #A03060"|"above=1 #a03060") pass "its window clicked, the dialog stays above it (window $fg, drawn $px)" ;;
    *) fail "its window clicked, the dialog went behind it (window $fg, dialog $fgd, $(P zabove SgLinuxWindow 'Gate Dialog' SgLinuxWindow 'Gate Main'), drawn $px)" ;; esac
# minimized as the person does (its minimize button, its taskbar button: SC_MINIMIZE)
P post SgLinuxWindow 'Gate Main' 0x0112 0xF020 >/dev/null; sleep 1.5
case "$(P list SgLinuxWindow -)" in *"'Gate Dialog' hidden"*) pass "its window minimized, the dialog goes with it" ;; *) fail "after minimizing the window: $(P list SgLinuxWindow - | tr '\n' '|')" ;; esac
P post SgLinuxWindow 'Gate Main' 0x0112 0xF120 >/dev/null; sleep 1.5
case "$(P list SgLinuxWindow -)" in *"'Gate Dialog' visible"*) pass "and comes back with it" ;; *) fail "after restoring: $(P list SgLinuxWindow - | tr '\n' '|')" ;; esac
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
