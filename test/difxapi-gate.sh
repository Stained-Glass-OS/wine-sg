#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# difxapi.dll (patches/sg/5504): DriverPackagePreinstall / Install / Uninstall /
# GetPath (A and W), the log callbacks and INSTALLERINFO, as Microsoft's pages
# describe them, against a root-enumerated test device of the probe's own and
# small driver packages (test/difxapi-probe.c): the checks and their error codes,
# the driver store, installing where the package is a better match (or forced),
# preinstalled-but-not-installed when no device matches, applications associated
# with a package and kept while another one needs it, GetPath's buffer rules, the
# event log callback, a non-administrator, a 32-bit caller (ERROR_IN_WOW64).
# The catalogs are well formed and not signed: their signatures are not checked
# (see patches/notes/5504.txt). No display needed.
#
#   WINE=/opt/wine-sg/bin/wine test/difxapi-gate.sh
# Mutants (difxapi main.c; x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_X"):
# SG_MUTANT_NO_ADMIN_CHECK, SG_MUTANT_NO_WOW64_CHECK, SG_MUTANT_NO_CATALOG_CHECK,
# SG_MUTANT_NO_APP_ASSOC, SG_MUTANT_NO_LOG, SG_MUTANT_NO_FORCE
# (and newdev's, which this gate also exercises).
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

