#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A packaged desktop program's own identity (patches/sg/7713) -- what DuckDuckGo's browser asks at its
# start, in this order, each of which stopped it (Store MSIX walk, 2026-11-08):
#   GetPackageFamilyName/GetPackageFullName of a process handle (were stubs: "no package")
#   Package.Current as the full package (IPackage2 ... was IPackage only: InvalidCastException)
#   Package.Id an object of its own (Windows.ApplicationModel.PackageId; was an interface of the
#     package that a query for IPackageId did not find)
#   StartupTask (GetForCurrentPackageAsync, GetAsync, RequestEnableAsync, Disable, State; the class
#     was missing): its tasks are the manifest's windows.startupTask extensions, enabled while their
#     Run value "<family>!<TaskId>" is there, DisabledByUser when Task Manager turned it off
# A signed sideloaded package (TrustedPeople) with the probe (test/pkgself-app.c) and two tasks, one
# the manifest enables.
#   WINE=/opt/wine-sg/bin/wine test/pkgself-gate.sh
# Mutants: SG_MUTANT_NO_PROCESS_PACKAGE (kernelbase), SG_MUTANT_CURRENT_PACKAGE_LOCAL,
# SG_MUTANT_STARTUP_NO_TASKS, SG_MUTANT_STARTUP_ENABLE_NOOP (windows.applicationmodel),
# SG_MUTANT_AX_ID_NO_OBJECT (appxdeploymentclient), SG_MUTANT_APPDATA_ONLY1 (windows.storage.applicationdata).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" python3 openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pkgself.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O2 -o "$B/deploy-probe.exe" "$HERE/deploy-probe.c" -lruntimeobject -lshlwapi || { fail "deploy-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$B/pkgself-app.exe" "$HERE/pkgself-app.c" -lruntimeobject || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/deploy-probe.exe" "$B/trust-probe.exe" "$B/pkgself-app.exe" "$C/"
PUB="CN=Stained Glass Self Test"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/pub.key" -out "$T/pub.pem" -days 2 -subj "/$PUB" \
    -addext "basicConstraints=critical,CA:FALSE" -addext "keyUsage=critical,digitalSignature" \
    -addext "extendedKeyUsage=codeSigning" 2>/dev/null || { fail "openssl could not make the certificate"; exit 1; }
openssl x509 -in "$T/pub.pem" -outform DER -out "$C/pub.cer"
cat > "$B/ext.xml" <<'XML'
<desktop:Extension Category="windows.startupTask" Executable="pkgself-app.exe" EntryPoint="Windows.FullTrustApplication"><desktop:StartupTask TaskId="SgTask" Enabled="false" DisplayName="Sg task"/></desktop:Extension>
<desktop:Extension Category="windows.startupTask" Executable="pkgself-app.exe" EntryPoint="Windows.FullTrustApplication"><desktop:StartupTask TaskId="SgTaskOn" Enabled="true" DisplayName="Sg task on"/></desktop:Extension>
XML
python3 "$HERE/mkmsix.py" package "$B/self.msix" --name StainedGlass.SelfTest --version 1.0.0.0 --publisher "$PUB" \
    --display-name "Self test" --app "App:pkgself-app.exe::Self test" --extensions "$B/ext.xml" \
    --file "$B/pkgself-app.exe:pkgself-app.exe" || { fail "the package could not be made"; exit 1; }
osslsigncode sign -certs "$T/pub.pem" -key "$T/pub.key" -h sha256 -in "$B/self.msix" -out "$C/self.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the package"; exit 1; }
run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
has() { if printf '%s\n' "$1" | grep -qxF "$2"; then pass "$3"; else fail "$3: no '$2' in: $(printf '%s' "$1" | tr '\n' ' ')"; fi; }
PUBID=$(python3 -c "
import hashlib,sys
h=hashlib.sha256(sys.argv[1].encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))" "$PUB")
FAM="StainedGlass.SelfTest_$PUBID"
FULL="StainedGlass.SelfTest_1.0.0.0_x64__$PUBID"
APP="C:\\Program Files\\WindowsApps\\$FULL\\pkgself-app.exe"
out=$(run trust-probe.exe --store LM TrustedPeople 'C:\pub.cer')
expect "$out" store added "the publisher is trusted (TrustedPeople)"
out=$(run deploy-probe.exe add 'C:\self.msix')
expect "$out" AddStatus 1 "the package installs"

