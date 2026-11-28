#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program's progress and badge on its taskbar button (patches/sg/9514), as a Windows
# program's ITaskbarList3 progress and overlay icon:
#   - XApp's _NET_WM_XAPP_PROGRESS on its window (Firefox's downloads, file copies): the button
#     fills green as far as it says; gone, the fill goes;
#   - its launcher entry (com.canonical.Unity.LauncherEntry, which sg-session's
#     sg-notify-bridge passes to the Linux notifications host, rundll32
#     windows.ui.dll,SgLinuxNotifications: LAUNCHER <app ID> count progress urgent): the count as
#     a red badge on the button of each of the app's windows (Telegram's unread messages), its
#     progress as the fill; count 0, the badge goes.
# A real xterm framed by the shell, its class the app's (HKCU\Software\Stained Glass\Linux apps\
# Window classes, as sg-shell's sg-linuxapp writes it).
#
#   WINE=/opt/wine-sg/bin/wine test/linuxprogress-gate.sh
#   mutants: SG_MUTANT_EMBED_NO_PROGRESS (winex11 sg_embed.c), SG_MUTANT_FRAME_NO_PROGRESS
#   (explorer systray.c), SG_MUTANT_LAUNCHER_IGNORED (windows.ui toast.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
unset SESSION_MANAGER
for t in Xvfb xterm xwininfo xprop import cc "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linuxprogress.XXXXXX); XP=; XT=; HP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap 'exec 3>&- 2>/dev/null; "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; [ -n "$XT" ] && kill "$XT" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { echo "FAIL  stand-in did not build"; exit 1; }
"$MINGW" -O2 -o "$T/fglock-probe.exe" "$HERE/fglock-probe.c" -luser32 -lgdi32 || { echo "FAIL  button probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/fglock-probe.exe" "$WINEPREFIX/drive_c/"
ID=SG.LinuxApp.gate-xterm
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Linux apps\Window classes' /v XTerm /d "$ID" /f >/dev/null 2>&1
"$WINE" reg add "HKCU\\Software\\Classes\\AppUserModelId\\$ID" /v DisplayName /d 'Gate Terminal' /f >/dev/null 2>&1
"$WINESERVER" -w
xterm -geometry 50x12+120+120 -title 'Linux Terminal' -e sh -c "cd '$T'; exec sh" & XT=$!
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
B=$("$WINE" fglock-probe.exe button 'Linux Terminal' 2>/dev/null | tr -d '\r')
# colour counts in the button: "green right_edge_fraction red"
colours() {
    import -window root "$T/shot.png" 2>/dev/null
    python3 - "$T/shot.png" $B <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
x, y, w, h = map(int, sys.argv[2:6])
g = r = 0; right = -1
for j in range(y, y + h):
    for i in range(x, x + w):
        R, G, Bl = im.getpixel((i, j))
        if abs(R - 0x06) < 40 and abs(G - 0xB0) < 40 and abs(Bl - 0x25) < 40:
            g += 1; right = max(right, i - x)
        if abs(R - 0xD1) < 40 and abs(G - 0x34) < 50 and abs(Bl - 0x38) < 50:
            r += 1
print(g, "%.2f" % ((right + 1) / w), r)
EOF
}
if [ -z "$B" ] || [ "$B" = none ]; then fail "no taskbar button for the Linux window"
else
    set -- $(colours)
    [ "$1" = 0 ] && [ "$3" -lt 4 ] && pass "no progress, no badge at first" || fail "at first: green $1, red $3"
    xprop -id "$XID" -f _NET_WM_XAPP_PROGRESS 32c -set _NET_WM_XAPP_PROGRESS 50; sleep 1.5
    set -- $(colours)
    awk -v g="$1" -v f="$2" 'BEGIN { exit !(g > 60 && f > 0.35 && f < 0.65) }' \
        && pass "_NET_WM_XAPP_PROGRESS 50: its button fills green half way (green $1, to $2)" || fail "XApp progress 50: green $1 to $2"
    xprop -id "$XID" -remove _NET_WM_XAPP_PROGRESS; sleep 1.5
    set -- $(colours)
    [ "$1" = 0 ] && pass "its progress gone, the fill goes" || fail "after the progress went: green $1"
    mkfifo "$T/in"
    "$WINE" rundll32.exe windows.ui.dll,SgLinuxNotifications < "$T/in" > "$T/out" 2>/dev/null &
    HP=$!
    exec 3<> "$T/in"   # read-write: never blocks, even if the host is gone
    i=0; while ! grep -q READY "$T/out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
    printf 'LAUNCHER %s\t7\t-1\t0\n' "$ID" >&3; sleep 2
    set -- $(colours)
    [ "$3" -gt 15 ] && [ "$1" = 0 ] && pass "its launcher entry's count: a red badge on its button (red $3)" || fail "badge: red $3, green $1"
    printf 'LAUNCHER %s\t0\t30\t0\n' "$ID" >&3; sleep 2
    set -- $(colours)
    [ "$3" -lt 4 ] && awk -v g="$1" -v f="$2" 'BEGIN { exit !(g > 30 && f > 0.15 && f < 0.45) }' \
        && pass "count 0: the badge goes; its progress 30: the fill (green $1, to $2)" || fail "after: red $3, green $1 to $2"
    exec 3>&-
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
