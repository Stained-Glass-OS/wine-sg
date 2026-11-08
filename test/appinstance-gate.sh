#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.ApplicationModel.AppInstance (patches/sg/7708): a packaged app's instances by key. The first process
# that asks for a key registers it (IsCurrentInstance); another process of the package that asks for the same
# key gets that instance; GetInstances lists the live ones; Unregister and a process's end take a key away; a
# process with no package identity has none (APPMODEL_ERROR_NO_PACKAGE). RecommendedInstance and
# GetActivatedEventArgs are empty without the shell's activation. Teams asks for AppInstance at start.
#
#   WINE=/opt/wine-sg/bin/wine test/appinstance-gate.sh
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

T=$(mktemp -d /var/tmp/sg-appinstance.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$(dirname "$WINE")/wineserver" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O2 -o "$B/deploy-probe.exe" "$HERE/deploy-probe.c" -lruntimeobject -lshlwapi || { fail "deploy-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$B/appinstance-app.exe" "$HERE/appinstance-app.c" -lruntimeobject || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/deploy-probe.exe" "$B/trust-probe.exe" "$B/appinstance-app.exe" "$C/"

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/pub.key" -out "$T/pub.pem" -days 2 -subj "/CN=Stained Glass Test Publisher" \
    -addext "basicConstraints=critical,CA:FALSE" -addext "keyUsage=critical,digitalSignature" \
    -addext "extendedKeyUsage=codeSigning" 2>/dev/null || { fail "openssl could not make the certificate"; exit 1; }
openssl x509 -in "$T/pub.pem" -outform DER -out "$C/pub.cer"
python3 "$HERE/mkmsix.py" package "$B/ai.msix" --name StainedGlass.AppInstanceTest --version 1.0.0.0 \
    --app "App:appinstance-app.exe::AppInstance test" --file "$B/appinstance-app.exe:appinstance-app.exe" \
    || { fail "the package could not be made"; exit 1; }
osslsigncode sign -certs "$T/pub.pem" -key "$T/pub.key" -h sha256 -in "$B/ai.msix" -out "$C/ai.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the package"; exit 1; }
run() { (cd "$C" && timeout -s KILL 60 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Test Publisher'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")
FULL="StainedGlass.AppInstanceTest_1.0.0.0_x64__$PUBID"
APP="C:\\Program Files\\WindowsApps\\$FULL\\appinstance-app.exe"
out=$(run trust-probe.exe --store LM TrustedPeople 'C:\pub.cer')
out=$(run deploy-probe.exe add 'C:\ai.msix')
expect "$out" AddStatus 1 "the test package installs"

out=$(run appinstance-app.exe register first)
expect "$out" Register 0x80073d54 "a process with no package identity has no instances (APPMODEL_ERROR_NO_PACKAGE)"
out=$(run "$APP" statics)
expect "$out" Recommended "0x00000000 1" "RecommendedInstance is empty without the shell's activation"
expect "$out" Activated "0x00000000 1" "GetActivatedEventArgs is empty for a process started directly"
out=$(run "$APP" register main)
expect "$out" Register 0x00000000 "the first process registers its key"
expect "$out" Current 1 "... and is the current instance"
# a process that holds "main", while another asks
(cd "$C" && timeout -s KILL 60 "$WINE" "$APP" register main 12 >/dev/null 2>&1 &)
python3 -c "import time; time.sleep(5)"
out=$(run "$APP" register main)
expect "$out" Current 0 "another process asking for that key gets the registered instance"
expect "$out" Key main "... with the key"
out=$(run "$APP" instances)
expect "$out" Instances "0x00000000 1" "GetInstances lists the live instance"
out=$(run "$APP" unregister other)
expect "$out" Unregister 0x00000000 "Unregister"
expect "$out" Instances "0x00000000 1" "... takes this process's key away, not another's"
python3 -c "import time; time.sleep(9)"
out=$(run "$APP" instances)
expect "$out" Instances "0x00000000 0" "an instance whose process ended is gone"
out=$(run "$APP" register main)
expect "$out" Current 1 "... and its key can be registered again"
out=$(run deploy-probe.exe remove "$FULL")
expect "$out" RemoveStatus 1 "the test package is removed"

[ "$RC" = 0 ] && echo "appinstance gate: PASS" || echo "appinstance gate: FAIL"
exit "$RC"
