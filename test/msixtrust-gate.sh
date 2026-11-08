#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Sideloaded packages and their signer's certificate (patches/sg/7702), as on
# Windows: a package signed by a certificate that chains to no trusted root is
# refused (CERT_E_UNTRUSTEDROOT, 0x800B0109) -- until that very certificate is
# in the machine's TrustedPeople store, where a package's .cer is installed to
# sideload it. Another certificate there does not help, and a package whose
# content was changed after signing is refused whatever is trusted.
#
# The certificates and the package are made here for the run.
#
#   WINE=/opt/wine-sg/bin/wine test/msixtrust-gate.sh
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

T=$(mktemp -d /var/tmp/sg-msixtrust.XXXXXX)
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
"$MINGW" -O2 -o "$B/msixapps-app.exe" "$HERE/msixapps-app.c" || { fail "the application did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/deploy-probe.exe" "$B/trust-probe.exe" "$C/"

# two self-signed code-signing certificates: the publisher's, and somebody else's
for who in pub other; do
    cn="Stained Glass Sideload Publisher"; [ "$who" = other ] && cn="Somebody Else"
    openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/$who.key" -out "$T/$who.pem" -days 2 -subj "/CN=$cn" \
        -addext "basicConstraints=critical,CA:FALSE" -addext "keyUsage=critical,digitalSignature" \
        -addext "extendedKeyUsage=codeSigning" 2>/dev/null || { fail "openssl could not make the $who certificate"; exit 1; }
    openssl x509 -in "$T/$who.pem" -outform DER -out "$C/$who.cer"
done
python3 "$HERE/mkmsix.py" package "$B/side.msix" --name StainedGlass.SideloadTest --version 1.0.0.0 \
    --publisher "CN=Stained Glass Sideload Publisher" --app "App:msixapps-app.exe::Sideload test" \
    --file "$B/msixapps-app.exe:msixapps-app.exe" || { fail "the package could not be made"; exit 1; }
osslsigncode sign -certs "$T/pub.pem" -key "$T/pub.key" -h sha256 -in "$B/side.msix" -out "$C/side.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the package"; exit 1; }
python3 - "$C/side.msix" "$C/side-tampered.msix" <<'EOF'
import sys, zipfile
src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
i = zipfile.ZipFile(src).getinfo('Assets/logo.png')
d[i.header_offset + 30 + len(i.filename) + len(i.extra) + 20] ^= 1
open(dst, 'wb').write(d)
EOF

run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Sideload Publisher'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")
FULL="StainedGlass.SideloadTest_1.0.0.0_x64__$PUBID"

out=$(run deploy-probe.exe add 'C:\side.msix')
expect "$out" AddStatus 3 "a package whose signer nothing trusts is refused"
expect "$out" AddError 0x800b0109 "... with CERT_E_UNTRUSTEDROOT"
out=$(run trust-probe.exe --store LM TrustedPeople 'C:\other.cer')
expect "$out" store added "somebody else's certificate is in the machine's TrustedPeople"
out=$(run deploy-probe.exe add 'C:\side.msix')
expect "$out" AddStatus 3 "... which does not make this package's signer trusted"
out=$(run trust-probe.exe --store LM TrustedPeople 'C:\pub.cer')
expect "$out" store added "the publisher's certificate is in the machine's TrustedPeople"
out=$(run deploy-probe.exe add 'C:\side-tampered.msix')
expect "$out" AddStatus 3 "a package changed after signing is still refused"
out=$(run deploy-probe.exe add 'C:\side.msix')
expect "$out" AddStatus 1 "the sideloaded package installs"
out=$(run deploy-probe.exe find StainedGlass.SideloadTest 'CN=Stained Glass Sideload Publisher')
expect "$out" FullName "$FULL" "... and is found"
out=$(run deploy-probe.exe remove "$FULL")
expect "$out" RemoveStatus 1 "... and is removed"

[ "$RC" = 0 ] && echo "msixtrust gate: PASS" || echo "msixtrust gate: FAIL"
exit "$RC"
