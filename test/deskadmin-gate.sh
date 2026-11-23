#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# A program that needs an administrator, double-clicked on the desktop, asks at once (patches/sg/8701).
# The desktop's thread (Explorer's) handed it to the broker through rundll32 shell32,SgElevate and
# waited up to 180 s for the broker's answer without taking any message. A new process's start
# sends to the desktop's window, so rundll32 -- and every program started meanwhile -- stood still
# until that wait gave up: the consent prompt came after exactly three minutes (LibreCAD's installer,
# qa4 2026-11-22). The wait now delivers the messages other threads send (sent messages only; posted
# and input messages stay queued), and the 180 s limit is unchanged.
# Here a standard user (SG_OTHER, as in elevreq-gate) runs Explorer's desktop on its own Xvfb, a
# requireAdministrator program is on its Desktop, and SG_ELEVATE is a stand-in broker client that
# records when it is reached: within seconds of the double-click, not three minutes.
#   WINE=/opt/wine-sg/bin/wine test/deskadmin-gate.sh
# Mutant: SG_MUTANT_WAIT_WITHOUT_SENT_MESSAGES (dlls/shell32/shlexec.c).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
DPY=${SG_GATE_DISPLAY:-:311}
LIMIT=${SG_DESKADMIN_LIMIT:-30}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
for t in "$MINGW" "$WINDRES" Xvfb xdotool import python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -e "/tmp/.X11-unix/X${DPY#:}" ] && { echo "SKIP: display $DPY is taken"; exit 77; }
W=$(mktemp -d /var/tmp/sg-deskadmin.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
XPID=
cleanup() {
    set +e
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" "$WINESERVER" -k 2>/dev/null
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    [ -n "$XPID" ] && sudo -n -u "$SG_OTHER" kill "$XPID" 2>/dev/null
    sleep 1
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp -r "$W/out/." "$ARTIFACTS"/ 2>/dev/null
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT INT TERM

cat > "$W/admin.manifest" <<EOM
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
<trustInfo xmlns="urn:schemas-microsoft-com:asm.v2"><security><requestedPrivileges>
<requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>
</requestedPrivileges></security></trustInfo></assembly>
EOM
printf '1 24 "%s"\n' "$W/admin.manifest" > "$W/admin.rc"
"$WINDRES" "$W/admin.rc" -O coff -o "$W/admin.res" && \
    TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/admin.exe" "$HERE/elevreq-probe.c" "$W/admin.res" -lshell32 -lole32 -luuid \
    || { echo "FAIL build"; exit 1; }
chmod 755 "$W/admin.exe"
TMPDIR=/var/tmp "$MINGW" -O2 -o "$W/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/vdesk-probe.exe"
mkdir -m 777 "$W/out"
# the stand-in broker client: when it was reached, and with what; it answers "launched"
cat > "$W/elevate" <<EOS
#!/bin/sh
[ "\$1" = --ready ] && { printf 0 > "\$2"; shift 2; }
[ "\$1" = --code ] && shift 2
printf '%s %s\n' "\$(date +%s)" "\$*" >> "$W/out/elevate.log"
exit 0
EOS
printf 'ready\n' > "$W/elevate.features"
chmod 755 "$W/elevate"

mkdir "$PFX"; chgrp "$SG_GROUP" "$PFX"; chmod 2770 "$PFX"; touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null
OH=$W/home-other
mkdir -m 777 "$OH"
sudo -n -u "$SG_OTHER" mkdir -p "$OH/Desktop" "$OH/run"
sudo -n -u "$SG_OTHER" chmod 700 "$OH/run"
sudo -n -u "$SG_OTHER" mkdir -p "$PFX/drive_c/users/$SG_OTHER/Desktop"
sudo -n -u "$SG_OTHER" cp "$W/admin.exe" "$PFX/drive_c/users/$SG_OTHER/Desktop/admin.exe"

other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
        HOME="$OH" XDG_RUNTIME_DIR="$OH/run" WAYLAND_DISPLAY=sg-gate-no-wayland DISPLAY="$DPY" \
        SG_FIREWALL_NOTIFY="$OH/no-firewall-notify" SG_POWERCTL="$OH/no-power-helper" \
        SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/elevate.features" "$@"
}
K='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\HideDesktopIcons\NewStartPanel'
other "$WINE" reg add "$K" /v '{645FF040-5081-101B-9F08-00AA002F954E}' /t REG_DWORD /d 1 /f >/dev/null 2>&1
other "$WINE" reg add "$K" /v '{20D04FE0-3AEA-1069-A2D8-08002B30309D}' /t REG_DWORD /d 1 /f >/dev/null 2>&1
other "$WINE" reg add "$K" /v '{59031a47-3f72-44a7-89c5-5595fe6b30ee}' /t REG_DWORD /d 1 /f >/dev/null 2>&1
other "$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
other "$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1

# the user's own X server, on this gate's display only
sudo -n -u "$SG_OTHER" Xvfb "$DPY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 &
sleep 2
XPID=$(pgrep -u "$SG_OTHER" -f "Xvfb $DPY " | head -1)
[ -n "$XPID" ] || { echo "FAIL no Xvfb on $DPY"; exit 1; }
other "$WINE" explorer /desktop=shell,1024x700 >"$W/out/explorer.out" 2>&1 &
sleep 8
# the desktop is drawn once something is on it: a window, away from the icons (desktop-icons-gate)
other "$WINE" "$W/vdesk-probe.exe" window Away 600 380 >/dev/null 2>&1 &
sleep 5
other import -window root "$W/out/desktop.png"
# the one icon in the first column: its middle
y=$(python3 - "$W/out/desktop.png" <<'PYEOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
bg = im.getpixel((500, 600))
rows = [y for y in range(0, 300) if sum(1 for x in range(4, 90) if im.getpixel((x, y)) != bg) > 3]
print((rows[0] + 20) if rows else 0)
PYEOF
)
[ "${y:-0}" -gt 0 ] && pass "the requireAdministrator program is on the desktop (y=$y)" || { fail "no icon on the desktop"; echo "deskadmin-gate: $fails failure(s)"; exit 1; }
t0=$(date +%s)
other xdotool mousemove 50 "$y" click --repeat 2 --delay 80 1
i=0
while [ ! -s "$W/out/elevate.log" ] && [ $i -lt "$LIMIT" ]; do sleep 1; i=$((i + 1)); done
log=$(cat "$W/out/elevate.log" 2>/dev/null)
if [ -n "$log" ]; then
    t1=${log%% *}
    case "$log" in *admin.exe*) pass "the double-click reached the elevation broker $((t1 - t0)) s later ($log)" ;;
        *) fail "the broker was reached with '$log'" ;; esac
else
    fail "no request reached the elevation broker within $LIMIT s of the double-click (the desktop thread held the new process)"
fi
# and the desktop still answers afterwards (a program started now runs)
out=$(other timeout 60 "$WINE" cmd /c echo desktop-alive 2>/dev/null | tr -d '\r')
[ "$out" = desktop-alive ] && pass "a program started afterwards runs" || fail "afterwards: '$out'"
echo "deskadmin-gate: $fails failure(s)"
exit $((fails > 0))
