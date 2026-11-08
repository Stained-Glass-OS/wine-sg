#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Updating a package under its running programs, and removing one (patches/sg/7720), as on Windows:
#   in use      a newer version over an older one whose program runs fails with ERROR_PACKAGES_IN_USE
#               (0x80073D02) and changes nothing; with ForceApplicationShutdown (here through
#               UpdatePackageAsync) the program ends and the update goes through
#   any version ForceUpdateFromAnyVersion installs an older version in place of the newer one (one
#               version stays, not both); without it the older one is refused (0x80073D06)
#   removal     removing a package ends its running programs before its files go
# A signed sideloaded package (TrustedPeople) whose application sleeps (test/msixapps-app.c is the
# second program).
#   WINE=/opt/wine-sg/bin/wine test/pkgupdate-gate.sh
# Mutants: SG_MUTANT_AX_UPDATE_IN_USE, SG_MUTANT_AX_ANYVERSION_KEEPS, SG_MUTANT_AX_REMOVE_KEEPS_RUNNING
# (dlls/appxdeploymentclient/deploy.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" python3 openssl osslsigncode; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pkgupdate.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O2 -o "$B/deploy-probe.exe" "$HERE/deploy-probe.c" -lruntimeobject -lshlwapi || { fail "deploy-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/msixapps-app.exe" "$HERE/msixapps-app.c" || { fail "the application did not build"; exit 1; }
printf '#include <windows.h>\nint main(void) { Sleep( 120000 ); return 0; }\n' > "$B/sgsleeper.c"
"$MINGW" -O2 -o "$B/sgsleeper.exe" "$B/sgsleeper.c" || { fail "the sleeper did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/deploy-probe.exe" "$B/trust-probe.exe" "$C/"
PUB="CN=Stained Glass Update Test"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/pub.key" -out "$T/pub.pem" -days 2 -subj "/$PUB" \
    -addext "basicConstraints=critical,CA:FALSE" -addext "keyUsage=critical,digitalSignature" \
    -addext "extendedKeyUsage=codeSigning" 2>/dev/null || { fail "openssl could not make the certificate"; exit 1; }
openssl x509 -in "$T/pub.pem" -outform DER -out "$C/pub.cer"
for v in 1 2; do
    python3 "$HERE/mkmsix.py" package "$B/u$v.msix" --name StainedGlass.UpdateTest --version "$v.0.0.0" --publisher "$PUB" \
        --display-name "Update test" --app "App:sgsleeper.exe::Update test" \
        --file "$B/sgsleeper.exe:sgsleeper.exe" --file "$B/msixapps-app.exe:msixapps-app.exe" || { fail "package $v could not be made"; exit 1; }
    osslsigncode sign -certs "$T/pub.pem" -key "$T/pub.key" -h sha256 -in "$B/u$v.msix" -out "$C/u$v.msix" >/dev/null 2>&1 \
        || { fail "osslsigncode could not sign package $v"; exit 1; }
done
run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
PUBID=$(python3 -c "
import hashlib,sys
h=hashlib.sha256(sys.argv[1].encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))" "$PUB")
FULL1="StainedGlass.UpdateTest_1.0.0.0_x64__$PUBID"; FULL2="StainedGlass.UpdateTest_2.0.0.0_x64__$PUBID"
WA="$C/Program Files/WindowsApps"
sleeper() { (cd "$C" && "$WINE" "C:\\Program Files\\WindowsApps\\$1\\sgsleeper.exe" >/dev/null 2>&1 &); sleep 4; }
running() { pgrep -f "WindowsApps.$1.sgsleeper.exe" >/dev/null; }
find_out() { run deploy-probe.exe find StainedGlass.UpdateTest "$PUB"; }
out=$(run trust-probe.exe --store LM TrustedPeople 'C:\pub.cer')
expect "$out" store added "the publisher is trusted (TrustedPeople)"
out=$(run deploy-probe.exe add 'C:\u1.msix')
expect "$out" AddStatus 1 "version 1 installs"

sleeper "$FULL1"
running "$FULL1" && pass "its program runs" || fail "the program did not start"
out=$(run deploy-probe.exe add 'C:\u2.msix')
expect "$out" AddExtended 0x80073d02 "version 2 over the running version 1: ERROR_PACKAGES_IN_USE"
out=$(find_out)
expect "$out" FullName "$FULL1" "version 1 stays installed"
expect "$out" Count 1 "alone"
running "$FULL1" && pass "and its program still runs" || fail "the program was ended"
out=$(run deploy-probe.exe update 'C:\u2.msix' 1)
expect "$out" AddStatus 1 "UpdatePackageAsync with ForceApplicationShutdown: version 2 installs"
running "$FULL1" && fail "version 1's program still runs" || pass "version 1's program was ended"
out=$(find_out)
expect "$out" FullName "$FULL2" "version 2 replaces version 1"
expect "$out" Count 1 "one version"
[ -d "$WA/$FULL1" ] && fail "version 1's folder stays" || pass "version 1's folder is gone"

out=$(run deploy-probe.exe add 'C:\u1.msix')
expect "$out" AddExtended 0x80073d06 "version 1 over version 2: ERROR_INSTALL_PACKAGE_DOWNGRADE"
out=$(run deploy-probe.exe add 'C:\u1.msix' 40000)
expect "$out" AddStatus 1 "with ForceUpdateFromAnyVersion version 1 installs"
out=$(find_out)
expect "$out" FullName "$FULL1" "in place of version 2"
expect "$out" Count 1 "one version, not both"

sleeper "$FULL1"
running "$FULL1" && pass "its program runs again" || fail "the program did not start"
out=$(run deploy-probe.exe remove "$FULL1")
expect "$out" RemoveStatus 1 "removing the package"
running "$FULL1" && fail "the removed package's program still runs" || pass "ends its running program"
[ -d "$WA/$FULL1" ] && fail "its folder stays" || pass "and its files go"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
