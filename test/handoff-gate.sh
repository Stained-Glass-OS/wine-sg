#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The system32 names that hand off to App Paths (patches/sg/0120-0124).
#
# calc.exe, charmap.exe, mspaint.exe and snippingtool.exe are system32 programs on Windows,
# which programs start with CreateProcess -- a search of system32 and PATH
# that never reads App Paths. wine-sg ships them as launchers for what App
# Paths registers (sg-shell's Calculator, Paint, Snipping Tool), and Wine's
# own taskmgr.exe and wmplayer.exe hand off the same way. From a 64-bit and a
# 32-bit caller, with the arguments intact; never in a loop; Wine's Task
# Manager still runs when nothing is registered. Win+Shift+S and Print Screen
# run snippingtool.exe /clip, Ctrl+Shift+Esc taskmgr.exe (0123).
#
#   WINE=/opt/wine-sg/bin/wine test/handoff-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v "$MINGW32" >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-handoff.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/handoff-probe.c" &&
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/handoff-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/probe64.exe"; cp "$T/probe32.exe" "$C/probe32.exe"; cp "$T/probe64.exe" "$C/standin.exe"
for n in calc charmap mspaint snippingtool; do
    for d in system32 syswow64; do
        [ -f "$C/windows/$d/$n.exe" ] && pass "$d\\$n.exe exists" || fail "no $d\\$n.exe"
    done
done
AP='HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths'
for n in calc charmap mspaint snippingtool taskmgr wmplayer; do
    "$WINE" reg add "$AP\\$n.exe" /ve /d 'C:\standin.exe' /f >/dev/null 2>&1
done
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$C"
P() { echo "\$*" >> "$T/log.out"; "$WINE" "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
# the stand-in writes after the launcher has exited: wait for its line
n=0
mark() { echo "== \$1" >> "$C/standin.log"; }
seen() { n=\$((n + 1)); i=0; while [ "\$(grep -c '^cmdline=' "$C/standin.log" 2>/dev/null)" -lt \$n ] && [ \$i -lt 20 ]; do sleep 0.5; i=\$((i + 1)); done; }
mark calc64;   P probe64.exe run 'calc.exe 12 + 30'; seen
mark paint64;  P probe64.exe run 'mspaint "C:\\my pictures\\a b.png"'; seen
mark snip64;   P probe64.exe run 'snippingtool.exe /clip'; seen
mark task64;   P probe64.exe run 'taskmgr.exe /7'; seen
mark wmp64;    P probe64.exe run '"C:\\Program Files\\Windows Media Player\\wmplayer.exe" "C:\\music\\x y.mp3"'; seen
mark charmap32; P probe32.exe run 'charmap'; seen
mark calc32;   P probe32.exe run 'calc.exe /x'; seen
mark paint32;  P probe32.exe run 'mspaint.exe'; seen
mark task32;   P probe32.exe run 'taskmgr.exe'; seen
# keys reach the desktop's X window only once a window is on it: a Notepad, clicked
"$WINE" notepad >/dev/null 2>&1 &
sleep 3; xdotool mousemove 200 150 click 1; sleep 1
mark keys
xdotool key super+shift+s; seen
mark prtsc
xdotool key Print; seen
mark ctrlshiftesc
xdotool key ctrl+shift+Escape; seen
mark end
# loop guard: App Paths naming another system copy must not chain launchers
"$WINE" reg add '$AP\\calc.exe' /ve /d 'C:\\windows\\syswow64\\calc.exe' /f >/dev/null 2>&1
"$WINE" probe64.exe run 'calc.exe loop' >/dev/null 2>&1 &
sleep 4; P probe64.exe count calc.exe; P probe64.exe find '#32770'
"$WINE" taskkill /f /im calc.exe >/dev/null 2>&1; sleep 1
# nothing registered: Wine's own Task Manager still runs
"$WINE" reg delete '$AP\\taskmgr.exe' /f >/dev/null 2>&1
"$WINE" taskmgr >/dev/null 2>&1 &
sleep 4; P probe64.exe find TaskManagerWindow; P probe64.exe find '#32770' taskmgr
EOF
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
tr -d '\r' < "$C/standin.log" > "$T/standin.txt" 2>/dev/null
sed 's/^/      /' "$T/log.out"; sed 's/^/      | /' "$T/standin.txt"

# the lines the stand-in recorded after mark $1
got() { awk -v m="== $1" '$0 == m { on = 1; next } /^== / { on = 0 } on' "$T/standin.txt"; }
want() { # $1 mark, $2 expected cmdline, $3 what
    if [ "$(got "$1")" = "cmdline=$2" ]; then pass "$3"; else fail "$3: got '$(got "$1")'"; fi
}
want calc64  '"C:\standin.exe" 12 + 30' 'calc.exe from a 64-bit program starts the App Paths Calculator with its arguments'
want paint64 '"C:\standin.exe" "C:\my pictures\a b.png"' 'mspaint (no .exe) keeps a quoted path with spaces'
want snip64  '"C:\standin.exe" /clip' 'snippingtool.exe /clip hands off'
want task64  '"C:\standin.exe" /7' "Wine's taskmgr.exe hands off to the App Paths Task Manager"
want wmp64   '"C:\standin.exe" "C:\music\x y.mp3"' "Wine's wmplayer.exe hands off to the App Paths Media Player"
want charmap32 '"C:\standin.exe"' 'charmap from a 32-bit program (0124)'
want calc32  '"C:\standin.exe" /x' 'calc.exe from a 32-bit program (syswow64) reaches the 64-bit App Paths view'
want paint32 '"C:\standin.exe"' 'mspaint.exe from a 32-bit program'
want task32  '"C:\standin.exe"' 'taskmgr.exe from a 32-bit program'
want keys    '"C:\standin.exe" /clip' 'Win+Shift+S runs snippingtool.exe /clip'
want prtsc   '"C:\standin.exe" /clip' 'Print Screen runs snippingtool.exe /clip'
want ctrlshiftesc '"C:\standin.exe"' 'Ctrl+Shift+Esc opens Task Manager (taskmgr.exe, App Paths)'
after() { awk -v c="$1" '$0 == c { getline; print; exit }' "$T/log.out"; }
# a chain would never settle on the launcher's "cannot find" message
[ "$(after 'probe64.exe count calc.exe')$(after 'probe64.exe find #32770')" = "count=1found=1" ] \
    && pass "App Paths naming a system copy does not chain launchers (it says it cannot find the program)" \
    || fail "launcher loop: $(after 'probe64.exe count calc.exe') $(after 'probe64.exe find #32770')"
case "$(after 'probe64.exe find TaskManagerWindow')$(after 'probe64.exe find #32770 taskmgr')" in
    *found=1*) pass "with nothing registered, Wine's Task Manager still opens" ;;
    *) fail "taskmgr without App Paths opened nothing" ;; esac
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
