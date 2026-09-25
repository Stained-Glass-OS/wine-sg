#!/bin/sh
# MSIX / Store packages beyond a single package (patches/sg/0200-0204):
#
#   uri         Windows.Foundation.Uri takes a DOS path as a file: URI
#               (winget hands PackageManager its downloaded file that way)
#   frameworks  a package whose framework is not installed is refused
#               (ERROR_INSTALL_RESOLVE_DEPENDENCY_FAILED); passed as a
#               dependency it is installed first; Package.Dependencies names
#               it; the application loads the framework's DLL (the package
#               graph on its DLL path) and GetCurrentPackageInfo lists it; a
#               framework in use cannot be removed
#   aliases     the package's windows.appExecutionAlias runs it from cmd with
#               its identity, its arguments intact and its exit code; the
#               aliases' folder is on the user's PATH; removal takes it away
#   versions    a newer version replaces the older (and its alias follows);
#               an older one is refused (ERROR_INSTALL_PACKAGE_DOWNGRADE)
#   bundles     the resource packages for the user's language are installed
#               with the application, other languages' are not, and they go
#               with it
#   start menu  the application's Start menu entry
#   winget      with WINGET_DIR (a user-supplied winget, never shipped):
#               `winget install --manifest` of an MSIX from a local server,
#               then its alias runs, `winget list` shows it and
#               `winget uninstall` removes it
#
# Every fixture is made here: test/mkmsix.py writes the packages, signed with
# osslsigncode by a certificate generated for this run and thrown away.
#
#   WINE=/opt/wine-sg/bin/wine test/msix-gate.sh
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

T=$(mktemp -d /var/tmp/sg-msix.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
HTTP_PID=
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    [ -n "$HTTP_PID" ] && kill "$HTTP_PID" 2>/dev/null
    "$(dirname "$WINE")/wineserver" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"

B="$T/b"
"$MINGW" -O2 -o "$B/msix-probe.exe" "$HERE/msix-probe.c" -lruntimeobject -lshlwapi || { fail "msix-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -shared -o "$B/sgframework.dll" "$HERE/msix-fw.c" -Wl,--out-implib,"$B/libsgframework.a" || { fail "the framework DLL did not build"; exit 1; }
"$MINGW" -O2 -o "$B/msix-app.exe" "$HERE/msix-app.c" -L"$B" -lsgframework || { fail "msix-app did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/msix-probe.exe" "$B/trust-probe.exe" "$C/"

# ---- fixtures -----------------------------------------------------------------
MK="python3 $HERE/mkmsix.py"
$MK package "$B/fw.msix" --name StainedGlass.TestFramework --version 1.0.0.0 --framework \
    --file "$B/sgframework.dll:sgframework.dll"
for v in 1.0.0.0 1.1.0.0; do
    $MK package "$B/app-$v.msix" --name StainedGlass.MsixTest --version "$v" --display-name "MSIX test" \
        --dep StainedGlass.TestFramework:1.0.0.0 --app "App:msix-app.exe:sgmsixtest.exe:MSIX test app" \
        --file "$B/msix-app.exe:msix-app.exe"
done
# a bundle: the application, and resource packages for English and Japanese
$MK package "$B/b-app.msix" --name StainedGlass.BundleTest --version 2.0.0.0 --app "App:msix-app.exe::Bundle test" \
    --dep StainedGlass.TestFramework:1.0.0.0 --file "$B/msix-app.exe:msix-app.exe"
$MK package "$B/b-en.msix" --name StainedGlass.BundleTest --version 2.0.0.0 --arch neutral \
    --resource-id split.language-en --language en-us --file "$HERE/mkmsix.py:strings-en.txt"
$MK package "$B/b-ja.msix" --name StainedGlass.BundleTest --version 2.0.0.0 --arch neutral \
    --resource-id split.language-ja --language ja-jp --file "$HERE/mkmsix.py:strings-ja.txt"
$MK bundle "$B/bundle.msixbundle" --name StainedGlass.BundleTest --version 2.0.0.0 \
    --pkg "$B/b-app.msix:application:x64" --pkg "$B/b-en.msix:resource:en-us" --pkg "$B/b-ja.msix:resource:ja-jp"

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" \
    -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
for f in fw.msix app-1.0.0.0.msix app-1.1.0.0.msix bundle.msixbundle; do
    osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/$f" -out "$C/$f" >/dev/null 2>&1 \
        || { fail "osslsigncode could not sign $f"; exit 1; }
done

run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Test Publisher'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")
FW="StainedGlass.TestFramework_1.0.0.0_x64__$PUBID"
APP10="StainedGlass.MsixTest_1.0.0.0_x64__$PUBID"
APP11="StainedGlass.MsixTest_1.1.0.0_x64__$PUBID"
BAPP="StainedGlass.BundleTest_2.0.0.0_x64__$PUBID"
BEN="StainedGlass.BundleTest_2.0.0.0_neutral_split.language-en_$PUBID"
BJA="StainedGlass.BundleTest_2.0.0.0_neutral_split.language-ja_$PUBID"
WINAPPS="$C/users/$(id -un)/AppData/Local/Microsoft/WindowsApps"
out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted on this machine"

# ---- uri ---------------------------------------------------------------------
out=$(run msix-probe.exe uri 'C:\users\x\Downloads\app.msix')
expect "$out" CreateUri 0x00000000 "Windows.Foundation.Uri accepts a DOS path"
expect "$out" AbsoluteUri "file:///C:/users/x/Downloads/app.msix" "as a file: URI"

# ---- frameworks ----------------------------------------------------------------
out=$(run msix-probe.exe add 'C:\app-1.0.0.0.msix')
expect "$out" AddExtended 0x80073cf3 "an application whose framework is not installed is refused"
case "$out" in *'Provide the framework "StainedGlass.TestFramework"'*) pass "and the error names the framework" ;;
    *) fail "error text: $(printf '%s' "$out" | grep Text)" ;; esac
out=$(run msix-probe.exe info "$APP10")
expect "$out" Found 0 "nothing of it was installed"

out=$(run msix-probe.exe add 'C:\app-1.0.0.0.msix' 'C:\fw.msix')
expect "$out" AddStatus 1 "with the framework passed as a dependency, it installs"
out=$(run msix-probe.exe info "$FW")
expect "$out" IsFramework 1 "the framework is installed, as a framework"
out=$(run msix-probe.exe info "$APP10")
expect "$out" Dependencies 1 "Package.Dependencies has one package"
expect "$out" Dependency "$FW" "the framework"
expect "$out" InstalledDate set "Package.InstalledDate is set"

# ---- aliases -----------------------------------------------------------------
[ -f "$WINAPPS/sgmsixtest.exe" ] && pass "the alias is in WindowsApps" || fail "no alias file in $WINAPPS"
[ -f "$WINAPPS/StainedGlass.MsixTest_$PUBID/sgmsixtest.exe" ] && pass "and in its family's folder" || fail "no family alias"
out=$(run reg query 'HKCU\Environment' /v Path)
case "$out" in *'AppData\Local\Microsoft\WindowsApps'*) pass "the aliases' folder is on the user's PATH" ;;
    *) fail "user PATH: $out" ;; esac
