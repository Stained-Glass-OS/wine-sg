#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A sandbox reads HKEY_CLASSES_ROOT after the shell's SYSTEM desktop process
# used COM (wine-sg 7805; David 2026-11-15: Windows Chrome had no sound).
# On a shared prefix the shell's desktop process on the SYSTEM account (an
# elevated program's display) takes Wine's admin token
# (ProcessWineGrantAdminToken). That token's default DACL was upstream's --
# Local System and Domain Users full -- and the first key it opened through
# COM, \Registry\Machine\Software\Classes (combase creates it to reach it;
# keys loaded from system.reg have no descriptor, and SYSTEM stamps one on
# them), got it: HKLM\Software\Classes writable by every user and readable by
# no sandbox (a restricted token keeps Domain Users only for deny). Chrome's
# audio service (USER_LIMITED) then could not open HKCR (ERROR_INVALID_HANDLE)
# and CoCreateInstance(MMDeviceEnumerator) failed (REGDB_E_READREGDB).
# A standard user (with a per-user install's HKCU\Software\Classes) runs a
# USER_LIMITED low-integrity child (comrestrict-probe.c) before and after a
# SYSTEM process with the admin token uses COM; checked after: the child opens
# HKCR\CLSID\{BCDE0395...}\InprocServer32 and the user's own ProgID through
# HKCR, CoCreateInstance(MMDeviceEnumerator) succeeds, and the key's DACL
# gives Domain Users no more than read.
# Needs SG_SYSTEM (prefix owner) and SG_USER1 in SG_GROUP with passwordless
# sudo (as hklmsd-gate.sh); exit 77 otherwise.
#   WINE=/opt/wine-sg/bin/wine test/comrestrict-gate.sh
# Mutant: SG_MUTANT_ADMIN_TOKEN_UPSTREAM_DACL (server/token.c) fails it.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="$(sg_wineserver)"
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_GROUP=${SG_GROUP:-sgwine}
for u in "$SG_SYSTEM" "$SG_USER1"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: no mingw"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
W=$(mktemp -d /var/tmp/sg-comrestrict.XXXXXX); chmod 755 "$W"; PFX=$W/prefix
as() {
    _u=$1; shift
    _ws=; [ "$_u" = "$SG_SYSTEM" ] && _ws='__wineservice_winstation\Default'
    sudo -n -u "$_u" env ${_ws:+SG_WINSTATION="$_ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$_u" XDG_RUNTIME_DIR="$W/run-$_u" "$@"
}
trap 'as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1; [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$W/comr.exe" "$HERE/comrestrict-probe.c" -lole32 -ladvapi32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
for u in "$SG_SYSTEM" "$SG_USER1"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u" "$W/run-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; DISPLAY= timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
# the machine server restarted as at boot: keys from system.reg, without descriptors
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; DISPLAY= timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/comr.exe" "$PFX/drive_c/comr.exe"
user() { as "$SG_USER1" timeout -s KILL 120 "$WINE" 'C:\comr.exe' "$@" 2>&1 | tr -d '\r'; }
system() { as "$SG_SYSTEM" sh -c "DISPLAY= timeout -s KILL 120 '$WINE' 'C:\\comr.exe' $*" 2>&1 | tr -d '\r'; }
v() { printf '%s\n' "$1" | awk -F= -v k="$2" '$1 == k { print substr($0, length(k) + 2); exit }'; }

user setup >/dev/null
o=$(user run)
[ "$(v "$o" hkcr_clsid)" = 0 ] && [ "$(v "$o" mmdevenum)" = 0 ] && pass "a sandbox reads HKCR and makes MMDeviceEnumerator at first" \
    || fail "at first: $(echo "$o" | tr '\n' ' ')"
s=$(system systemcom)
[ "$(v "$s" grant)" = 0 ] || echo "NOTE  the SYSTEM process did not get the admin token: $(echo "$s" | tr '\n' ' ')"
sd=$(v "$(user sd)" classes_sd)
case "$sd" in
*"(A;;GA;;;S-1-5-21-0-0-0-513)"*) fail "after SYSTEM's COM, HKLM\\Software\\Classes: every user may write it ($sd)" ;;
*"(A;;GR;;;BU)"*) pass "after SYSTEM's COM, HKLM\\Software\\Classes: users and sandboxes read, administrators write ($sd)" ;;
*) fail "after SYSTEM's COM, HKLM\\Software\\Classes: $sd (want BU/RC/AC read, Domain Users no more than read)" ;;
esac
o=$(user run)
[ "$(v "$o" hkcr_clsid)" = 0 ] && [ "$(v "$o" hkcr_user_progid)" = 0 ] && [ "$(v "$o" mmdevenum)" = 0 ] \
    && pass "and a sandbox still reads HKCR (the machine's class and the user's ProgID) and makes MMDeviceEnumerator" \
    || fail "after SYSTEM's COM, the sandbox: $(echo "$o" | tr '\n' ' ') (want hkcr_clsid=0 hkcr_user_progid=0 mmdevenum=0)"
[ $RC = 0 ] && echo "comrestrict: all passed"
exit $RC
