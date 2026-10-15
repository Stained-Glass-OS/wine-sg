#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# newdev.dll (patches/sg/5503): DiInstallDriver stages a driver package in the
# driver store (replacing a copy of the same package) and installs it on the
# devices present that it is a better match for -- or on all it matches with
# DIIRFLAG_FORCE_INF -- and gives Windows' errors (invalid flags, file not
# found, access denied for a non-administrator, ERROR_IN_WOW64 for a 32-bit
# caller); InstallSelectedDriver installs the selected driver. The device is a
# root-enumerated one the probe makes (test/newdev-probe.c), the packages two
# INFs of different rank. No display needed.
#
#   WINE=/opt/wine-sg/bin/wine test/newdev-gate.sh
# Mutants (newdev main.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_X"):
# SG_MUTANT_NO_ADMIN_CHECK, SG_MUTANT_NO_WOW64_CHECK, SG_MUTANT_NO_BETTER_MATCH,
# SG_MUTANT_NO_STAGE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-newdev.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix" "$T/work"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
x86_64-w64-mingw32-gcc -O2 -o "$T/nd64.exe" "$HERE/newdev-probe.c" -lsetupapi -ladvapi32 || { echo "FAIL  probe"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/nd32.exe" "$HERE/newdev-probe.c" -lsetupapi -ladvapi32 || { echo "FAIL  probe"; exit 1; }
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

W="Z:$(printf '%s' "$T/work" | sed 's#/#\\#g')"
out=$(DISPLAY= timeout -s KILL 200 "$WINE" "$T/nd64.exe" "$W" 2>/dev/null | tr -d '\r')
has() { printf '%s\n' "$out" | grep -qxF -- "$1"; }
chk() { # label expected-line
    if has "$2"; then pass "$1"; else fail "$1: wanted '$2', got '$(printf '%s' "$out" | grep -F "${2%% *}" | head -1)'"; fi
}
chk "64-bit: not a WoW64 process" "wow64=0"
chk "64-bit: the test device exists" "device=ok"
chk "64-bit: the prefix owner is an administrator" "is-admin=1"
chk "package A installed on the device it matches" "install-a ret=1 error=0 reboot=0 which=A inf=sga.inf"
chk "package A is in the driver store (system INF directory)" "staged-a=1"
chk "the same package again succeeds and stays the one copy" "install-a-again ret=1 error=0 reboot=0 which=A inf=sga.inf"
chk "still one copy of the package" "staged-a-after-replace=1"
chk "package B, a better match (first hardware id), replaces A on the device" "install-b-better ret=1 error=0 reboot=0 which=B inf=sgb.inf"
chk "package A, now a worse match, does not replace B" "install-a-worse ret=1 error=0 reboot=0 which=B inf=sgb.inf"
chk "DIIRFLAG_FORCE_INF installs A over B" "install-a-forced ret=1 error=0 reboot=0 which=A inf=sga.inf"
chk "a package matching no device is stored and succeeds" "install-nomatch ret=1 error=0 reboot=0 which=A inf=sga.inf"
chk "... and is in the driver store" "staged-nomatch=1"
chk "a missing INF: ERROR_FILE_NOT_FOUND" "missing ret=0 error=2 reboot=0 which=A inf=sga.inf"
chk "unknown flags: ERROR_INVALID_FLAGS" "bad-flags ret=0 error=1004 reboot=0 which=A inf=sga.inf"
chk "a NULL path: ERROR_INVALID_PARAMETER" "null-path ret=0 error=87"
chk "DIIRFLAG_INSTALL_AS_SET stages a directory of INFs, installs nothing" "install-set ret=1 error=0 reboot=0 which=A inf=sga.inf"
chk "... both INFs are in the driver store" "staged-set=1"
chk "the restricted token is not an administrator (the check below means something)" "restricted-is-admin=0"
chk "a non-administrator: ERROR_ACCESS_DENIED, the device is unchanged" "non-admin ret=0 error=5 reboot=0 which=A inf=sga.inf"
chk "InstallSelectedDriver installs the selected driver" "selected ret=1 error=0 reboot-flag=0 which=B"
chk "InstallSelectedDriver: Reserved must be NULL" "selected-reserved ret=0 error=87"

out=$(DISPLAY= timeout -s KILL 100 "$WINE" "$T/nd32.exe" "$W" 2>/dev/null | tr -d '\r')
chk "32-bit: a WoW64 process" "wow64=1"
chk "32-bit: DiInstallDriver fails with ERROR_IN_WOW64" "wow64-install ret=0 error=3758096949 reboot=0"
chk "32-bit: InstallSelectedDriver fails with ERROR_IN_WOW64" "wow64-selected ret=0 error=3758096949"

[ $RC = 0 ] && echo "newdev-gate: all passed" || echo "newdev-gate: FAILURES"
exit $RC
