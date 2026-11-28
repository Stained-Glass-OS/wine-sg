#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux app's window on the taskbar is its app's (patches/sg/9511), as
# Windows knows a window's app by its AppUserModelID. sg-shell's sg-linuxapp
# gives each Linux app's Start shortcut the ID SG.LinuxApp.<desktop id>,
# maps the app's window classes to it (HKCU\Software\Stained Glass\Linux
# apps\Window classes) and registers the ID (HKCU\Software\Classes\
# AppUserModelId\<ID>: DisplayName, RelaunchCommand, SgShortcut). Before,
# the button's program was "\sg-linux\<class>": the app pinned from Start
# showed twice (pin and running button: every Linux pin starts
# sg-linuxapp64.exe), its menu's Pin made a shortcut to nothing and
# middle-click started nothing.
#   - the running window's button stands in its pinned shortcut's place: one button, no pin;
#   - middle-click starts another (the app's RelaunchCommand);
#   - unpinned, its menu's Pin to taskbar pins the app's own shortcut (a copy with the ID),
#     and the pin is the running button again.
# The window list comes from a stand-in sg-lockctl (fake-lockctl.c), as in xwin-taskbar-gate.sh.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxappid-gate.sh
#   mutants: SG_MUTANT_LINUX_NO_APPID (explorer/systray.c), SG_MUTANT_PIN_NO_APPID (explorer/taskpins.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v cc >/dev/null || { echo "SKIP: needs $MINGW and cc"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-linuxappid.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { fail "stand-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/linuxappid-probe.exe" "$HERE/linuxappid-probe.c" -lole32 -luuid || { fail "link probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$T/linuxappid-probe.exe" "$WINEPREFIX/drive_c/"
U=$(id -un)
C="$WINEPREFIX/drive_c"
START='C:\users\'"$U"'\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Linux apps'
PINS="$C/users/$U/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar"
PINSW='C:\users\'"$U"'\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar'
ID=SG.LinuxApp.org.example.GateApp
# what sg-linuxapp writes for the app (its shortcut, the ID's key, the class map); the
# "app" is a cmd line that leaves a mark, standing in for sg-linuxapp64.exe --run
mkdir -p "$C/users/$U/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Linux apps" "$PINS"
RELAUNCH='C:\windows\system32\cmd.exe /c echo relaunched> C:\relaunched.txt'
"$WINE" "C:\\linuxappid-probe.exe" link "$START\\Gate App.lnk" 'C:\windows\system32\cmd.exe' '/c echo started> C:\started.txt' "$ID" >/dev/null 2>&1 \
    || fail "the app's shortcut was not made"
cp "$C/users/$U/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Linux apps/Gate App.lnk" "$PINS/Gate App.lnk"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Taskbar' /v PinOrder /t REG_MULTI_SZ /d 'Gate App.lnk' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Linux apps\Window classes' /v GateApp /d "$ID" /f >/dev/null 2>&1
"$WINE" reg add "HKCU\\Software\\Classes\\AppUserModelId\\$ID" /v DisplayName /d 'Gate App' /f >/dev/null 2>&1
"$WINE" reg add "HKCU\\Software\\Classes\\AppUserModelId\\$ID" /v RelaunchCommand /d "$RELAUNCH" /f >/dev/null 2>&1
"$WINE" reg add "HKCU\\Software\\Classes\\AppUserModelId\\$ID" /v SgShortcut /d "$START\\Gate App.lnk" /f >/dev/null 2>&1
"$WINESERVER" -w
TAB=$(printf '\t')
printf 'END\n' > "$T/list"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$C"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
S() { echo "== \$1" >> "$T/log.out"; "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
mid() { echo "\$1" | awk -F, '{ printf "%d %d", (\$1 + \$3) / 2, (\$2 + \$4) / 2 }'; }
first() { "$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | sed -n "s/^\$1=//p" | head -1; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
S idle
printf '4242 shown - GateApp${TAB}Gate App - a document\nEND\n' > "$T/list"; sleep 3
S running
# middle-click: another window of the app
xdotool mousemove \$(mid "\$(first button)") click 2; sleep 3
# unpinned (its menu, Unpin from tas&kbar), then pinned again (Pin to tas&kbar)
xdotool mousemove \$(mid "\$(first button)") click 3; sleep 1.5; xdotool key k; sleep 2.5
ls "$PINS" > "$T/unpinned.out" 2>&1
S unpinned
xdotool mousemove \$(mid "\$(first button)") click 3; sleep 1.5; xdotool key k; sleep 2.5
ls "$PINS" > "$T/pinned.out" 2>&1
"$WINE" linuxappid-probe.exe appid "$PINSW\\Gate App.lnk" 2>/dev/null | tr -d '\r' > "$T/pinid.out"
S repinned
printf 'END\n' > "$T/list"; sleep 3
S gone
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" | grep -v "appbar\|work=\|bar=\|autohide"
count() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { n++ } END { print n + 0 }' "$T/log.out"; }

[ "$(count idle pin)" = 1 ] && [ "$(count idle button)" = 0 ] && pass "the Linux app pinned from Start: its pin, not running" || fail "idle: pins $(count idle pin) buttons $(count idle button)"
[ "$(count running pin)" = 0 ] && [ "$(count running button)" = 1 ] && pass "running, its window's button stands in the pin's place (one button)" \
    || fail "running: pins $(count running pin) buttons $(count running button) (the app shown twice)"
grep -q relaunched "$C/relaunched.txt" 2>/dev/null && pass "middle-click starts another (the app's RelaunchCommand)" || fail "middle-click started nothing"
! grep -q "Gate App" "$T/unpinned.out" && [ "$(count unpinned pin)" = 0 ] && [ "$(count unpinned button)" = 1 ] && pass "its menu's Unpin from taskbar unpins the app" \
    || fail "unpin: $(tr '\n' '|' < "$T/unpinned.out") pins $(count unpinned pin) buttons $(count unpinned button)"
grep -qx 'Gate App.lnk' "$T/pinned.out" && grep -qx "$ID" "$T/pinid.out" && pass "its menu's Pin to taskbar pins the app's own shortcut, with its ID" \
    || fail "pin: $(tr '\n' '|' < "$T/pinned.out") id '$(cat "$T/pinid.out")'"
[ "$(count repinned pin)" = 0 ] && [ "$(count repinned button)" = 1 ] && pass "pinned again, still one button" || fail "repinned: pins $(count repinned pin) buttons $(count repinned button)"
[ "$(count gone pin)" = 1 ] && [ "$(count gone button)" = 0 ] && pass "closed, its pin stays" || fail "gone: pins $(count gone pin) buttons $(count gone button)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
