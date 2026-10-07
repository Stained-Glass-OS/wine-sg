#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The WinRT settings Microsoft Edge asks for at start (patches/sg/1303):
# Windows.System.Profile.PlatformDiagnosticsAndUsageDataSettings and
# EducationSettings, Windows.System.UserProfile.AssignedAccessSettings and
# DiagnosticsSettings. Wine had none ("RoGetActivationFactory Failed to find
# library for ..." in David's Edge report, 2026-10-06). They answer as a
# machine with default settings does -- diagnostic data "Required" (Basic),
# not a school PC, not a kiosk, no tailored experiences -- and follow Group
# Policy's AllowTelemetry. Mutant: SG_MUTANT_NO_PRIVACY_SETTINGS
# (dlls/twinapi.appcore/main.c).
#
#   WINE=/opt/wine-sg/bin/wine test/privacysettings-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-privacysettings.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/privacysettings-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
run() { DISPLAY= timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r' > "$T/out"; sed 's/^/      /' "$T/out"; }
v() { sed -n "s/^$1 //p" "$T/out"; }
run
[ "$(v diag-factory)" = 00000000 ] && [ "$(v diag-level)" = "00000000 1" ] \
    && pass "PlatformDiagnosticsAndUsageDataSettings: CollectionLevel Basic (Required), the default" || fail "diag: $(v diag-factory) $(v diag-level)"
[ "$(v diag-can-basic)" = "00000000 1" ] && [ "$(v diag-can-full)" = "00000000 0" ] \
    && pass "...CanCollectDiagnostics: Basic yes, Full no" || fail "can collect: $(v diag-can-basic) / $(v diag-can-full)"
[ "$(v edu-factory)" = 00000000 ] && [ "$(v edu-is)" = "00000000 0" ] && pass "EducationSettings: not an education environment" || fail "edu: $(v edu-factory) $(v edu-is)"
[ "$(v kiosk-factory)" = 00000000 ] && [ "$(v kiosk-enabled)" = "00000000 0" ] && [ "$(v kiosk-single)" = "00000000 0" ] \
    && pass "AssignedAccessSettings.GetDefault: not a kiosk" || fail "kiosk: $(v kiosk-factory) $(v kiosk-default) $(v kiosk-enabled) $(v kiosk-single)"
[ "$(v tailor-factory)" = 00000000 ] && [ "$(v tailor-can)" = "00000000 0" ] \
    && pass "DiagnosticsSettings.GetDefault: no tailored experiences" || fail "tailor: $(v tailor-factory) $(v tailor-default) $(v tailor-can)"
# Group Policy: diagnostic data off (Security)
"$WINE" reg add 'HKLM\Software\Policies\Microsoft\Windows\DataCollection' /v AllowTelemetry /t REG_DWORD /d 0 /f >/dev/null 2>&1
run
[ "$(v diag-level)" = "00000000 0" ] && [ "$(v diag-can-basic)" = "00000000 0" ] \
    && pass "AllowTelemetry=0 by policy: CollectionLevel Security, Basic not allowed" || fail "policy: $(v diag-level) $(v diag-can-basic)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
