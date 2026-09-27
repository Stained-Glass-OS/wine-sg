#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Toast notifications (patches/sg/0436, 0437): Windows.UI.Notifications and the
# Windows.Data.Xml.Dom documents toasts are written in. Programs ask for a
# toast -- Firefox and Thunderbird for web and mail notifications, Chrome,
# Electron programs, .NET programs -- and it shows at the bottom right above
# the taskbar; clicking it or one of its buttons tells the program, as do
# its close button and its time running out. Under Xvfb, with the probe
# playing the person.
#
#   WINE=/opt/wine-sg/bin/wine test/toast-gate.sh
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
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-toast.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/toast-probe.exe" "$HERE/toast-probe.c" \
    -lole32 -loleaut32 -lruntimeobject -luser32 -luuid || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1600x1000x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 180 "$WINE" "$T/toast-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -Eq "^$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" | head -1))"; fi; }
check 'transacted=1'      "RegCreateKeyTransacted, RegOpenKeyTransacted, RegDeleteKeyTransacted (0436)"
check 'manager=00000000'  "ToastNotificationManager activates"
check 'factory=00000000'  "ToastNotification activates"
check 'notifier=00000000' "a notifier for a program's AppUserModelID"
check 'setting=0$'        "notifications are enabled"
check 'template=<toast><visual><binding template="ToastText02"><text id="1"/><text id="2"/></binding></visual></toast>$' \
                          "GetTemplateContent: the ToastText02 template, as compact XML"
check 'edited=<toast launch="from-body"><visual><binding template="ToastText02"><text id="1">Probe title</text><text id="2">A toast made from a template</text></binding></visual><actions><action content="Open" arguments="from-button"/></actions></toast>$' \
                          "the template edited through the DOM (text nodes, attributes, new elements)"
check 'show=00000000 shown=1' "Show puts the toast on screen"
check 'corner=1'          "at the bottom right of the work area"
check 'body_click=1 args=from-body' "clicking it raises Activated with the toast's launch arguments"
check 'gone_after_click=1' "and closes it"
check 'button_click=1 args=from-button' "clicking a button raises Activated with the button's arguments"
check 'hide=1 reason=1 gone=1' "Hide closes it: Dismissed, ApplicationHidden"
check 'loadxml=00000000'  "XmlDocument.LoadXml reads toast XML text"
check 'close=1 reason=0'  "its close button: Dismissed, UserCanceled"
check 'timeout=1 reason=2' "left alone it times out: Dismissed, TimedOut"
check 'sta_event=1 on_own_thread=1' "a single-threaded apartment gets its events on its own thread"
check 'replaced=1'        "a toast with the same tag replaces the one on screen"
check 'badxml=1'          "malformed XML is refused"
check 'done=1'            "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