printf '@echo off\r\nsgmsixtest.exe one "two three"\r\necho errorlevel=%%errorlevel%%\r\n' > "$C/alias.bat"
out=$(run cmd /c 'C:\alias.bat')
expect "$out" package "$APP10" "cmd runs the alias as the package's application, with its identity"
expect "$out" framework 7 "which loads the framework's DLL (the package graph)"
expect "$out" graph 2 "GetCurrentPackageInfo: the package and its framework"
expect "$out" graph1 "$FW framework" "the framework, with its folder"
expect "$out" arg2 "two three" "its arguments arrive intact"
expect "$out" errorlevel 42 "and cmd waits for it and gets its exit code"
lnk=$(find "$C/users" -path '*Start Menu/Programs/MSIX test app.lnk' 2>/dev/null | head -1)
[ -n "$lnk" ] && pass "a Start menu entry for the application" || fail "no Start menu entry"

out=$(run msix-probe.exe remove "$FW")
expect "$out" RemoveExtended 0x80073cfa "a framework an installed package depends on cannot be removed"
out=$(run msix-probe.exe info "$FW")
expect "$out" IsFramework 1 "and it is still there"

# ---- versions ------------------------------------------------------------------------
out=$(run msix-probe.exe add 'C:\app-1.1.0.0.msix')
expect "$out" AddStatus 1 "a newer version installs (its framework is already there)"
out=$(run msix-probe.exe info "$APP10")
expect "$out" Found 0 "and replaces the older version"
out=$(run cmd /c 'C:\alias.bat')
expect "$out" package "$APP11" "the alias now starts the new version"
out=$(run msix-probe.exe add 'C:\app-1.0.0.0.msix')
expect "$out" AddExtended 0x80073d06 "an older version is refused (ERROR_INSTALL_PACKAGE_DOWNGRADE)"

# ---- bundles -----------------------------------------------------------------------------
out=$(run msix-probe.exe add 'C:\bundle.msixbundle')
expect "$out" AddStatus 1 "a bundle with resource packages installs"
out=$(run msix-probe.exe list 4)
if printf '%s\n' "$out" | grep -qxF "Package=$BEN" && ! printf '%s\n' "$out" | grep -qF "language-ja"; then
    pass "with the resource package for the user's language (en-US) and not Japanese's"
