#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The .NET Framework 3.5 as an optional feature (patches/sg/0827-0829): the
# questions installers of .NET 3.5 programs ask a system where 3.5 is a
# "Windows feature" -- WMI's Win32_OptionalFeature (NetFx3, InstallState 1),
# dism /online /get-featureinfo /featurename:NetFx3 ("State : Enabled"),
# dism /enable-feature, fondue.exe /enable-feature:NetFx3 (exit 0) -- are
# answered from the .NET Framework's own registration (Wine Mono's support
# package writes NDP\v3.5 Install=1): enabled when it is there, disabled and
# not installable when it is not. Before, the class did not exist, dism was a
# stub printing nothing and fondue.exe did not exist, so 3.5 was "missing"
# and Microsoft's 3.5 setup ran, which says to use "Turn Windows features on
# or off" (Meedio/MeediOS, David 2026-10-05).
#
#   WINE=/opt/wine-sg/bin/wine test/netfx3-gate.sh
#   mutants: SG_MUTANT_OPTIONALFEATURE (dlls/wbemprox/builtin.c),
#            SG_MUTANT_DISM_NETFX3 (programs/dism/dism.c),
#            SG_MUTANT_FONDUE (programs/fondue/fondue.c)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-netfx3.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
w() { timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
mkdir -p "$WINEPREFIX/drive_c/t"
cat > "$WINEPREFIX/drive_c/t/of.vbs" <<'VBS'
Set items = GetObject("winmgmts:\\.\root\cimv2").ExecQuery("SELECT * FROM Win32_OptionalFeature WHERE Name='NetFx3'")
For Each f In items
  WScript.Echo "feature " & f.Name & " " & f.InstallState
Next
WScript.Echo "end"
VBS
NDP='HKLM\Software\Microsoft\NET Framework Setup\NDP\v3.5'

# 1. no .NET Framework registered: disabled, cannot be turned on
w reg delete "$NDP" /f /reg:64 >/dev/null; w reg delete "$NDP" /f /reg:32 >/dev/null
w dism /online /get-featureinfo /featurename:NetFx3 > "$T/info0"
grep -q '^State : Disabled$' "$T/info0" && pass "without .NET 3.5 registered: dism says State : Disabled" \
    || fail "without .NET 3.5: dism said: $(tr '\n' '|' < "$T/info0")"
timeout -s KILL 120 "$WINE" dism /online /enable-feature /featurename:NetFx3 >/dev/null 2>&1; rc=$?
[ "$rc" != 0 ] && pass "without it: dism /enable-feature fails (exit $rc)" || fail "without it: /enable-feature succeeded"
timeout -s KILL 120 "$WINE" fondue /enable-feature:NetFx3 /hide-ux:all >/dev/null 2>&1; rc=$?
[ "$rc" != 0 ] && pass "without it: fondue fails (exit $rc)" || fail "without it: fondue said the feature is on"
w cscript //nologo 'C:\t\of.vbs' > "$T/wmi0"
grep -q '^feature NetFx3 2$' "$T/wmi0" && pass "without it: Win32_OptionalFeature NetFx3 InstallState 2 (disabled)" \
    || fail "without it: WMI said: $(tr '\n' '|' < "$T/wmi0")"

# 2. registered as Wine Mono's support package registers it: enabled
w reg add "$NDP" /v Install /t REG_DWORD /d 1 /f /reg:64 >/dev/null
w reg add "$NDP" /v Install /t REG_DWORD /d 1 /f /reg:32 >/dev/null
w dism /online /get-featureinfo /featurename:NetFx3 > "$T/info1"; 
grep -q '^Feature Name : NetFx3$' "$T/info1" && grep -q '^State : Enabled$' "$T/info1" &&
grep -q '^The operation completed successfully.$' "$T/info1" && pass "dism /get-featureinfo NetFx3: State : Enabled" \
    || fail "dism /get-featureinfo said: $(tr '\n' '|' < "$T/info1")"
w dism /Online /Get-Features /Format:Table > "$T/list"
grep -q '^NetFx3  *| Enabled$' "$T/list" && pass "dism /get-features /format:table lists NetFx3 Enabled" \
    || fail "dism /get-features: $(tr '\n' '|' < "$T/list")"
timeout -s KILL 120 "$WINE" dism /online /enable-feature /featurename:NetFx3 /all /norestart >/dev/null 2>&1; rc=$?
[ "$rc" = 0 ] && pass "dism /enable-feature NetFx3 /all succeeds (it is on)" || fail "dism /enable-feature exit $rc"
timeout -s KILL 120 "$WINE" dism /online /get-featureinfo /featurename:NoSuchFeature >/dev/null 2>&1; rc=$?
[ "$rc" != 0 ] && pass "an unknown feature is an error (exit $rc)" || fail "an unknown feature succeeded"
timeout -s KILL 120 "$WINE" 'C:\windows\syswow64\dism.exe' /online /get-featureinfo /featurename:netfx3 2>/dev/null | tr -d '\r' > "$T/info32"
grep -q '^State : Enabled$' "$T/info32" && pass "32-bit dism, any case: State : Enabled" || fail "32-bit dism: $(tr '\n' '|' < "$T/info32")"
timeout -s KILL 120 "$WINE" fondue /enable-feature:NetFx3 /caller-name:gate /hide-ux:all >/dev/null 2>&1; rc=$?
[ "$rc" = 0 ] && pass "fondue /enable-feature:NetFx3 exits 0" || fail "fondue exit $rc"
w cscript //nologo 'C:\t\of.vbs' > "$T/wmi1"
grep -q '^feature NetFx3 1$' "$T/wmi1" && pass "Win32_OptionalFeature WHERE Name='NetFx3': InstallState 1 (enabled)" \
    || fail "WMI said: $(tr '\n' '|' < "$T/wmi1")"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
