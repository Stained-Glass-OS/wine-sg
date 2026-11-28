#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Linux programs' notifications as toasts (patches/sg/9512): the host sg-session's
# sg-notify-bridge (org.freedesktop.Notifications) hands them to -- rundll32
# windows.ui.dll,SgLinuxNotifications, lines on its standard input -- shows each as a toast of
# the program's AppUserModelID, kept in the notification centre's history, and says on its
# output what the person did. Under Xvfb, the gate playing both the bridge and the person:
#   - SHOW: a toast on screen, in the history under the app's ID and registered name;
#     an ID with no registered name gets the bridge's (an app's own name is kept);
#   - a click on it: ACT n default; the notification centre's click on a button of an
#     earlier one (SgActivateNotification with its arguments): ACT n <key>;
#   - the same number again replaces it (one toast on screen, one history entry);
#   - HIDE: it goes; the input's end ends the host.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxnotify-gate.sh
#   mutant: SG_MUTANT_LINUX_NOTE_NO_ANSWER (dlls/windows.ui/toast.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${DPY:-$((700 + $$ % 200))}"
RC=0 XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo xdotool "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-linuxnotify.XXXXXX)
cleanup() {
    exec 3>&- 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    sg_kill_prefix "$T/prefix"
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O2 -o "$T/probe.exe" "$HERE/linuxnotify-probe.c" -luser32 || { fail "probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# the app's own registration (sg-shell's sg-linuxapp)
"$WINE" reg add 'HKCU\Software\Classes\AppUserModelId\SG.LinuxApp.org.gate.Mail' /v DisplayName /d 'Gate Mail' /f >/dev/null 2>&1
"$WINESERVER" -w
probe() { "$WINE" "$T/probe.exe" "$@" 2>/dev/null | tr -d '\r'; }
toasts() { probe toasts | sed -n 's/^toasts=//p'; }
mkfifo "$T/in"
timeout -s KILL 240 "$WINE" rundll32.exe windows.ui.dll,SgLinuxNotifications < "$T/in" > "$T/out" 2>/dev/null &
HP=$!
exec 3<> "$T/in"   # read-write: never blocks, even if the host is gone
i=0; while ! grep -q READY "$T/out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
grep -q READY "$T/out" && pass "the host starts (READY)" || fail "no READY: $(cat "$T/out")"
TAB=$(printf '\t')
X1='<toast launch="default"><visual><binding template="ToastGeneric"><text>New mail</text><text>From Ann</text></binding></visual><actions><action content="Reply" arguments="reply"/></actions></toast>'
printf 'SHOW 1\tSG.LinuxApp.org.gate.Mail\tThunderbird\t%s\n' "$X1" >&3
i=0; while [ "$(toasts)" != 1 ] && [ $i -lt 30 ]; do sleep 0.5; i=$((i + 1)); done
[ "$(toasts)" = 1 ] && pass "SHOW puts a toast on screen" || fail "toasts on screen: $(toasts)"
hist=$("$WINE" reg query 'HKCU\Software\Stained Glass\Notifications\History\00000000' 2>/dev/null | tr -d '\r')
printf '%s\n' "$hist" | grep -q 'Title.*New mail' && printf '%s\n' "$hist" | grep -q 'AUMID.*SG.LinuxApp.org.gate.Mail' \
    && printf '%s\n' "$hist" | grep -q 'App.*REG_SZ.*Gate Mail' \
    && pass "kept in the notification centre's history, under the app's ID and its registered name" || fail "history: $(printf '%s\n' "$hist" | tr '\n' '|')"
# a click on the toast's text
R=$(probe toasts | sed -n 's/^toast=//p' | head -1)
set -- $(echo "$R" | awk -F, '{ printf "%d %d", $1 + 60, $2 + 40 }')
xdotool mousemove "$1" "$2"; sleep 0.3; xdotool click 1
i=0; while ! grep -q "^ACT 1" "$T/out" && [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done
grep -qx "ACT 1${TAB}default" "$T/out" && pass "a click on it: ACT 1 default" || fail "click: $(tr '\n' '|' < "$T/out")"
# a second one, from a program with no registered name; then its button from the centre
X2='<toast launch="default"><visual><binding template="ToastGeneric"><text>Build done</text></binding></visual><actions><action content="Open" arguments="open-log"/></actions></toast>'
printf 'SHOW 2\tSG.LinuxNotify.gate-tool\tgate-tool\t%s\n' "$X2" >&3
sleep 2
name=$("$WINE" reg query 'HKCU\Software\Classes\AppUserModelId\SG.LinuxNotify.gate-tool' /v DisplayName 2>/dev/null | tr -d '\r' | awk '/DisplayName/ { print $3 }')
[ "$name" = gate-tool ] && pass "an ID with no registered name gets the program's name" || fail "name: '$name'"
appname=$("$WINE" reg query 'HKCU\Software\Classes\AppUserModelId\SG.LinuxApp.org.gate.Mail' /v DisplayName 2>/dev/null | tr -d '\r' | sed -n 's/.*REG_SZ *//p')
[ "$appname" = "Gate Mail" ] && pass "an app's own registered name is kept" || fail "app name now: '$appname'"
seq=$("$WINE" reg query 'HKCU\Software\Stained Glass\Notifications\History' /s 2>/dev/null | tr -d '\r' | awk '/^HKEY/ { k = $0 } /Build done/ { sub(/.*\\/, "", k); print k + 0 }' | head -1)
probe activate "${seq:-99}" open-log > "$T/act.out"
i=0; while ! grep -q "^ACT 2" "$T/out" && [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done
grep -qx "ACT 2${TAB}open-log" "$T/out" && pass "the notification centre's click on its button: ACT 2 open-log" || fail "centre: $(cat "$T/act.out") $(tr '\n' '|' < "$T/out")"
# the same number again replaces it
X3='<toast launch="default"><visual><binding template="ToastGeneric"><text>Copying</text><text>10%</text></binding></visual></toast>'
X4='<toast launch="default"><visual><binding template="ToastGeneric"><text>Copying</text><text>90%</text></binding></visual></toast>'
printf 'SHOW 3\tSG.LinuxNotify.gate-tool\tgate-tool\t%s\n' "$X3" >&3; sleep 1.5
before=$(toasts)
printf 'SHOW 3\tSG.LinuxNotify.gate-tool\tgate-tool\t%s\n' "$X4" >&3; sleep 1.5
after=$(toasts)
copies=$("$WINE" reg query 'HKCU\Software\Stained Glass\Notifications\History' /s 2>/dev/null | tr -d '\r' | grep -c 'Copying')
[ "$after" = "$before" ] && [ "$copies" = 1 ] && pass "the same number again replaces it (on screen and in the centre)" || fail "replace: toasts $before -> $after, history $copies"
printf 'HIDE 3\n' >&3; sleep 1.5
[ "$(toasts)" = $((after - 1)) ] && pass "HIDE takes it away" || fail "after HIDE: $(toasts) (was $after)"
exec 3>&-
i=0; while kill -0 "$HP" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done
! kill -0 "$HP" 2>/dev/null && pass "the input's end ends the host" || fail "the host still runs after its input ended"
sed 's/^/      out: /' "$T/out"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
