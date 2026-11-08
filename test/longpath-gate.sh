#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package's files beyond MAX_PATH (patches/sg/7712). The Store's install of ChatGPT -- deepest file
# 253 characters below Program Files, past 260 in a person's AppData\Local\Programs\WindowsApps --
# stopped with 0x80080206 "the package's files could not be installed" (qa4, 2026-11-08): the payload
# paths were cut at MAX_PATH. Now the payload is written, checked and removed through "\\?\" paths.
#
# A signed package with a file 230 characters deep: installed for the machine (Program Files) and for
# the person (their WindowsApps when Program Files' is not writable); each time the file is there,
# whole, and removing the package takes its folder away.
#   WINE=/opt/wine-sg/bin/wine test/longpath-gate.sh
# Mutant: SG_MUTANT_AX_SHORT_PATHS (dlls/appxdeploymentclient).
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
T=$(mktemp -d /var/tmp/sg-longpath.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset DISPLAY WAYLAND_DISPLAY
cleanup() {
    chmod -R u+w "$T" 2>/dev/null
    "$(dirname "$WINE")/wineserver" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O2 -o "$B/deploy-probe.exe" "$HERE/deploy-probe.c" -lruntimeobject -lshlwapi || { fail "deploy-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/msixapps-app.exe" "$HERE/msixapps-app.c" || { fail "the application did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/deploy-probe.exe" "$B/trust-probe.exe" "$C/"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/pub.key" -out "$T/pub.pem" -days 2 -subj "/CN=Stained Glass Long Paths" \
    -addext "basicConstraints=critical,CA:FALSE" -addext "keyUsage=critical,digitalSignature" \
    -addext "extendedKeyUsage=codeSigning" 2>/dev/null || { fail "openssl could not make the certificate"; exit 1; }
openssl x509 -in "$T/pub.pem" -outform DER -out "$C/pub.cer"
DEEP='node_modules\.pnpm\@statsig_client-core@3.32.6\node_modules\@statsig\client-core\src\a-rather-long-folder-name-0123456789\another-long-folder-name-abcdefghij\DownloadConfigSpecsResponse-with-a-long-name.js'
head -c 150000 /dev/urandom > "$B/deep.bin"
python3 "$HERE/mkmsix.py" package "$B/long.msix" --name StainedGlass.LongPathTest --version 1.0.0.0 \
    --publisher "CN=Stained Glass Long Paths" --app "App:msixapps-app.exe::Long path test" \
    --file "$B/msixapps-app.exe:msixapps-app.exe" --file "$B/deep.bin:$DEEP" || { fail "the package could not be made"; exit 1; }
osslsigncode sign -certs "$T/pub.pem" -key "$T/pub.key" -h sha256 -in "$B/long.msix" -out "$C/long.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the package"; exit 1; }
run() { (cd "$C" && timeout -s KILL 300 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Long Paths'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")
FULL="StainedGlass.LongPathTest_1.0.0.0_x64__$PUBID"
UNIXDEEP=$(printf '%s' "$DEEP" | tr '\\' '/')
out=$(run trust-probe.exe --store LM TrustedPeople 'C:\pub.cer')
expect "$out" store added "the publisher is trusted (TrustedPeople)"

check_install() {   # check_install WHERE UNIXDIR WINDIR
    WINPATH="$3\\$FULL\\$DEEP"
    echo "      deepest path: ${#WINPATH} characters"
    out=$(run deploy-probe.exe add 'C:\long.msix')
    expect "$out" AddStatus 1 "$1: the package installs"
    if cmp -s "$B/deep.bin" "$2/$FULL/$UNIXDEEP"; then pass "$1: its ${#WINPATH}-character file is there, whole"
    else fail "$1: the deep file is missing or differs"; fi
    out=$(run deploy-probe.exe remove "$FULL")
    expect "$out" RemoveStatus 1 "$1: it is removed"
    if [ ! -e "$2/$FULL" ]; then pass "$1: with its folder, deep files and all"
    else fail "$1: left behind: $(find "$2/$FULL" -type f | head -3 | sed "s|$2/||")"; fi
}
check_install "for the machine" "$C/Program Files/WindowsApps" 'C:\Program Files\WindowsApps'
# Program Files' WindowsApps not writable: the person's own
chmod a-w "$C/Program Files/WindowsApps"
U=$(ls -d "$C"/users/*/AppData/Local | grep -v Public | head -1)
WU=$(printf 'C:%s' "${U#$C}" | tr '/' '\\')
check_install "for the person" "$U/Programs/WindowsApps" "$WU\\Programs\\WindowsApps"
chmod u+w "$C/Program Files/WindowsApps"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
