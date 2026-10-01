#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A scanner's INF installs (patches/sg/0641, 0642). Scanner INFs say
# "Include=sti.inf / Needs=STI.USBSection" and, in .Services,
# "Needs=STI.USBSection.Services" (the usbscan driver), and carry the still
# image class's SubClass/DeviceType/DeviceData/Events lines. Wine read
# neither Include/Needs nor the class's lines, had no sti.inf, and the Ambir
# ImageScan Pro 490i's library said "not connected".
#
#   WINE=/opt/wine-sg/bin/wine test/stiinf-gate.sh
# Mutation: -DSG_MUTANT_NONEEDS (Include/Needs ignored): service, fromneeds
# and plainneeded fail; -DSG_MUTANT_NOSTI (no class directives): devicetype,
# twainds and event fail.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-stiinf.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -municode -o "$T/sti.exe" "$HERE/stiinf-probe.c" -lsetupapi -ladvapi32 ||
    { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w

INF="$WINEPREFIX/drive_c/windows/inf"
[ -f "$INF/sti.inf" ] && pass "sti.inf is in %windir%\\inf" || fail "no sti.inf in %windir%\\inf"
DRV="$WINEPREFIX/drive_c/sgprobe"
mkdir -p "$DRV"
cat > "$INF/sgneeds.inf" <<'EOF'
[Version]
Signature="$CHICAGO$"

[SgNeeded]
AddReg=SgNeeded.Reg

[SgNeeded.Reg]
HKR,,FromInclude,,"yes"

[SgPlainNeeded]
AddReg=SgPlainNeeded.Reg

[SgPlainNeeded.Reg]
HKCU,Software\SgStiProbe,Needed,,"yes"
EOF
cat > "$DRV/probe.inf" <<'EOF'
[Version]
Signature="$CHICAGO$"
Class=Image
ClassGUID={6bdd1fc6-810f-11d0-bec7-08002be2092f}
Provider=%Sg%

[Manufacturer]
%Sg%=Models,NTamd64

[Models.NTamd64]
%Probe%=Probe,ROOT\SGPROBESCANNER

[Probe]
Include=sti.inf,sgneeds.inf
Needs=STI.USBSection,SgNeeded
SubClass=StillImage
DeviceType=1
DeviceSubType=0x2
Capabilities=0x31
DeviceData=Probe.DeviceData
Events=Probe.Events

[Probe.Services]
Include=sti.inf
Needs=STI.USBSection.Services

[Probe.DeviceData]
TwainDS="SG Probe TWAIN"
Server=local

[Probe.Events]
ScanButton=%ScanButton%,{a6c5a715-8c6e-11d2-977a-0000f87a926f},*

[Plain]
Include=sgneeds.inf
Needs=SgPlainNeeded
AddReg=Plain.Reg

[Plain.Reg]
HKCU,Software\SgStiProbe,Own,,"yes"

[Strings]
Sg="Stained Glass OS"
Probe="SG probe scanner"
ScanButton="Scan button"
EOF
out=$(timeout 120 "$WINE" "$T/sti.exe" 'C:\sgprobe' 2>/dev/null | tr -d '\r')
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v install)" = 1 ] && pass "the device installs" || fail "device install: $out"
[ "$(v service)" = usbscan ] && pass "its service is usbscan, from Needs=STI.USBSection.Services" || fail "service: '$(v service)'"
[ "$(v fromneeds)" = yes ] && pass "a Needs'd section of an Include'd INF writes the driver key" || fail "fromneeds: '$(v fromneeds)'"
[ "$(v subclass)" = StillImage ] && [ "$(v devicetype)" = 0x1 ] && [ "$(v devicesubtype)" = 0x2 ] && [ "$(v capabilities)" = 0x31 ] &&
    pass "SubClass, DeviceType, DeviceSubType, Capabilities" ||
    fail "class values: $(v subclass) $(v devicetype) $(v devicesubtype) $(v capabilities)"
[ "$(v twainds)" = "SG Probe TWAIN" ] && pass "DeviceData's lines in DeviceData" || fail "DeviceData: '$(v twainds)'"
[ "$(v event)" = "Scan button" ] && [ "$(v eventguid)" = "{a6c5a715-8c6e-11d2-977a-0000f87a926f}" ] &&
    pass "Events\\ScanButton" || fail "event: '$(v event)' '$(v eventguid)'"
[ "$(v plain)" = 1 ] && [ "$(v plainown)" = yes ] && pass "a plain section installs" || fail "plain: $(v plain) '$(v plainown)'"
[ "$(v plainneeded)" = yes ] && pass "and the section it Needs, first" || fail "plain Needs: '$(v plainneeded)'"
exit $RC
