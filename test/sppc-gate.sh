#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Software Licensing client's store (patches/sg/0491). Office's installer
# (Click-to-Run's integrator.exe /I /License) installs its license files with
# sppc's SLInstallLicense, which Wine lacked: the integrator aborted and
# Office stopped with 30015-11. Licenses and product keys are kept as given
# under an identifier made from their content (the same one again for the
# same content), and uninstalling removes them -- asking twice says they are
# not installed; what is installed reads back (0495). Nothing is granted:
# application policies hold nothing, no right is consumed, and the
# licensing status is what it was.
#
#   WINE=/opt/wine-sg/bin/wine test/sppc-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-sppc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
# sppc's exports, as the probe links them (mingw has no import library for it)
printf 'LIBRARY sppc.dll\nEXPORTS\nSLOpen\nSLClose\nSLInstallLicense\nSLUninstallLicense\nSLInstallProofOfPurchase\nSLUninstallProofOfPurchase\nSLGetLicensingStatusInformation\nSLGetLicenseFileId\nSLGetLicense\nSLGetPKeyId\nSLLoadApplicationPolicies\nSLGetApplicationPolicy\nSLUnloadApplicationPolicies\nSLGetPolicyInformationDWORD\nSLConsumeRight\n' > "$T/sppc.def"
"${DLLTOOL:-x86_64-w64-mingw32-dlltool}" -d "$T/sppc.def" -l "$T/libsppc.a" || { fail "no import library"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/sppc-probe.c" "$T/libsppc.a" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
STORE="$WINEPREFIX/drive_c/windows/system32/spp/sg-store"
ls "$STORE/licenses" > "$T/files" 2>&1
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v open)" = 00000000 ] && pass "SLOpen" || fail "open: $(v open)"
case "$(v install)" in "00000000 {"*) pass "SLInstallLicense installs a license: $(v install)" ;; *) fail "install: $(v install)" ;; esac
[ "$(v again)" = "$(v install)" ] && [ "$(v same)" = "1 different 1" ] \
    && pass "the same license again: the same identifier; another license: another" || fail "ids: $(v again) / $(v same)"
[ "$(v uninstall)" = 00000000 ] && [ "$(v uninstall-again)" = c004f011 ] \
    && pass "SLUninstallLicense removes it; again: SL_E_LICENSE_FILE_NOT_INSTALLED" || fail "uninstall: $(v uninstall) $(v uninstall-again)"
case "$(v key)" in "00000000 {"*) pass "SLInstallProofOfPurchase keeps a product key" ;; *) fail "key: $(v key)" ;; esac
[ "$(v unkey)" = 00000000 ] && [ "$(v unkey-again)" = c004f014 ] \
    && pass "SLUninstallProofOfPurchase removes it; again: SL_E_PKEY_NOT_INSTALLED" || fail "unkey: $(v unkey) $(v unkey-again)"
[ "$(v empty)" = 80070057 ] && pass "an empty license: E_INVALIDARG" || fail "empty: $(v empty)"
[ "$(v fileid)" = "00000000 1" ] && [ "$(v getlicense)" = "00000000 1" ] && [ "$(v fileid-gone)" = c004f011 ] \
    && pass "SLGetLicenseFileId and SLGetLicense read the store back (0495); gone once uninstalled" || fail "read back: $(v fileid) / $(v getlicense) / $(v fileid-gone)"
[ "$(v pkeyid)" = "00000000 1" ] && pass "SLGetPKeyId finds the installed key" || fail "pkeyid: $(v pkeyid)"
[ "$(v loadpolicies)" = "00000000 1" ] && [ "$(v policy)" = "c004f012 0 0000000000000000" ] && [ "$(v unloadpolicies)" = 00000000 ] \
    && pass "application policies load, and hold none (SL_E_VALUE_NOT_FOUND)" || fail "policies: $(v loadpolicies) / $(v policy) / $(v unloadpolicies)"
[ "$(v policydword)" = c004f012 ] && [ "$(v consume)" = c004f013 ] \
    && pass "no policy value, and no right is granted (SL_E_RIGHT_NOT_GRANTED)" || fail "policy dword / consume: $(v policydword) / $(v consume)"
[ "$(v status)" = c004f002 ] && pass "and nothing is granted: the licensing status is unchanged (SL_E_RIGHT_NOT_CONSUMED)" || fail "status: $(v status)"
[ "$(wc -l < "$T/files")" = 0 ] && pass "the store is empty again after uninstalling" || fail "store: $(cat "$T/files")"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