out=$(run "$APP" family)
expect "$out" Family "$FAM" "GetPackageFamilyName(GetCurrentProcess()) is the package's family"
expect "$out" FamilySizeRet 122 "asked for its size: ERROR_INSUFFICIENT_BUFFER"
expect "$out" FamilySize "$((${#FAM} + 1))" "with the size"
expect "$out" FullSelf "$FULL" "GetPackageFullName of an opened handle to itself"
expect "$out" FullChild "$FULL" "of another process of the package"
expect "$out" OtherRet 15700 "an unpackaged process: APPMODEL_ERROR_NO_PACKAGE"

out=$(run "$APP" package)
expect "$out" Current 0x00000000 "Package.Current"
expect "$out" Package2 0x00000000 "is the full package (IPackage2)"
expect "$out" DisplayName "Self test" "with its display name"
expect "$out" IdQI 0x00000000 "its Id answers IPackageId"
expect "$out" IdClass Windows.ApplicationModel.PackageId "and is a PackageId"
expect "$out" IdOwnObject 1 "an object of its own"
expect "$out" IdFamily "$FAM" "with the family name"

out=$(run "$APP" appdata)
expect "$out" Data2 0x00000000 "ApplicationData.Current answers IApplicationData2 (7717)"
expect "$out" Data3 0x00000000 "and IApplicationData3"
printf '%s\n' "$out" | grep -q "^LocalCachePath=.*\\\\Packages\\\\$FAM\\\\LocalCache\$" && pass "LocalCacheFolder is the package's LocalCache" || fail "LocalCache: $(printf '%s' "$out" | tr '\n' ' ')"
printf '%s\n' "$out" | grep -q "^PublisherPath=.*\\\\Publishers\\\\$PUBID\\\\SgShared\$" && pass "a publisher cache folder is under Publishers\\<publisher id>" || fail "publisher folder: $(printf '%s' "$out" | tr '\n' ' ')"
expect "$out" Shared "0x00000000 1" "SharedLocalFolder: none (the policy is off)"
expect "$out" BadName 0x80070057 "a folder name with a backslash: E_INVALIDARG"

out=$(run "$APP" tasks)
expect "$out" Count 2 "StartupTask.GetForCurrentPackageAsync: the manifest's two tasks"
has "$out" "Task=SgTask 0" "the one the manifest leaves off is Disabled"
has "$out" "Task=SgTaskOn 2" "the one it enables is Enabled (installed in Run)"
out=$(run "$APP" enable SgTask)
expect "$out" EnableState 2 "RequestEnableAsync enables a disabled task"
reg=$(run reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Run' /v "$FAM!SgTask")
printf '%s' "$reg" | grep -q 'pkgself-app.exe' && pass "its Run value runs the program" || fail "Run value: $reg"
out=$(run "$APP" disable SgTask)
has "$out" "Task=SgTask 0" "Disable: Disabled again"
reg=$(run reg query 'HKCU\Software\Microsoft\Windows\CurrentVersion\Run' /v "$FAM!SgTask")
printf '%s' "$reg" | grep -q 'pkgself-app.exe' && fail "the Run value stays" || pass "its Run value is gone"
run reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run' /v "$FAM!SgTaskOn" /t REG_BINARY /d 030000000000000000000000 /f >/dev/null
out=$(run "$APP" enable SgTaskOn)
expect "$out" EnableState 1 "a task the person turned off in Task Manager: DisabledByUser, and it stays so"
out=$(run "$APP" get NoSuchTask)
expect "$out" Status 3 "GetAsync of a task the manifest does not have: the operation fails"
out=$(run 'C:\pkgself-app.exe' tasks)
if printf '%s\n' "$out" | grep -q '^Results=0x8'; then pass "a program outside any package has no tasks (the operation fails)"
else fail "unpackaged: $(printf '%s' "$out" | tr '\n' ' ')"; fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
