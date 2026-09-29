#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Elevated programs' windows on the taskbar, minimizable (patches/sg/0500).
# An elevated program's window lives on the Wine desktop
# WinSta0\sg-elevated-<display>, whose windows IsWindowVisible calls hidden
# (not our desktop), so no elevated window had a button. Its style says
# whether it is shown. The compositor's WINDOWS list (sg-lockctl WINDOWS
# --out) says which is focused or minimized: a click brings it forward
# (ACTIVATE), minimizes it when it is in front (MINIMIZE), and brings it
# back once minimized. A stand-in sg-lockctl (SG_LOCKCTL) serves the list
# the gate writes and records what it is asked.
#
#   WINE=/opt/wine-sg/bin/wine test/elevmin-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v cc >/dev/null || { echo "SKIP: needs $MINGW and cc"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-elevmin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { fail "stand-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/elevmin-probe.exe" "$HERE/elevmin-probe.c" -luser32 || { fail "elevated probe did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$T/elevmin-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
TAB=$(printf '\t')
printf 'END\n' > "$T/list"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
S() { echo "== \$1" >> "$T/log.out"; "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
mid() { echo "\$1" | awk -F, '{ printf "%d %d", (\$1 + \$3) / 2, (\$2 + \$4) / 2 }'; }
first() { "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | sed -n "s/^\$1=//p" | head -1; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 4
"$WINE" elevmin-probe.exe > "$T/probe.out" 2>&1 &
i=0; while ! grep -q 'xwin=' "$T/probe.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
X=\$(sed -n 's/^xwin=//p' "$T/probe.out" | tr -d '\r')
echo "\$X" > "$T/xwin"
printf '3 %s 100 100 300 200 shown - Elevated Probe\n' "\$X" > "$T/windows"
sleep 3
S listed
B=\$(first button)
xdotool mousemove \$(mid "\$B") click 1; sleep 2
printf '3 %s 100 100 300 200 shown focused Elevated Probe\n' "\$X" > "$T/windows"; sleep 2.5
S focused
xdotool mousemove \$(mid "\$B") click 1; sleep 2
printf '3 %s 100 100 300 200 minimized - Elevated Probe\n' "\$X" > "$T/windows"; sleep 2.5
xdotool mousemove \$(mid "\$B") click 1; sleep 2
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" | grep -v "appbar\|work=\|bar=\|autohide"
echo "      commands: $(tr '\n' '|' < "$T/commands" 2>/dev/null)"
count() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { n++ } END { print n + 0 }' "$T/log.out"; }

X=$(cat "$T/xwin" 2>/dev/null)
[ -n "$X" ] || fail "the elevated probe did not start: $(cat "$T/probe.out")"
[ "$(count listed button)" = 1 ] && pass "the elevated window has a button" || fail "buttons: $(count listed button)"
sed -n 1p "$T/commands" 2>/dev/null | grep -qx "ACTIVATE 3 $X" && pass "a click brings it forward (ACTIVATE 3 $X)" || fail "first click: $(sed -n 1p "$T/commands" 2>/dev/null)"
sed -n 2p "$T/commands" 2>/dev/null | grep -qx "MINIMIZE 3 $X" && pass "in front, a click minimizes it (MINIMIZE 3 $X)" || fail "second click: $(sed -n 2p "$T/commands" 2>/dev/null)"
sed -n 3p "$T/commands" 2>/dev/null | grep -qx "ACTIVATE 3 $X" && pass "minimized, a click brings it back (ACTIVATE)" || fail "third click: $(sed -n 3p "$T/commands" 2>/dev/null)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