T=$(mktemp -d /var/tmp/sg-difxapi.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix" "$T/work"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
for a in 64:x86_64 32:i686; do
    bits=${a%%:*}; cc=${a##*:}-w64-mingw32-gcc
    "$cc" -O2 -o "$T/dx$bits.exe" "$HERE/difxapi-probe.c" -lsetupapi -ladvapi32 -lwintrust -lcrypt32 ||
        { echo "FAIL  the $bits-bit probe did not build"; exit 1; }
done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

W="Z:$(printf '%s' "$T/work" | sed 's#/#\\#g')"
out=$(DISPLAY= timeout -s KILL 250 "$WINE" "$T/dx64.exe" "$W" 2>/dev/null | tr -d '\r')
chk() { # label expected-line
    if printf '%s\n' "$out" | grep -qxF -- "$2"; then pass "$1"; else
        fail "$1: wanted '$2', got '$(printf '%s\n' "$out" | grep -F -m1 -- "${2%% *}")'"; fi
}
chk "64-bit: not WoW64" "wow64=0"
chk "the test device exists, the owner is an administrator" "device=ok"
chk "is-admin" "is-admin=1"

# the arguments and the path
chk "Preinstall(NULL): ERROR_INVALID_PARAMETER" "pre-null 0x57"
chk "an unknown flag: ERROR_INVALID_PARAMETER" "pre-badflags 0x57"
chk "DRIVER_PACKAGE_DELETE_FILES is for Uninstall only" "pre-delete-files-flag 0x57"
chk "a relative path: ERROR_INVALID_NAME" "pre-relative 0x7b"
chk "a missing INF: ERROR_FILE_NOT_FOUND" "pre-missing 0x2"
chk "a path past the maximum: ERROR_FILENAME_EXCED_RANGE" "pre-toolong 0xce"
chk "an INF in the system INF directory: ERROR_CANT_ACCESS_FILE" "pre-in-system-inf 0x780"
chk "no catalog named: TRUST_E_NOSIGNATURE" "pre-unsigned 0x800b0100"
chk "a catalog named and missing: CRYPT_E_FILE_ERROR" "pre-catalog-missing 0x80092003"
chk "not a PnP function driver: ERROR_INVALID_FUNCTION" "pre-not-pnp 0x1"

# preinstall and the driver store
chk "Preinstall of a package succeeds, installs on no device" "pre-a 0 which=-"
chk "GetPath gives the INF in the driver store" "getpath-a 0 filerepository=1 endswith-sga=1 count-is-len+1=1"
chk "GetPath with no buffer: ERROR_INSUFFICIENT_BUFFER and the size" "getpath-size 0x7a count-is-len+1=1"
chk "GetPath with a small buffer: ERROR_INSUFFICIENT_BUFFER" "getpath-small 0x7a count-is-len"
chk "Preinstall again: ERROR_ALREADY_EXISTS" "pre-a-again 0xb7"
chk "... with DRIVER_PACKAGE_REPAIR it succeeds" "pre-a-repair 0"
chk "... DRIVER_PACKAGE_SILENT alone is still ALREADY_EXISTS" "pre-a-silent 0xb7"
chk "ONLY_IF_DEVICE_PRESENT and no device: ERROR_NO_SUCH_DEVINST" "pre-only-if-present-nodev 0xe000020b"
chk "... and the package was not stored" "getpath-nodev-not-stored 0xe0000302"
chk "Preinstall of a package for an absent device succeeds" "pre-n 0"
chk "... and it is in the store" "getpath-nodev-stored 0"
chk "GetPath of a missing INF: ERROR_FILE_NOT_FOUND" "getpath-missing 0x2"
chk "GetPath of an INF in the system INF directory: ERROR_CANT_ACCESS_FILE" "getpath-system-inf 0x780"
chk "GetPath with no size pointer: ERROR_INVALID_PARAMETER" "getpath-null-count 0x57"

# install
chk "INSTALLERINFO with an empty string: ERROR_INVALID_PARAMETER" "bad-installerinfo 0x57"
chk "Install installs on the device it matches" "install-a 0 reboot=0 which=A"
chk "a better match replaces it" "install-b-better 0 which=B"
chk "a worse match: ERROR_NO_MORE_ITEMS, the device keeps B" "install-a-worse 0x103 which=B"
chk "DRIVER_PACKAGE_FORCE installs it anyway" "install-a-forced 0 which=A"
chk "ONLY_IF_DEVICE_PRESENT with FORCE installs on the device" "install-only-if-present-forced-a 0 which=A"
chk "no device matches: ERROR_NO_SUCH_DEVINST (preinstalled)" "install-nodev 0xe000020b"
chk "an unsigned package: TRUST_E_NOSIGNATURE" "pre-unsigned-install 0x800b0100"
chk "ONLY_IF_DEVICE_PRESENT, not a better match: ERROR_NO_MORE_ITEMS" "pre-only-if-present-worse 0x103"
chk "the ANSI forms: Preinstall" "pre-a-ansi 0xb7"
chk "the ANSI forms: Install" "install-ansi-forced 0 which=A"
chk "the ANSI forms: GetPath" "getpath-ansi 0 endswith-sga=1 count-is-len+1=1"

# applications associated with a package
chk "an application installs a package" "app1-install 0xe000020b"
chk "a second application installs it too" "app2-install 0xe000020b"
chk "the first uninstalls: ERROR_DEPENDENT_APPLICATIONS_EXIST" "app1-uninstall-others-remain 0xe0000300"
chk "... the package is still in the store" "still-in-store 0"
chk "... and the first is no longer associated, the second still is" "app1-uninstall-again-others-remain 0xe0000300"
chk "Uninstall with an empty INSTALLERINFO string: ERROR_INVALID_PARAMETER" "uninstall-bad-installerinfo 0x57"
chk "Uninstall with a flag it does not take: ERROR_INVALID_PARAMETER" "uninstall-bad-flags 0x57"
chk "the last application uninstalls: the package goes" "app2-uninstall-last 0"
chk "... it is not in the store" "gone-from-store 0xe0000302"
chk "Uninstall of a package not in the store: ERROR_DRIVER_PACKAGE_NOT_IN_STORE" "uninstall-not-in-store 0xe0000302"
chk "Uninstall of a missing INF: ERROR_FILE_NOT_FOUND" "uninstall-missing-file 0x2"
chk "DRIVER_PACKAGE_FORCE uninstalls although another application needs it" "uninstall-force-with-others 0"
chk "... it is gone" "gone-after-force 0xe0000302"
chk "a package no application is associated with: installs" "plain-install 0xe000020b"
chk "... uninstalls" "plain-uninstall 0"

# the log callback
chk "the log callback gets events, a success event, the INF's path and its context" \
    "log-events-success=1 success-event=1 path-in-text=1 ctx=1"
chk "a failing call logs an error event with the error" "log-error-event=1 error-code=2"
chk "registering another callback replaces the first" "log-replaced: old=0 new=1"
chk "SetDifxLogCallback replaces it too" "log-stdcall-replaced: old=0 new=1"
chk "NULL unregisters" "log-unregistered: events=0"

# a user who is not an administrator
chk "the restricted token is not an administrator (the checks below mean something)" "restricted-is-admin=0"
chk "Preinstall: ERROR_ACCESS_DENIED" "user-preinstall 0x5"
chk "Install: ERROR_ACCESS_DENIED" "user-install 0x5"
chk "Uninstall: ERROR_ACCESS_DENIED" "user-uninstall 0x5"
chk "GetPath: ERROR_ACCESS_DENIED" "user-getpath 0x5"

out=$(DISPLAY= timeout -s KILL 100 "$WINE" "$T/dx32.exe" "$W" 2>/dev/null | tr -d '\r')
chk "32-bit: a WoW64 process" "wow64=1"
chk "32-bit: Preinstall: ERROR_IN_WOW64" "wow64-preinstall 0xe0000235"
chk "32-bit: Install: ERROR_IN_WOW64" "wow64-install 0xe0000235"
chk "32-bit: Uninstall: ERROR_IN_WOW64" "wow64-uninstall 0xe0000235"
chk "32-bit: GetPath: ERROR_IN_WOW64" "wow64-getpath 0xe0000235"

[ $RC = 0 ] && echo "difxapi-gate: all passed" || echo "difxapi-gate: FAILURES"
exit $RC
