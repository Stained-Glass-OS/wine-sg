#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# The display devices a session records stay writable by its user (patches/
# sg/0619). The GPU (Enum\PCI), the monitors (Enum\DISPLAY) and their device
# classes were read-only to users like the rest of HKLM once the machine
# stamped them: the desktop owner's display setup failed ("Failed to write
# gpu"), left a record that named a GPU it could not write, and every
# program then redid the display setup at every display call ("Failed to
# read display config", hundreds of times a program; 0 after this, in the QA
# VM). Here the prefix owner (the SYSTEM account) stamps those keys as the
# machine does at boot; the standard user must be able to record devices
# under them, and still not elsewhere in HKLM.
#
# This user owns the prefix; SG_OTHER (default sgconf, in SG_GROUP) is the
# standard user. Needs passwordless `sudo -u $SG_OTHER`.
#   WINE=... test/displaycfg-gate.sh
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
W=$(mktemp -d /var/tmp/displaycfg.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null
# a server started afresh, as at boot: keys come back from system.reg with no
# security descriptor, and the machine (SYSTEM) stamps them when it touches them
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
CS='HKLM\System\CurrentControlSet'
DISPCLASS='{4D36E968-E325-11CE-BFC1-08002BE10318}'
MONCLASS='{4D36E96E-E325-11CE-BFC1-08002BE10318}'
# the machine stamps them as it touches them (as SYSTEM: the prefix owner)
for k in "$CS\\Enum" "$CS\\Enum\\PCI" "$CS\\Enum\\DISPLAY" "$CS\\Control\\Class\\$DISPCLASS" "$CS\\Control\\Class\\$MONCLASS" 'HKLM\Software\SgGate'; do
    sg "$SG_GROUP" -c "umask 002; '$WINE' reg add '$k' /f" >/dev/null 2>&1
done
user() { sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all HOME=/var/tmp \
         WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" "$WINE" reg add "$1" /v Gate /d 1 /f >/dev/null 2>&1; }
for k in "$CS\\Enum\\PCI\\VEN_10DE&DEV_0001&SUBSYS_00000000&REV_00\\4&gate" "$CS\\Enum\\DISPLAY\\GATE001\\5&gate" \
         "$CS\\Control\\Class\\$DISPCLASS\\0007" "$CS\\Control\\Class\\$MONCLASS\\0007"; do
    user "$k" && pass "the standard user records a display device: ${k#"$CS\\"}" || fail "the standard user may not write ${k#"$CS\\"}"
done
user 'HKLM\Software\SgGate\Elsewhere' && fail "the standard user wrote elsewhere in HKLM (HKLM\\Software\\SgGate)" ||
    pass "and still not elsewhere in HKLM"
[ $fails = 0 ] && exit 0 || exit 1
