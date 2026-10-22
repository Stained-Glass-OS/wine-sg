#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package's COM servers and File Explorer commands (patches/sg/6029): TeraCopy's package (and the
# Win11-style menus of other programs) declares a com:SurrogateServer class and a
# fileExplorerContextMenus verb that names it.
#
#   classes   installing the package registers its classes for the user (InprocServer32 in the
#             package's external location, a DllSurrogate AppID) -- but not one the machine has
#             already; removing it takes them away
#   menu      File Explorer's context menu shows the verb's IExplorerCommand for its item types
#             ("*", ".ext", "Directory"), not a hidden one; sub-commands as a submenu, hidden ones
#             left out, disabled ones disabled; invoking runs the command with the selected items
#   removal   after the package is removed the command is gone from the menu
#
#   WINE=/opt/wine-sg/bin/wine test/pkgverb-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in "$MINGW" python3 openssl osslsigncode; do
    command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-pkgverb.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
    LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"

PUB='CN=Stained Glass Test Publisher'
PUBID=$(python3 -c "
import hashlib,sys
h=hashlib.sha256(sys.argv[1].encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))" "$PUB")
FULL="StainedGlass.VerbTest_1.0.0.0_x64__$PUBID"
C1='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0001}'
CKEEP='{5A3C1D10-6C1E-4D57-9B8A-2F4E7C9D0009}'

"$MINGW" -O2 -o "$B/sparse-probe.exe" "$HERE/sparse-probe.c" -lruntimeobject -lshlwapi -lole32 -luuid &&
"$MINGW" -O2 -o "$B/pkgverb-probe.exe" "$HERE/pkgverb-probe.c" -lole32 -lshell32 -luuid &&
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 &&
"$MINGW" -O2 -shared -o "$B/pkgverb.dll" "$HERE/pkgverb-cmd.c" -lole32 -luuid -lshell32 || { fail "the probes did not build"; exit 1; }

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$B/sparse-probe.exe" "$B/pkgverb-probe.exe" "$B/trust-probe.exe" "$C/"
mkdir -p "$C/VerbApp" "$C/Data/Sub"
cp "$B/pkgverb.dll" "$C/VerbApp/"
echo text > "$C/Data/a.txt"
echo sgx > "$C/Data/b.sgx"

cat > "$B/ext.xml" <<'EOF'
<com:Extension Category="windows.comServer"><com:ComServer>
<com:SurrogateServer DisplayName="SG verbs">
<com:Class Id="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0001" Path="pkgverb.dll" ThreadingModel="STA"/>
<com:Class Id="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0002" Path="pkgverb.dll" ThreadingModel="STA"/>
<com:Class Id="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0003" Path="pkgverb.dll" ThreadingModel="STA"/>
<com:Class Id="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0009" Path="pkgverb.dll" ThreadingModel="STA"/>
</com:SurrogateServer></com:ComServer></com:Extension>
<desktop4:Extension Category="windows.fileExplorerContextMenus"><desktop4:FileExplorerContextMenus>
<desktop5:ItemType Type="*"><desktop5:Verb Id="SgVerb" Clsid="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0001"/>
<desktop5:Verb Id="SgHidden" Clsid="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0003"/></desktop5:ItemType>
<desktop5:ItemType Type=".sgx"><desktop5:Verb Id="SgMenu" Clsid="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0002"/></desktop5:ItemType>
<desktop10:ItemType Type="Directory"><desktop10:Verb Id="SgVerbDir" Clsid="5a3c1d10-6c1e-4d57-9b8a-2f4e7c9d0001"/></desktop10:ItemType>
</desktop4:FileExplorerContextMenus></desktop4:Extension>
EOF
python3 "$HERE/mkmsix.py" package "$B/verb.msix" --name StainedGlass.VerbTest --version 1.0.0.0 --allow-external \
    --display-name "Verb test" --app "App:pkgverb-probe.exe::Verb test app" --extensions "$B/ext.xml"

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/verb.msix" -out "$C/verb.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign"; exit 1; }

