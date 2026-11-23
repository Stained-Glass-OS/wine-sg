#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package's user data goes with the package (patch 8800), as on Windows: RemovePackageAsync takes
# %LOCALAPPDATA%\Packages\<family> (LocalState, LocalCache, ...) and the family's settings
# (HKCU\Software\Wine\ApplicationData\<family>) away. An MSIX uninstall left LocalCache behind
# (LanguageTool, store walk 2026-11-11). The data stays when the caller keeps it
# (RemovalOptions_PreserveApplicationData), when an update replaces the package, and while another
# version of the family is still there.
#
#   WINE=/opt/wine-sg/bin/wine test/pkgdata-gate.sh
# Mutant: SG_MUTANT_AX_REMOVE_KEEPS_DATA (in dlls/appxdeploymentclient/deploy.c).
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

T=$(mktemp -d /var/tmp/sg-pkgdata.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
cleanup() {
    "$(sg_wineserver)" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O1 -o "$B/msix2-probe.exe" "$HERE/msix2-probe.c" -lruntimeobject -lshlwapi || { fail "msix2-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/msix2-probe.exe" "$B/trust-probe.exe" "$C/"

MK="python3 $HERE/mkmsix.py"
for v in 1 2; do
    $MK package "$B/app$v.msix" --name StainedGlass.DataTest --version "$v.0.0.0" --display-name "Data test" \
        --app "App:msix2-probe.exe:sgdatatest.exe:Data test app" --file "$B/msix2-probe.exe:msix2-probe.exe" \
        || { fail "mkmsix could not make version $v"; exit 1; }
done

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" \
    -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
for f in app1 app2; do
    osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/$f.msix" -out "$C/$f.msix" >/dev/null 2>&1 \
        || { fail "osslsigncode could not sign $f"; exit 1; }
done

run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
P=./msix2-probe.exe
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Test Publisher'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")
V1="StainedGlass.DataTest_1.0.0.0_x64__$PUBID"
V2="StainedGlass.DataTest_2.0.0.0_x64__$PUBID"
FAMILY="StainedGlass.DataTest_$PUBID"
DATA="$C/users/$(id -un)/AppData/Local/Packages/$FAMILY"
SETKEY="HKCU\\Software\\Wine\\ApplicationData\\$FAMILY\\LocalSettings"
OTHER="$C/users/$(id -un)/AppData/Local/Packages/StainedGlass.Other_$PUBID"

# what a package's programs keep: files in its folders and a settings value
make_data() {
    mkdir -p "$DATA/LocalState" "$DATA/LocalCache/Local" "$DATA/TempState" "$OTHER/LocalState"
    echo state > "$DATA/LocalState/state.txt"
    echo cache > "$DATA/LocalCache/Local/cache.bin"
    echo other > "$OTHER/LocalState/keep.txt"
    run reg add "$SETKEY" /v Theme /t REG_SZ /d dark /f >/dev/null
}
settings_there() { run reg query "$SETKEY" /v Theme 2>/dev/null | grep -q dark; }

out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted on this machine"

# ---- removal with the data kept ----------------------------------------------------------------
out=$(run $P add 'C:\app1.msix')
expect "$out" AddStatus 1 "version 1 installs"
make_data
out=$(run $P removekeep "$V1")
expect "$out" RemoveStatus 1 "RemovePackageWithOptionsAsync(PreserveApplicationData) removes it"
out=$(run $P find "$V1")
expect "$out" Found 0 "the package is gone"
[ -f "$DATA/LocalState/state.txt" ] && [ -f "$DATA/LocalCache/Local/cache.bin" ] \
    && pass "its data stays when the caller keeps it" || fail "PreserveApplicationData lost the data"
settings_there && pass "and so do its settings" || fail "PreserveApplicationData lost the settings"

# ---- an update keeps the data ------------------------------------------------------------------
out=$(run $P add 'C:\app1.msix')
expect "$out" AddStatus 1 "version 1 installs again"
out=$(run $P add 'C:\app2.msix')
expect "$out" AddStatus 1 "version 2 updates it"
out=$(run $P find "$V1")
expect "$out" Found 0 "version 1 is replaced"
[ -f "$DATA/LocalState/state.txt" ] && [ -f "$DATA/LocalCache/Local/cache.bin" ] \
    && pass "an update keeps the user's data" || fail "the update took the data"
settings_there && pass "and its settings" || fail "the update took the settings"

# ---- removing the package takes its data -------------------------------------------------------
out=$(run $P remove "$V2")
expect "$out" RemoveStatus 1 "RemovePackageAsync removes version 2"
out=$(run $P find "$V2")
expect "$out" Found 0 "no version of the family is left"
[ ! -e "$DATA" ] && pass "Packages\\<family> went with it (LocalState, LocalCache, TempState)" \
    || fail "the package's data stayed: $(cd "$DATA" 2>/dev/null && find . | tr '\n' ' ')"
settings_there && fail "the family's settings stayed" || pass "and its settings went"
[ -f "$OTHER/LocalState/keep.txt" ] && pass "another family's data is left alone" || fail "another family's data went"

# ---- a removal that fails takes nothing --------------------------------------------------------
make_data
out=$(run $P remove "StainedGlass.DataTest_9.0.0.0_x64__$PUBID")
expect "$out" RemoveStatus 3 "removing a version that is not installed fails"
[ -f "$DATA/LocalState/state.txt" ] && pass "and leaves the family's data" || fail "a failed removal took the data"

exit $RC
