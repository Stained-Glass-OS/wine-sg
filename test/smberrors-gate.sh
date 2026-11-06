#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What network drives say when something goes wrong (patches/sg/0982):
#
#  - NET USE with a wrong password prints, as Windows' does,
#    "System error 1326 has occurred." and "The user name or password is
#    incorrect." -- it printed only "Logon failure." (mutant
#    SG_MUTANT_NETUSE_NO_SYSERR in programs/net/net.c; the message itself
#    is kernelbase's for ERROR_LOGON_FAILURE);
#  - a remembered drive ("Reconnect at sign-in") whose server refuses it
#    at sign-in gets the notification "Could not reconnect all network
#    drives" (kept in the notification history, as the notification centre
#    lists it) -- there was none, the drive was just missing (mutant
#    SG_MUTANT_NO_RECONNECT_NOTICE in dlls/mpr/wnet.c);
#  - File Explorer's "See more" button: its tooltip goes while its menu is
#    open -- it covered the menu's first item (mutant
#    SG_MUTANT_MENU_UNDER_TOOLTIP in programs/explorer/fileexplorer.c).
#
# The network side is the stand-in for sg-netmountd of mapcred-gate.sh (on
# /run/stained-glass-net/netmount.sock, with sudo, removed after).
#
#   WINE=... test/smberrors-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
NET=/run/stained-glass-net
DRIVES=$NET/drives/$(id -u)
RC=0; XP=; DP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb xdotool import python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
/usr/bin/python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
[ -e "$NET" ] && { echo "SKIP: $NET exists (sg-netmountd's)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-smberrors.XXXXXX); chmod 755 "$T"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "$DP" ] && kill "$DP" 2>/dev/null
    sudo -n rm -rf "$NET"
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/smberrors-probe.exe" "$HERE/smberrors-probe.c" -luser32 || { echo "FAIL  probe did not build"; exit 1; }
sudo -n mkdir -p "$DRIVES" && sudo -n chown -R "$(id -u)" "$NET" || { echo "SKIP: cannot make $NET"; exit 77; }
mkdir "$T/share"; echo hello > "$T/share/hello.txt"
python3 "$HERE/mapcred-netmountd.py" "$NET/netmount.sock" "$DRIVES" "$T/share" Right1pass "$T/requests" & DP=$!
i=0; while [ ! -S "$NET/netmount.sock" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done

unset DISPLAY XAUTHORITY
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
DPY=":$(cat "$T/display")"
case "$DPY" in :|:0) echo "FAIL  no display of our own"; exit 1 ;; esac
export DISPLAY="$DPY"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/smberrors-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c" || exit 1

# 1. NET USE, a wrong password
timeout 60 "$WINE" net use 'X:' '\\sgnetuse\files' Wrong2pass /user:other > "$T/out" 2>&1 </dev/null
tr -d '\r' < "$T/out" | sed 's/^/      /'
if tr -d '\r' < "$T/out" | grep -qx 'System error 1326 has occurred.' && tr -d '\r' < "$T/out" | grep -qx 'The user name or password is incorrect.'; then
    pass "NET USE with a wrong password: \"System error 1326 has occurred.\" and \"The user name or password is incorrect.\""
else fail "NET USE with a wrong password said: $(tr -d '\r' < "$T/out" | tr '\n' '|')"; fi
[ "$(tr -d '\r' < "$T/out" | sed -n 2p)" = "" ] && pass "...with the blank line between them, as on Windows" || fail "layout: $(tr -d '\r' < "$T/out" | tr '\n' '|')"

# 2. a remembered drive whose server refuses it at sign-in
"$WINE" reg add 'HKCU\Network\Q' /v RemotePath /d '\\sgdown\gone' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Network\Q' /v ConnectionType /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
XDG_SESSION_ID=4243 "$WINE" explorer /desktop=shell,1024x700 > /dev/null 2>&1 &
found=; i=0
while [ $i -lt 60 ]; do
    "$WINE" reg query 'HKCU\Software\Stained Glass\Notifications\History' /s > "$T/hist" 2>/dev/null
    grep -q 'Could not reconnect all network drives' "$T/hist" && { found=1; break; }
    sleep 1; i=$((i + 1))
done
grep -q '^LOGON\|^MAP .*sgdown\|sgdown' "$T/requests" 2>/dev/null && pass "the shell tried the remembered drive at sign-in" || fail "the remembered drive was not tried: $(cat "$T/requests" 2>/dev/null)"
[ -n "$found" ] && pass "a drive not reconnected at sign-in: \"Could not reconnect all network drives\" ($(tr -d '\r' < "$T/hist" | grep -m1 Body | sed 's/.*REG_SZ *//'))" \
    || fail "no \"Could not reconnect\" notification: $(tr -d '\r' < "$T/hist" | grep -i title | head -3 | tr '\n' '|')"
import -window root "$T/notice.png" 2>/dev/null
"$WINE" reg delete 'HKCU\Network' /f >/dev/null 2>&1

# 3. File Explorer's "See more": its tooltip and its menu
"$WINE" explorer.exe 'C:\' >/dev/null 2>&1 &
sleep 5
"$WINE" 'C:\smberrors-probe.exe' cmdbar > "$T/cmdbar" 2>/dev/null
cb=$(tr -d '\r' < "$T/cmdbar" | sed -n 's/^CMDBAR //p')
if [ -z "$cb" ]; then fail "no File Explorer command bar: $(cat "$T/cmdbar")"; else
    import -window root "$T/fe.png" 2>/dev/null
    # the rightmost glyph in the command bar is "See more"
    xy=$(/usr/bin/python3 - "$T/fe.png" $cb <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
l, t, r, b = map(int, sys.argv[2:6])
y0 = (t + b) // 2
bg = im.getpixel((r - 2, y0))
for x in range(r - 3, l, -1):
    for y in range(t + 4, b - 4):
        p = im.getpixel((x, y))
        if sum(abs(p[i] - bg[i]) for i in range(3)) > 120:
            print(x - 6, y0); sys.exit(0)
EOF
)
    if [ -z "$xy" ]; then fail "no See more button found in $cb"; else
        xdotool mousemove ${xy% *} ${xy#* }; sleep 0.3; xdotool mousemove $((${xy% *} + 1)) ${xy#* }; sleep 2.5
        "$WINE" 'C:\smberrors-probe.exe' tips > "$T/tips0" 2>/dev/null
        xdotool click 1; sleep 2
        "$WINE" 'C:\smberrors-probe.exe' tips > "$T/tips1" 2>/dev/null
        import -window root "$T/menu.png" 2>/dev/null
        tr -d '\r' < "$T/tips0" | sed 's/^/      hover: /'; tr -d '\r' < "$T/tips1" | sed 's/^/      menu: /'
        # a tooltip near the pointer (not the notification's balloon), and
        # whether any tooltip lies over the open menu
        near=$(tr -d '\r' < "$T/tips0" | awk -v x=${xy% *} -v y=${xy#* } '$1 == "TIP" && $2 < x + 80 && $4 > x - 80 && $3 < y + 60 && $5 > y - 10' | head -1)
        menu=$(tr -d '\r' < "$T/tips1" | grep -m1 '^MENU ')
        over=$(tr -d '\r' < "$T/tips1" | awk -v m="$menu" 'BEGIN { split(m, r, " ") } $1 == "TIP" && $2 < r[4] && $4 > r[2] && $3 < r[5] && $5 > r[3]' | head -1)
        if [ -z "$near" ]; then fail "hovering See more showed no tooltip (the gate cannot judge)"
        elif [ -z "$menu" ]; then fail "clicking See more opened no menu"
        elif [ -n "$over" ]; then fail "the See more tooltip stays over its open menu ($over over $menu)"
        else pass "See more: its tooltip ($near) goes when its menu opens ($menu)"; fi
        xdotool key Escape; sleep 1
        # a click straight away, before the tooltip's delay: the tooltip came
        # up after the menu, over its first item
        xdotool mousemove $((${xy% *} - 200)) $((${xy#* } + 300)); sleep 1
        xdotool mousemove ${xy% *} ${xy#* } click 1; sleep 2.5
        "$WINE" 'C:\smberrors-probe.exe' tips > "$T/tips2" 2>/dev/null
        import -window root "$T/menu2.png" 2>/dev/null
        tr -d '\r' < "$T/tips2" | sed 's/^/      quick click: /'
        menu=$(tr -d '\r' < "$T/tips2" | grep -m1 '^MENU ')
        over=$(tr -d '\r' < "$T/tips2" | awk -v m="$menu" 'BEGIN { split(m, r, " ") } $1 == "TIP" && $2 < r[4] && $4 > r[2] && $3 < r[5] && $5 > r[3]' | head -1)
        if [ -z "$menu" ]; then fail "a quick click on See more opened no menu"
        elif [ -n "$over" ]; then fail "clicked at once, the See more tooltip comes up over its menu ($over over $menu)"
        else pass "clicked at once: no tooltip comes up over the menu ($menu)"; fi
        xdotool key Escape
    fi
fi
[ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null
echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
