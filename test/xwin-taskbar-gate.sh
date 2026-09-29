#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The session's Linux programs' windows on the taskbar (patches/sg/0496).
# A Linux terminal's window is a top-level beside Wine's desktop, which no
# shell hook reports; the compositor lists them (sg-lockctl XWINDOWS) and
# each gets a button with its title. A click brings it forward (XACTIVATE),
# or minimizes it when it is in front (XMINIMIZE); its menu's Close window
# closes it (XCLOSE); gone from the list, its button goes. The list comes
# as a file (sg-lockctl XWINDOWS --out): Wine hands a native program no pipe
# for its output. A stand-in
# sg-lockctl (SG_LOCKCTL) serves a list the gate writes and records what it
# is asked.
#
#   WINE=/opt/wine-sg/bin/wine test/xwin-taskbar-gate.sh
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

T=$(mktemp -d /var/tmp/sg-xwin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { fail "stand-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
TAB=$(printf '\t')
printf '4242 shown - XTerm%sLinux Terminal\nEND\n' "$TAB" > "$T/list"

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
S listed; import -window root "$T/listed.png"
B=\$(first button)
xdotool mousemove \$(mid "\$B") click 1; sleep 2
printf '4242 shown focused XTerm${TAB}Linux Terminal: ~\nEND\n' > "$T/list"; sleep 2.5
xdotool mousemove \$(mid "\$B") click 1; sleep 2
S focused
xdotool mousemove \$(mid "\$B") click 3; sleep 1.5
# the menu stands on the pointer: its title, a line, Close window
set -- \$(mid "\$B"); xdotool mousemove \$((\$1 + 40)) \$((\$2 - 32)) click 1; sleep 2
printf 'END\n' > "$T/list"; sleep 2.5
S gone
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" | grep -v "appbar\|work=\|bar=\|autohide"
echo "      commands: $(tr '\n' '|' < "$T/commands" 2>/dev/null)"
count() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { n++ } END { print n + 0 }' "$T/log.out"; }

[ "$(count listed button)" = 1 ] && pass "the Linux program's window has a button" || fail "buttons: $(count listed button)"
convert "$T/listed.png" -crop 400x40+0+660 +repage "$T/bar.png" 2>/dev/null
sed -n 1p "$T/commands" 2>/dev/null | grep -qx "XACTIVATE 4242" && pass "a click brings it forward (XACTIVATE 4242)" || fail "first click: $(sed -n 1p "$T/commands" 2>/dev/null)"
sed -n 2p "$T/commands" 2>/dev/null | grep -qx "XMINIMIZE 4242" && pass "in front, a click minimizes it (XMINIMIZE)" || fail "second click: $(sed -n 2p "$T/commands" 2>/dev/null)"
sed -n 3p "$T/commands" 2>/dev/null | grep -qx "XCLOSE 4242" && pass "its menu's Close window closes it (XCLOSE)" || fail "close: $(sed -n 3p "$T/commands" 2>/dev/null)"
[ "$(count gone button)" = 0 ] && pass "gone from the list, its button goes" || fail "after it went: $(count gone button) buttons"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
