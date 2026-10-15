#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Machine keys and registry virtualization in a shared prefix (patches/sg/3764, SHIPPED HOLE).
# Keys loaded from system.reg have no security descriptor, and an object without one was open to
# everybody: any user could write HKLM\Software\...\Run, an autostart for every user. In a system prefix
# such a key is now checked against the machine default (SYSTEM and Administrators full, Users read),
# and an old program's denied write goes to HKCU\Software\Classes\VirtualStore, as on Windows.
# With the standard user $SG_USER1 and SYSTEM ($SG_SYSTEM, the prefix owner), the prefix restarted as at boot:
#   standard user: denied on keys of system.reg (Software, Microsoft, Run, Windows NT, Session Manager,
#     Wine), on keys with and without descriptors; allowed on HKCU and the display configuration keys;
#     a write-restricted token is denied; SYSTEM allowed
#   a 32-bit program without a manifest: its write under HKLM\Software goes to the store and reads back
#     (also below it, merged with the machine's keys and values), not under Windows, Windows NT, Classes,
#     System; another user does not see it
#   a 64-bit program, and a 32-bit one with requestedExecutionLevel: denied; SYSTEM's 32-bit program: no store
#   WINE=... WINESERVER=... test/hklmsd-gate.sh
# Mutants: server SG_MUTANT_MKEY_NODEFAULT, SG_MUTANT_MKEY_AU_WRITE; kernelbase SG_MUTANT_VIRT_ANY_LEVEL,
# SG_MUTANT_VIRT_64BIT, SG_MUTANT_VIRT_NOT_EXCLUDED, SG_MUTANT_VIRT_NO_STORE_READ.
# Needs passwordless sudo and the accounts of usersessions-gate.sh; exit 77 otherwise.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_USER2=${SG_USER2:-sgtest2}; SG_GROUP=${SG_GROUP:-sgwine}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs both mingw compilers"; exit 77; }
W=$(mktemp -d /var/tmp/hklmsd.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" XDG_RUNTIME_DIR="$W/run-$u" "$@"
}
cleanup() {
    set +e
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT

# probes: 64-bit token probe, the 32-bit legacy program as it is, with a manifest, and 64-bit
cat > "$W/manifest.xml" <<'EOM'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3"><security><requestedPrivileges>
    <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
  </requestedPrivileges></security></trustInfo>
</assembly>
EOM
echo '1 24 "manifest.xml"' > "$W/m.rc"
(cd "$W" && i686-w64-mingw32-windres m.rc m32.o) || { echo "FAIL manifest resource"; exit 1; }
TOK="-D'ARRAY_SIZE(x)=(sizeof(x)/sizeof((x)[0]))'"
x86_64-w64-mingw32-gcc -O1 -D'ARRAY_SIZE(x)=(sizeof(x)/sizeof((x)[0]))' -o "$W/tok.exe" "$HERE/hklmsd-tok-probe.c" -ladvapi32 -lshell32 || { echo "FAIL build"; exit 1; }
i686-w64-mingw32-gcc -O1 -o "$W/reg32.exe" "$HERE/hklmsd-virt-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
i686-w64-mingw32-gcc -O1 -o "$W/reg32m.exe" "$HERE/hklmsd-virt-probe.c" "$W/m32.o" -ladvapi32 || { echo "FAIL build"; exit 1; }
x86_64-w64-mingw32-gcc -O1 -o "$W/reg64.exe" "$HERE/hklmsd-virt-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }

for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u" "$W/run-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
# the machine server restarted as at boot: keys come from system.reg, without descriptors; then what
# sg-session's sg_protect_machine_registry makes
as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' cmd /c exit" >/dev/null 2>&1
for k in 'HKLM\Software\Policies' 'HKLM\Software\Microsoft\Windows\CurrentVersion\Policies' \
         'HKLM\System\CurrentControlSet\Control\Video' 'HKLM\System\CurrentControlSet\Control\GraphicsDrivers'; do
    as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 120 '$WINE' reg add '$k' /f" >/dev/null 2>&1
done
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
for u in "$SG_USER1" "$SG_USER2"; do
    as "$u" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
done
for e in tok reg32 reg32m reg64; do sudo -n install -o "$SG_SYSTEM" -g "$SG_GROUP" -m 755 "$W/$e.exe" "$PFX/drive_c/$e.exe"; done
run() { local u=$1; shift; as "$u" timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'; }
v() { printf '%s\n' "$1" | K="$2" awk 'BEGIN { k = ENVIRON["K"] } index($0, k " ") == 1 { print substr($0, length(k) + 2); exit }'; }

# --- a standard user, 64-bit program
out=$(run "$SG_USER1" 'C:\tok.exe' iw)
[ "$(v "$out" ADMINMEMBER)" = 0 ] && [ "$(v "$out" ELEVATED)" = 0 ] && pass "$SG_USER1 is a standard user (not an administrator, not elevated)" || fail "token of $SG_USER1: $out"
for k in 'Software\SgTokProbe' 'Software\Policies\SgTokProbe' 'System\CurrentControlSet\Services\SgTokProbe'; do
    [ "$(v "$out" "CREATE hklm $k ->")" = 5 ] && pass "...cannot create HKLM\\$k" || fail "created HKLM\\$k: $(v "$out" "CREATE hklm $k ->")"
done
for k in 'Software\Microsoft' 'Software\Microsoft\Windows NT\CurrentVersion' 'Software\Microsoft\Windows\CurrentVersion\Run' \
         'System\CurrentControlSet\Control\Session Manager' 'Software\Wine'; do
    [ "$(v "$out" "OPENW hklm $k ->")" = 5 ] && pass "...cannot open HKLM\\$k for writing" || fail "opened HKLM\\$k for writing: $(v "$out" "OPENW hklm $k ->")"
done
[ "$(v "$out" "CREATE hkcu Software\\SgTokProbe ->")" = 0 ] && pass "...writes HKCU" || fail "HKCU write: $out"
out=$(run "$SG_USER1" 'C:\tok.exe' disp)
[ "$(v "$out" "CREATE hklm System\\CurrentControlSet\\Control\\Video\\SgTokProbe ->")" = 0 ] && \
[ "$(v "$out" "SETVALUE hklm System\\CurrentControlSet\\Control\\GraphicsDrivers\\SgTokProbe ->")" = 0 ] && pass "...writes the display configuration keys" || fail "display config keys: $out"
# a restricted token's child process now and then ends early (seen with the machine busy, also before this
# patch): ask again until it has reported
for try in 1 2 3; do
    out=$(run "$SG_USER1" 'C:\tok.exe' restricted)
    [ -n "$(v "$out" "[write-restricted,Users] CREATE hklm Software\\SgTokProbe ->")" ] && break
done
[ "$(v "$out" "[write-restricted,Users] CREATE hklm Software\\SgTokProbe ->")" = 5 ] && pass "...a write-restricted token of it is denied too" || fail "write-restricted: $out"

# --- SYSTEM
out=$(run "$SG_SYSTEM" 'C:\tok.exe' iw)
[ "$(v "$out" ELEVATED)" = 1 ] && [ "$(v "$out" "CREATE hklm Software\\SgTokProbe ->")" = 0 ] && [ "$(v "$out" "OPENW hklm Software\\Microsoft\\Windows\\CurrentVersion\\Run ->")" = 0 ] \
    && pass "SYSTEM (elevated) writes HKLM\\Software and Run" || fail "SYSTEM: $out"

# --- legacy 32-bit program of the standard user: virtualized
out=$(run "$SG_USER1" 'C:\reg32.exe' keep)
chk() { [ "$(v "$out" "$1")" = "$2" ] && pass "$3" || fail "$3 (got '$(v "$out" "$1")')"; }
chk CREATE_HKLM_SOFTWARE 0 "32-bit program without manifest: creating HKLM\\Software\\X works (virtualized)"
chk SET_VALUE 0 "...setting a value works"
chk REOPEN_READ 0 "...HKLM\\Software\\X opens again"
chk READBACK "0 7" "...and the value reads back"
chk STORE_WOW 0 "...it is in HKCU\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Wow6432Node"
chk SET_EXISTING 0 "...a value written to HKLM\\Software\\Microsoft is accepted"
chk READ_VIRTUAL "0 5" "...and read back through HKLM"
chk SUBKEYS_SEEN 1 "...the machine's subkeys are still listed"
chk ENUM_SEES_VIRTUAL 1 "...the value enumerates"
chk CREATE_WINDOWSNT 5 "...Software\\Microsoft\\Windows NT is not virtualized"
chk CREATE_RUN 5 "...nor Software\\Microsoft\\Windows (Run)"
chk CREATE_CLASSES 5 "...nor Software\\Classes"
chk CREATE_SYSTEM 5 "...nor HKLM\\System"
chk CREATE_CHILD 0 "...a subkey of a virtualized key can be created"
chk READ_CHILD "0 9" "...and read back"
out2=$(run "$SG_USER2" 'C:\reg32.exe' peek)
[ "$(v "$out2" PEEK)" = 2 ] && pass "another user does not see it" || fail "$SG_USER2 sees the virtualized key: $out2"
out2=$(run "$SG_USER1" 'C:\reg32.exe' peek)
[ "$(v "$out2" PEEK)" = 0 ] && pass "the user sees it again in a new process" || fail "peek of $SG_USER1: $out2"

# --- not virtualized: manifest, 64-bit
for e in reg32m reg64; do
    out=$(run "$SG_USER1" "C:\\$e.exe")
    [ "$(v "$out" CREATE_HKLM_SOFTWARE)" = 5 ] && [ "$(v "$out" OPEN_EXISTING_WRITE)" = 5 ] && pass "$e: the write is denied, not virtualized" || fail "$e: $out"
done
# --- SYSTEM's 32-bit program writes the machine
out=$(run "$SG_SYSTEM" 'C:\reg32.exe')
chk CREATE_HKLM_SOFTWARE 0 "SYSTEM's 32-bit program writes HKLM"
chk STORE_WOW 2 "...and nothing goes to a store"

[ $fails = 0 ] && echo "hklmsd-gate: all passed" || echo "hklmsd-gate: $fails failure(s)"
exit $((fails != 0))