run() { (cd "$C" && timeout -s KILL 180 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
lacks() {    # lacks OUTPUT LINE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2"; then fail "$3: '$2' is there"; else pass "$3"; fi
}
regval() { run reg query "$1" ${2:+/v "$2"} | grep -i "REG_SZ" | sed 's/.*REG_SZ *//' | head -1; }
haskey() { run reg query "$1" | grep -q '^HKEY'; }

out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted on this machine"
run reg add "HKLM\\Software\\Classes\\CLSID\\$CKEEP\\InprocServer32" /ve /d 'C:\keep.dll' /f >/dev/null

out=$(run pkgverb-probe.exe menu 'C:\Data\a.txt')
lacks "$out" "item=SG Package Verb|enabled" "before the package the menu has no package command"

out=$(run sparse-probe.exe add 'C:\verb.msix' 'C:\VerbApp')
expect "$out" status 1 "the package with its COM server and context menu verbs is added"
[ "$(regval "HKCU\\Software\\Classes\\CLSID\\$C1\\InprocServer32")" = 'C:\VerbApp\pkgverb.dll' ] &&
    pass "its class is registered for the user, in the external location" ||
    fail "class path: $(regval "HKCU\\Software\\Classes\\CLSID\\$C1\\InprocServer32")"
[ "$(regval "HKCU\\Software\\Classes\\CLSID\\$C1\\InprocServer32" ThreadingModel)" = Apartment ] &&
    pass "with its threading model (STA = Apartment)" || fail "threading model"
run reg query "HKCU\\Software\\Classes\\AppID\\$C1" /v DllSurrogate | grep -q 'DllSurrogate *REG_SZ' &&
    pass "as a surrogate server (DllSurrogate AppID)" || fail "no DllSurrogate AppID"
[ "$(regval "HKLM\\Software\\Classes\\CLSID\\$CKEEP\\InprocServer32")" = 'C:\keep.dll' ] &&
    ! haskey "HKCU\\Software\\Classes\\CLSID\\$CKEEP" &&
    pass "a class the machine has already is left as it is" || fail "the existing class was changed"
out=$(run pkgverb-probe.exe create "$C1")
expect "$out" create 00000000 "CoCreateInstance finds the package's command"

out=$(run pkgverb-probe.exe menu 'C:\Data\a.txt')
expect "$out" item "SG Package Verb|enabled" "the context menu of a file shows the package's command for every file"
lacks "$out" "item=SG Hidden Verb|enabled" "not the command that says it is hidden"
lacks "$out" "item=SG Package Menu|enabled" "not the command for another file type"
out=$(run pkgverb-probe.exe menu 'C:\Data\b.sgx')
expect "$out" item "SG Package Menu|enabled" "a .sgx file's menu shows the command for .sgx"
expect "$out" sub "Sub A|enabled" "with its sub-commands as a submenu"
lacks "$out" "sub=Sub B|enabled" "a hidden sub-command left out"
expect "$out" sub "Sub C|disabled" "a disabled one disabled"
out=$(run pkgverb-probe.exe menu 'C:\Data\Sub')
expect "$out" item "SG Package Verb|enabled" "a folder's menu shows the command for Directory"
rm -f "$C/pkgverb-log.txt"
out=$(run pkgverb-probe.exe invoke "SG Package Verb" 'C:\Data\a.txt' 'C:\Data\b.sgx')
expect "$out" invoke 00000000 "the command is invoked from the menu"
grep -q "invoked SG Package Verb" "$C/pkgverb-log.txt" 2>/dev/null && grep -q 'item C:\\Data\\a.txt' "$C/pkgverb-log.txt" &&
    grep -q 'item C:\\Data\\b.sgx' "$C/pkgverb-log.txt" && pass "with both selected items" ||
    fail "the command's log: $(tr '\n' ' ' < "$C/pkgverb-log.txt" 2>/dev/null)"
rm -f "$C/pkgverb-log.txt"
out=$(run pkgverb-probe.exe invoke "Sub A" 'C:\Data\b.sgx')
expect "$out" invoke 00000000 "a sub-command is invoked"
grep -q "invoked Sub A" "$C/pkgverb-log.txt" 2>/dev/null && pass "and runs itself" || fail "Sub A did not run"

out=$(run sparse-probe.exe remove "$FULL")
expect "$out" status 1 "the package is removed"
haskey "HKCU\\Software\\Classes\\CLSID\\$C1" && fail "its class is still registered" ||
    pass "and its classes with it"
[ "$(regval "HKLM\\Software\\Classes\\CLSID\\$CKEEP\\InprocServer32")" = 'C:\keep.dll' ] &&
    pass "the machine's own class stays" || fail "the existing class was removed"
out=$(run pkgverb-probe.exe menu 'C:\Data\a.txt')
lacks "$out" "item=SG Package Verb|enabled" "the menu no longer has the command"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
