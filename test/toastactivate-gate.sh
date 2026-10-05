#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Clicking a notification in the notification centre brings up the program
# that sent it, as Windows hands a click in its action centre to the program
# (patches/sg/0819-0821). sg-shell's centre (sg-notify) calls windows.ui's
# SgActivateNotification( entry ); here the probe stands in for it. Under
# Xvfb, with the shell's taskbar (balloons need its notification area):
#   - a running program that holds its toast gets the toast's Activated
#     event, with the launch arguments, on its own single-threaded apartment
#     (mutant SG_MUTANT_NO_CENTRE_ACTIVATION);
#   - a program that registered a COM activator for its AppUserModelID
#     (CustomActivator) gets INotificationActivationCallback::Activate --
#     through the new interface's proxy -- whether it runs (mutant
#     SG_MUTANT_NO_TOAST_ACTIVATOR) or not: then COM starts it from its
#     per-user LocalServer32 (HKCU\Software\Classes\CLSID; mutant
#     SG_MUTANT_NO_USER_LOCALSERVER);
#   - the activator named by the program's Start menu shortcut
#     (System.AppUserModel.ToastActivatorCLSID), which a shortcut now keeps
#     (mutant SG_MUTANT_NO_LINK_PROPERTIES);
#   - a click on the toast on screen calls the activator too, raises
#     Activated, and the toast leaves the centre;
#   - a program gone, with no activator, is started;
#   - a balloon's icon gets NIN_BALLOONUSERCLICK (mutant
#     SG_MUTANT_NO_CENTRE_ACTIVATION);
#   - a toast replacing another by tag replaces it in the centre too.
#
#   WINE=/opt/wine-sg/bin/wine test/toastactivate-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0 XP=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-toastactivate.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/toastactivate-probe.c" \
    -lole32 -loleaut32 -lruntimeobject -luser32 -luuid -lshell32 -lpropsys || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/probe.exe"
COM='{5A3C1E10-7B2D-4C11-9E01-534754410001}'
LNK='{5A3C1E10-7B2D-4C11-9E01-534754410002}'
reg() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg 'HKCU\Software\Wine\Explorer' /v Desktop /d shell
# the probe's activators, registered for this user as programs do
reg 'HKCU\Software\Classes\AppUserModelId\SG.Probe.Com' /v CustomActivator /d "$COM"
reg "HKCU\\Software\\Classes\\CLSID\\$COM\\LocalServer32" /ve /d 'C:\probe.exe comserver'
reg "HKCU\\Software\\Classes\\CLSID\\$LNK\\LocalServer32" /ve /d 'C:\probe.exe comserver'
"$WINESERVER" -w
mkdir -p "$T/run"; export XDG_RUNTIME_DIR="$T/run" SG_LOCK_CONTROL=/nonexistent
"$WINE" explorer /desktop=shell,1280x800 >/dev/null 2>&1 &
sleep 8

waitfile() { i=0; while [ ! -s "$C/$1" ] && [ $i -lt "${2:-150}" ]; do sleep 0.2; i=$((i + 1)); done; tr -d '\r' < "$C/$1" 2>/dev/null; }
entry() { "$WINE" reg query "HKCU\\Software\\Stained Glass\\Notifications\\History\\$(printf %08d "$1")" 2>/dev/null | tr -d '\r'; }
val() { printf '%s\n' "$1" | sed -n "s/^ *$2 *REG_[A-Z_]* *//p"; }
centre() { timeout -s KILL 60 "$WINE" 'C:\probe.exe' centre "$1" 2>/dev/null | tr -d '\r' | sed -n 's/^hr=//p'; }

# 1. a running program holding its toast: Activated
"$WINE" 'C:\probe.exe' events SG.Probe.Events 'Events toast' >/dev/null 2>&1 &
seq=$(waitfile events.seq)
e=$(entry "$seq")
[ "$(val "$e" AUMID)" = SG.Probe.Events ] && [ -n "$(val "$e" Pid)" ] && [ -n "$(val "$e" ToastId)" ] && \
    [ -n "$(val "$e" Window)" ] && val "$e" Exe | grep -qi 'probe.exe' \
    && pass "the history entry says who takes a click: AppUserModelID, process, toast, window, file" \
    || fail "history entry $seq: $(printf '%s\n' "$e" | tr -s ' ' | tr '\n' '|')"
hr=$(centre "$seq")
out=$(waitfile events.txt 100)
[ "$hr" = 00000000 ] && [ "$out" = "event=1 args=from-centre own_thread=1" ] \
    && pass "a running program holding the toast gets Activated with its arguments, on its own apartment" \
    || fail "events: hr=$hr $out"

# 2. a COM activator, the program running
"$WINE" 'C:\probe.exe' comrun SG.Probe.Com 'Com toast' >/dev/null 2>&1 &
seq=$(waitfile comrun.seq)
e=$(entry "$seq")
[ "$(val "$e" Activator)" = "$COM" ] && pass "the activator registered for the AppUserModelID is kept" \
    || fail "Activator: '$(val "$e" Activator)'"
