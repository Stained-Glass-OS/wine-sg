#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Runtime classes declared by packages (patches/sg/6124): RoGetActivationFactory finds the classes
# that the process's package and the frameworks it depends on declare in their manifests
# (windows.activatableClass.inProcessServer), as Windows does for a packaged app. A WinUI 3 app's
# Microsoft.UI.Xaml classes come from the Windows App SDK framework package this way: App Installer
# stopped at Microsoft.UI.Xaml.PropertyMetadata. test/pkgclass-dll.c is the class server (built for
# the framework and for the app's own package), test/pkgclass-app.c the application.
#   WINE=/opt/wine-sg/bin/wine test/pkgclass-gate.sh
# Mutant (dlls/combase/roapi.c): SG_MUTANT_NO_PACKAGE_CLASSES.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
export TMPDIR=/var/tmp
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" python3 openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-pkgclass.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O2 -o "$B/msix-probe.exe" "$HERE/msix-probe.c" -lruntimeobject -lshlwapi || { fail "msix-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -shared -DTAG='"fw"' -o "$B/sgclassfw.dll" "$HERE/pkgclass-dll.c" -lruntimeobject -luuid || { fail "the framework's class DLL did not build"; exit 1; }
"$MINGW" -O2 -shared -DTAG='"app"' -o "$B/sgclassapp.dll" "$HERE/pkgclass-dll.c" -lruntimeobject -luuid || { fail "the app's class DLL did not build"; exit 1; }
"$MINGW" -O2 -o "$B/pkgclass-app.exe" "$HERE/pkgclass-app.c" -lruntimeobject || { fail "the application did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$B/msix-probe.exe" "$B/trust-probe.exe" "$C/"

cat > "$B/fw-ext.xml" <<'EOF'
<Extension Category="windows.activatableClass.inProcessServer">
  <InProcessServer>
    <Path>sgclassfw.dll</Path>
    <ActivatableClass ActivatableClassId="StainedGlass.Test.FwWidget" ThreadingModel="both" />
  </InProcessServer>
</Extension>
EOF
cat > "$B/app-ext.xml" <<'EOF'
<Extension Category="windows.activatableClass.inProcessServer">
  <InProcessServer>
    <Path>sgclassapp.dll</Path>
    <ActivatableClass ActivatableClassId="StainedGlass.Test.AppWidget" ThreadingModel="both" />
  </InProcessServer>
</Extension>
EOF
MK="python3 $HERE/mkmsix.py"
$MK package "$B/fw.msix" --name StainedGlass.ClassFramework --version 1.0.0.0 --framework \
    --file "$B/sgclassfw.dll:sgclassfw.dll" --package-extensions "$B/fw-ext.xml"
$MK package "$B/app.msix" --name StainedGlass.ClassApp --version 1.0.0.0 --display-name "Class test" \
    --dep StainedGlass.ClassFramework:1.0.0.0 --app "App:pkgclass-app.exe:sgpkgclass.exe:Class test" \
    --file "$B/pkgclass-app.exe:pkgclass-app.exe" --file "$B/sgclassapp.dll:sgclassapp.dll" \
    --package-extensions "$B/app-ext.xml"

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" \
    -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
for f in fw.msix app.msix; do
    osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/$f" -out "$C/$f" >/dev/null 2>&1 \
        || { fail "osslsigncode could not sign $f"; exit 1; }
done
run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted (this run's own)"
out=$(run msix-probe.exe add 'C:\app.msix' 'C:\fw.msix')
expect "$out" AddStatus 1 "the application and its framework install"

printf '@echo off\r\nsgpkgclass.exe\r\n' > "$C/run.bat"
out=$(run cmd /c 'C:\run.bat')
printf '%s\n' "$out" | sed 's/^/      /'
expect "$out" fw 0x00000000 "a class its framework package declares is activated (as WinUI 3's are from the Windows App SDK)"
expect "$out" fw_class "fw:StainedGlass.Test.FwWidget" "from the framework's DLL, in the framework's folder"
expect "$out" app 0x00000000 "a class its own package declares is activated"
expect "$out" app_class "app:StainedGlass.Test.AppWidget" "from the package's own DLL"
expect "$out" none 0x80040154 "a class no package declares is still not registered (REGDB_E_CLASSNOTREG)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
