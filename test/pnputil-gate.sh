#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# pnputil.exe (patches/sg/5522): /add-driver (a file, a pattern, /subdirs,
# /install), /enum-drivers, /delete-driver, /scan-devices, and the device
# commands failing instead of pretending. The driver packages are generated
# INFs (no hardware matches them); the driver store is the prefix's
# %windir%\inf.
#
#   WINE=/opt/wine-sg/bin/wine test/pnputil-gate.sh
# Mutant (programs/pnputil/main.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_PNPUTIL_NOSUMMARY"):
# /add-driver prints no summary and always succeeds.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-pnputil.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix" "$T/pkgs/sub"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"

mkinf() { # path hwid version
    cat > "$1" <<EOS
[Version]
Signature="\$Windows NT\$"
Class=System
ClassGuid={4d36e97e-e325-11ce-bfc1-08002be10318}
Provider=%Prov%
DriverVer=01/02/2024,$3
[Manufacturer]
%Mfg%=Models,NTamd64,NTx86
[Models.NTamd64]
%Dev%=Install,$2
[Models.NTx86]
%Dev%=Install,$2
[Install]
AddReg=Reg
[Reg]
HKR,,Which,,"$2"
[Strings]
Prov="Stained Glass"
Mfg="Stained Glass"
Dev="SG test device"
EOS
}
mkinf "$T/pkgs/one.inf" 'SG\PNPUTIL_ONE' 1.0.0.1
mkinf "$T/pkgs/two.inf" 'SG\PNPUTIL_TWO' 1.0.0.2
mkinf "$T/pkgs/sub/three.inf" 'SG\PNPUTIL_THREE' 1.0.0.3

DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
INFDIR="$WINEPREFIX/drive_c/windows/inf"
STORE="$WINEPREFIX/drive_c/windows/system32/driverstore/filerepository"
oemcount() { ls "$STORE" 2>/dev/null | grep -c '\.inf_'; }
run() { DISPLAY= timeout -s KILL 120 "$WINE" pnputil "$@" > "$T/out" 2> "$T/err"; RET=$?; tr -d '\r' < "$T/out" > "$T/o"; tr -d '\r' < "$T/err" > "$T/e"; }

BASE=$(oemcount)   # Wine's own packages are in the store from the prefix's creation
[ "$BASE" -gt 0 ] && ! ls "$STORE" | grep -qi '^one\.inf_' && pass "the store has Wine's own packages ($BASE) and not ours" || fail "store at start: $BASE"

run /add-driver "Z:$T/pkgs/one.inf"
if [ $RET = 0 ] && grep -q "Driver package added successfully" "$T/o" && grep -q "^Published Name: *one\.inf" "$T/o" && grep -q "Added driver packages:  1" "$T/o"
then pass "/add-driver FILE stages the package and says its published name"; else fail "add one ($RET): $(cat "$T/o" "$T/e")"; fi
[ "$(oemcount)" = $((BASE + 1)) ] && pass "the package is in the driver store" || fail "store has $(oemcount) packages, was $BASE"
PUB=$(sed -n 's/^Published Name: *//p' "$T/o" | head -1)

run /enum-drivers
if grep -q "^Published Name: *$PUB" "$T/o" && grep -q "Provider Name: *Stained Glass" "$T/o" && grep -q "Class Name: *System" "$T/o" && grep -q "Driver Version: *01/02/2024,1.0.0.1" "$T/o"
then pass "/enum-drivers lists it with provider, class and version"; else fail "enum: $(cat "$T/o" "$T/e")"; fi

run /add-driver "Z:$T/pkgs/*.inf"
if [ $RET = 0 ] && grep -q "Total driver packages:  2" "$T/o" && grep -q "Added driver packages:  2" "$T/o"; then pass "a pattern adds every matching package"; else fail "pattern ($RET): $(cat "$T/o" "$T/e")"; fi
run /add-driver "Z:$T/pkgs/*.inf" /subdirs
grep -q "Total driver packages:  3" "$T/o" && pass "/subdirs goes down into directories" || fail "subdirs: $(cat "$T/o" "$T/e")"

run /add-driver "Z:$T/pkgs/one.inf" /install
if [ $RET = 0 ] && grep -q "Driver package added successfully" "$T/o"; then pass "/add-driver /install succeeds with no matching device (the package is stored)"; else fail "install ($RET): $(cat "$T/o" "$T/e")"; fi

run /add-driver "Z:$T/pkgs/nothing.inf"
[ $RET != 0 ] && pass "a file that is not there fails" || fail "missing file: rc 0"

run /delete-driver "$PUB"
if [ $RET = 0 ] && grep -q "Driver package deleted successfully" "$T/o"; then pass "/delete-driver removes a package"; else fail "delete ($RET): $(cat "$T/o" "$T/e")"; fi
[ ! -e "$INFDIR/$PUB" ] && [ "$(ls "$STORE" 2>/dev/null | grep -c "^$PUB")" = 0 ] && pass "and its INF and its store directory are gone" || fail "$PUB still there"
run /delete-driver "$PUB"
[ $RET != 0 ] && pass "deleting it again fails" || fail "delete twice: rc 0"
run /delete-driver

[ $RET != 0 ] && pass "no name: an error" || fail "delete without a name: rc 0"

run /scan-devices
[ $RET = 0 ] && pass "/scan-devices succeeds" || fail "scan ($RET)"
run /disable-device 'ROOT\SYSTEM\0000'
[ $RET != 0 ] && grep -q "not supported" "$T/e" && pass "a device command fails rather than pretending" || fail "disable ($RET): $(cat "$T/e")"
run
[ $RET != 0 ] && pass "no command: usage and failure" || fail "no command: rc 0"

[ "$RC" = 0 ] && echo "pnputil-gate: all passed" || echo "pnputil-gate: FAILURES"
exit $RC
