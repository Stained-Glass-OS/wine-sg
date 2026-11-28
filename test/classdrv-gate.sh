#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A VPN's network adapter, made as OpenVPN's installer makes it (patches/sg/8423; i1014: the MSI
# ended with exit 67 at Wintun's driver step, then at its adapters):
#  1. the Net device setup class by name and GUID (SetupDiClassNameFromGuid, ...GuidsFromName)
#  2. a Net class driver list (SPDIT_CLASSDRIVER) builds, with no device and nothing of ours in the
#     driver store, then lists a driver package copied into the store (SetupCopyOEMInf) and not
#     after it is uninstalled (SetupUninstallOEMInf) -- Wintun's installer removes its old drivers so
#  3. a ROOT\NET device for a hardware ID, registered, gets the store's best driver from
#     DiInstallDevice (was a stub), and the network class installer's NetCfgInstanceId and connection
#     (PnpInstanceID, a name "Ethernet ...") -- what tapctl waits for
#  4. netsh interface set interface name=... newname=... renames the connection
#  5. class install parameters are kept (Set/GetClassInstallParams; the set one was a stub): a
#     DIF_PROPERTYCHANGE disable leaves the device CM_PROB_DISABLED (SetupDiChangeState); DIF_REMOVE
#     removes it and its connection
# The probe is test/classdrv-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/classdrv-gate.sh
# Mutants (dlls/setupapi/devinst.c): SG_MUTANT_NO_CLASS_DRIVERS, SG_MUTANT_NO_NETCFG_INSTANCE,
#   SG_MUTANT_CLASSINSTALLPARAMS_STUB, SG_MUTANT_NO_CHANGESTATE; (dlls/newdev/main.c)
#   SG_MUTANT_DIINSTALLDEVICE_NO_BEST; (programs/netsh/netsh.c) SG_MUTANT_NETSH_NO_RENAME
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-classdrv.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}"
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/classdrv-probe.c" -lsetupapi -lcfgmgr32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1 //p" | head -1; }

[ "$(v CLASSNAME)" = "1 Net" ] && [ "$(v CLASSGUID)" = "1 1" ] && pass "the Net setup class, by GUID and by name" \
    || fail "Net class: name $(v CLASSNAME), guid $(v CLASSGUID)"
[ "$(v BEFORE)" = 0 ] && pass "a Net class driver list builds with none of ours in the store" || fail "class driver list: $(v BEFORE) $(v BUILD_ERR)"
case "$(v STORED)" in "1 "*" types 1") pass "...and lists the package copied into the store, as class drivers" ;;
    *) fail "class driver list after SetupCopyOEMInf: $(v STORED)" ;; esac
[ "$(v UNINSTALL)" = 1 ] && [ "$(v AFTER)" = 0 ] && pass "...and not once it is uninstalled" \
    || fail "uninstall $(v UNINSTALL), after $(v AFTER)"
[ "$(v REGISTER)" = 1 ] && [ "$(v DIINSTALL)" = "1 err 0 reboot 0" ] && pass "DiInstallDevice installs the store's best driver on a new ROOT\\NET device" \
    || fail "register $(v REGISTER), DiInstallDevice $(v DIINSTALL)"
[ "$(v NETCFG)" = "1 connection 1 named 1" ] && pass "...which gets a NetCfgInstanceId and its connection (PnpInstanceID, an Ethernet name)" \
    || fail "network class installer: $(v NETCFG)"
[ "$(v NETSH)" = "0 renamed 1" ] && pass "netsh renames the connection" || fail "netsh rename: $(v NETSH)"
[ "$(v PARAMS)" = 1 ] && pass "class install parameters are kept" || fail "class install parameters: $(v PARAMS)"
[ "$(v DISABLE)" = "1 problem 22" ] && pass "DIF_PROPERTYCHANGE disables the device (CM_PROB_DISABLED)" || fail "disable: $(v DISABLE)"
[ "$(v REMOVE)" = "1 connection_gone 1" ] && pass "DIF_REMOVE removes the device and its connection" || fail "remove: $(v REMOVE)"
exit $RC
