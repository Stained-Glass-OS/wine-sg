#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An app execution alias runs its own extension's program (patches/sg/7714): the
# windows.appExecutionAlias extension may name an Executable other than its application's.
# ImageMagick's MSIX (Advanced Installer) has a stub as the application and magick.exe on the
# alias extension: "magick" ran the stub, which printed nothing.
# A signed package (TrustedPeople) whose application is stub.exe and whose alias extension runs
# real.exe (test/msixapps-app.c: records its start in C:\msixapps-out.txt).
#   WINE=/opt/wine-sg/bin/wine test/aliasexe-gate.sh
# Mutant: SG_MUTANT_AX_ALIAS_APP_EXE (dlls/appxdeploymentclient).
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
T=$(mktemp -d /var/tmp/sg-aliasexe.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O2 -o "$B/deploy-probe.exe" "$HERE/deploy-probe.c" -lruntimeobject -lshlwapi || { fail "deploy-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/msixapps-app.exe" "$HERE/msixapps-app.c" || { fail "the application did not build"; exit 1; }
printf 'int main(void) { return 0; }\n' > "$B/stub.c"
"$MINGW" -O2 -o "$B/stub.exe" "$B/stub.c" || { fail "the stub did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/deploy-probe.exe" "$B/trust-probe.exe" "$C/"
PUB="CN=Stained Glass Alias Test"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/pub.key" -out "$T/pub.pem" -days 2 -subj "/$PUB" \
    -addext "basicConstraints=critical,CA:FALSE" -addext "keyUsage=critical,digitalSignature" \
    -addext "extendedKeyUsage=codeSigning" 2>/dev/null || { fail "openssl could not make the certificate"; exit 1; }
openssl x509 -in "$T/pub.pem" -outform DER -out "$C/pub.cer"
cat > "$B/ext.xml" <<'XML'
<uap5:Extension Category="windows.appExecutionAlias" Executable="tools\real.exe" EntryPoint="Windows.FullTrustApplication"><uap5:AppExecutionAlias><uap5:ExecutionAlias Alias="sgrealtool.exe"/></uap5:AppExecutionAlias></uap5:Extension>
XML
python3 "$HERE/mkmsix.py" package "$B/alias.msix" --name StainedGlass.AliasExeTest --version 1.0.0.0 --publisher "$PUB" \
    --display-name "Alias test" --app "App:stub.exe::Alias test" --extensions "$B/ext.xml" \
    --file "$B/stub.exe:stub.exe" --file "$B/msixapps-app.exe:tools\\real.exe" || { fail "the package could not be made"; exit 1; }
osslsigncode sign -certs "$T/pub.pem" -key "$T/pub.key" -h sha256 -in "$B/alias.msix" -out "$C/alias.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the package"; exit 1; }
run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
PUBID=$(python3 -c "
import hashlib,sys
h=hashlib.sha256(sys.argv[1].encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))" "$PUB")
FAM="StainedGlass.AliasExeTest_$PUBID"
out=$(run trust-probe.exe --store LM TrustedPeople 'C:\pub.cer')
expect "$out" store added "the publisher is trusted (TrustedPeople)"
out=$(run deploy-probe.exe add 'C:\alias.msix')
expect "$out" AddStatus 1 "the package installs"
"$(dirname "$WINE")/wineserver" -k 2>/dev/null; sleep 1
rm -f "$C/msixapps-out.txt"
printf '@echo off\r\nsgrealtool.exe one two\r\n' > "$C/alias.bat"
run cmd /c 'C:\alias.bat' >/dev/null
if [ -f "$C/msixapps-out.txt" ]; then pass "the alias runs the program its extension names (tools\\real.exe), not the application's"
else fail "the alias did not run tools\\real.exe"; fi
out=$(tr -d '\r' < "$C/msixapps-out.txt" 2>/dev/null)
expect "$out" args "one two" "with the arguments"
expect "$out" family "$FAM" "and the package's identity"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