hr=$(centre "$seq")
out=$(waitfile com.txt 100)
[ "$hr" = 00000000 ] && [ "$out" = "com=1 aumid=SG.Probe.Com args=from-centre count=0" ] \
    && pass "a running program's activator: INotificationActivationCallback::Activate(AUMID, arguments)" \
    || fail "comrun: hr=$hr $out"

# 3. a COM activator, the program gone: COM starts it (per-user LocalServer32)
timeout -s KILL 60 "$WINE" 'C:\probe.exe' comshow SG.Probe.Com 'Closed toast' >/dev/null 2>&1
seq=$(waitfile comshow.seq); rm -f "$C/comshow.seq"
hr=$(centre "$seq")
out=$(waitfile comserver.txt 100)
[ "$hr" = 00000000 ] && [ "$out" = "com=1 aumid=SG.Probe.Com args=from-centre count=0" ] \
    && pass "a program gone is started by COM for its activator (LocalServer32 under HKCU)" \
    || fail "comserver: hr=$hr $out"
rm -f "$C/comserver.txt"

# 4. the activator named by the Start menu shortcut
out=$(timeout -s KILL 60 "$WINE" 'C:\probe.exe' link SG.Probe.Lnk "$LNK" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -qx 'link_aumid=SG.Probe.Lnk' && printf '%s\n' "$out" | grep -qx 'link_activator=1' \
    && printf '%s\n' "$out" | grep -qx 'link_count=2' && printf '%s\n' "$out" | grep -qx 'link_target_ok=1' \
    && pass "a shortcut keeps its properties (AppUserModelID, toast activator) through a save and a load" \
    || fail "shortcut properties: $(printf '%s\n' "$out" | tr '\n' ' ')"
timeout -s KILL 60 "$WINE" 'C:\probe.exe' comshow SG.Probe.Lnk 'Lnk toast' >/dev/null 2>&1
seq=$(waitfile comshow.seq); rm -f "$C/comshow.seq"
e=$(entry "$seq")
hr=$(centre "$seq")
out=$(waitfile comserver.txt 100)
[ "$(val "$e" Activator)" = "$LNK" ] && [ "$hr" = 00000000 ] && [ "$out" = "com=1 aumid=SG.Probe.Lnk args=from-centre count=0" ] \
    && pass "the activator of the program's shortcut (ToastActivatorCLSID) is found and called" \
    || fail "shortcut activator: '$(val "$e" Activator)' hr=$hr $out"
rm -f "$C/comserver.txt"

# 5. the toast on screen clicked: Activated, the activator too, and it leaves the centre
timeout -s KILL 90 "$WINE" 'C:\probe.exe' popup SG.Probe.Com 'Popup toast' >/dev/null 2>&1
out=$(waitfile popup.txt 50 | tr '\n' ' ' | tr -s ' ')
[ "$out" = "popup=1 event=1 args=from-popup own_thread=1 com=1 aumid=SG.Probe.Com args=from-popup count=0 still_listed=0 " ] \
    && pass "a click on the toast on screen: Activated, the activator, and it leaves the centre" || fail "popup: $out"

# 6. a program gone, no activator: it is started
timeout -s KILL 60 "$WINE" 'C:\probe.exe' comshow SG.Probe.Plain 'Plain toast' >/dev/null 2>&1
seq=$(waitfile comshow.seq); rm -f "$C/comshow.seq" "$C/launched.txt"
hr=$(centre "$seq")
out=$(waitfile launched.txt 100)
[ "$hr" = 00000000 ] && [ "$out" = "launched=1" ] && pass "a program gone, with no activator, is started" || fail "launch: hr=$hr $out"

# 7. a balloon: its icon's window is told
"$WINE" 'C:\probe.exe' balloon >/dev/null 2>&1 &
seq=$(waitfile balloon.seq)
e=$(entry "$seq")
hr=$(centre "$seq")
out=$(waitfile balloon.txt 100)
[ -n "$(val "$e" Owner)" ] && [ "$hr" = 00000000 ] && [ "$out" = "click=1 id=7" ] \
    && pass "a balloon's icon gets NIN_BALLOONUSERCLICK" || fail "balloon: Owner '$(val "$e" Owner)' hr=$hr $out"

# 8. a toast with a tag replaces the one before it in the centre
timeout -s KILL 60 "$WINE" 'C:\probe.exe' tag SG.Probe.Tag >/dev/null 2>&1
all=$("$WINE" reg query 'HKCU\Software\Stained Glass\Notifications\History' /s 2>/dev/null | tr -d '\r')
n1=$(printf '%s\n' "$all" | grep -c 'Title.*REG_SZ.*Tagged one'); n2=$(printf '%s\n' "$all" | grep -c 'Title.*REG_SZ.*Tagged two')
n3=$(printf '%s\n' "$all" | grep -c 'Title.*REG_SZ.*Untagged')
[ "$n1" = 0 ] && [ "$n2" = 1 ] && [ "$n3" = 1 ] && pass "a toast with the same tag replaces the one in the centre" \
    || fail "tag: one $n1 two $n2 untagged $n3"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