else fail "resource packages: $(printf '%s' "$out" | tr '\n' ' ')"; fi
out=$(run msix-probe.exe info "$BEN")
expect "$out" IsResourcePackage 1 "Package.IsResourcePackage"
out=$(run msix-probe.exe list 1)
if printf '%s\n' "$out" | grep -qxF "Package=$BAPP" && ! printf '%s\n' "$out" | grep -q "language-"; then
    pass "main-package queries list the application, not its resources"
else fail "main packages: $(printf '%s' "$out" | tr '\n' ' ')"; fi
out=$(run msix-probe.exe remove "$BAPP")
expect "$out" RemoveStatus 1 "the bundle's application is removed"
out=$(run msix-probe.exe list 4)
printf '%s\n' "$out" | grep -q "BundleTest" && fail "resource packages left behind: $out" \
    || pass "and its resource packages with it"

# ---- removal ---------------------------------------------------------------------------------
out=$(run msix-probe.exe remove "$APP11")
expect "$out" RemoveStatus 1 "the application is removed"
[ ! -e "$WINAPPS/sgmsixtest.exe" ] && [ ! -e "$WINAPPS/StainedGlass.MsixTest_$PUBID/sgmsixtest.exe" ] \
    && pass "with its aliases" || fail "aliases left behind"
lnk=$(find "$C/users" -path '*Start Menu/Programs/MSIX test app.lnk' 2>/dev/null | head -1)
[ -z "$lnk" ] && pass "and its Start menu entry" || fail "Start menu entry left behind"
out=$(run msix-probe.exe remove "$FW")
expect "$out" RemoveStatus 1 "now the framework can be removed"

# ---- winget --------------------------------------------------------------------------------
if [ -n "${WINGET_DIR:-}" ] && [ -f "$WINGET_DIR/winget.exe" ] && command -v xvfb-run >/dev/null; then
    mkdir -p "$T/www" "$C/manifests"
    cp "$C/fw.msix" "$T/www/" && cp "$C/app-1.0.0.0.msix" "$T/www/app.msix"
    PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
    (cd "$T/www" && exec python3 -m http.server "$PORT" --bind 127.0.0.1 >/dev/null 2>&1) &
    HTTP_PID=$!
    cat > "$C/manifests/app.yaml" <<EOF
PackageIdentifier: StainedGlass.MsixTest
PackageVersion: 1.0.0.0
PackageLocale: en-US
Publisher: Stained Glass OS
PackageName: MSIX test
License: LGPL-2.1-or-later
ShortDescription: The MSIX gate's application
Installers:
- Architecture: x64
  InstallerType: msix
  InstallerUrl: http://127.0.0.1:$PORT/app.msix
  InstallerSha256: $(sha256sum "$T/www/app.msix" | cut -d' ' -f1)
  PackageFamilyName: StainedGlass.MsixTest_$PUBID
ManifestType: singleton
ManifestVersion: 1.6.0
EOF
    cp -r "$WINGET_DIR" "$C/winget"
    wg() { (cd "$C/winget" && timeout -s KILL 900 xvfb-run -a "$WINE" winget.exe "$@" --disable-interactivity 2>/dev/null | tr -d '\r'); }
    wg settings --enable LocalManifestFiles >/dev/null
    # the framework first, as a Store app's dependencies are
    out=$(run msix-probe.exe add 'C:\fw.msix')
    expect "$out" AddStatus 1 "winget: the framework is installed"
    out=$(wg install --manifest 'C:\manifests\app.yaml' --accept-package-agreements --accept-source-agreements)
    if printf '%s\n' "$out" | grep -q 'Successfully installed'; then pass "winget installs an MSIX package"
    else fail "winget install: $(printf '%s' "$out" | tail -3 | tr '\n' ' ')"; fi
    out=$(run cmd /c 'C:\alias.bat')
    expect "$out" package "$APP10" "the package winget installed runs from its alias"
    out=$(wg list --accept-source-agreements)
    if printf '%s\n' "$out" | grep -qF "MSIX\\$APP10"; then pass "winget lists it (as MSIX\\<full name>)"
    else fail "winget list: $(printf '%s' "$out" | tail -3 | tr '\n' ' ')"; fi
    out=$(wg uninstall --id "MSIX\\$APP10" --accept-source-agreements)
    out2=$(run msix-probe.exe info "$APP10")
    if printf '%s\n' "$out" | grep -q 'Successfully uninstalled' && printf '%s\n' "$out2" | grep -qx 'Found=0'; then
        pass "winget uninstalls it"
    else fail "winget uninstall: $(printf '%s' "$out" | tail -3 | tr '\n' ' ')"; fi
else
    echo "info  WINGET_DIR not set (or no xvfb-run): the winget clauses are skipped"
fi

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
