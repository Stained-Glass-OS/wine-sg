#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A package's applications in the shell (patches/sg/7700, 7701):
#
#   integration  what the manifest's applications declare is registered for
#                the user: windows.protocol schemes (default "%1", or the
#                Protocol's Parameters), windows.fileTypeAssociation types
#                (a ProgID with the type's DisplayName and logo, the default
#                program of an extension nobody else has, offered for one
#                somebody else has), windows.startupTask (Enabled="true"),
#                the application in RegisteredApplications with its
#                Capabilities, the Start menu entry with its AppUserModelID
#                and the logo's sizes as its icon; a scheme another program
#                has stays that program's
#   activation   IApplicationActivationManager (in process and through its
#                surrogate) starts the application by AUMID with arguments,
#                for a file, inside the package (its identity); ShellExecuteEx
#                and explorer.exe start shell:AppsFolder\<AUMID>; a scheme and
#                a file type start it with the URI or the file; an AUMID of no
#                package starts nothing
#   removal      RemovePackageAsync takes all of it away and leaves other
#                programs' registrations as they were
#   download     a package downloaded from an address that does not end in
#                .msix (eM Client's .appinstaller service) is verified as a
#                package: wintrust picks the SIP by the file's extension
#
# The package is made here (test/mkmsix.py) and signed by a certificate
# generated for this run, trusted for this prefix only.
#
#   WINE=/opt/wine-sg/bin/wine test/msixapps-gate.sh
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

T=$(mktemp -d /var/tmp/sg-msixapps.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
# shellcheck disable=SC2317  # invoked via trap
HTTP_PID=
cleanup() {
    [ -n "$HTTP_PID" ] && kill "$HTTP_PID" 2>/dev/null
    "$(dirname "$WINE")/wineserver" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/b"
B="$T/b"
"$MINGW" -O2 -municode -o "$B/msixapps-probe.exe" "$HERE/msixapps-probe.c" -lole32 -lshell32 -luuid -lpropsys -lgdi32 -luser32 \
    || { fail "msixapps-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/deploy-probe.exe" "$HERE/deploy-probe.c" -lruntimeobject -lshlwapi || { fail "deploy-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/trust-probe.exe" "$HERE/trust-probe.c" -lwintrust -lcrypt32 || { fail "trust-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$B/msixapps-app.exe" "$HERE/msixapps-app.c" || { fail "msixapps-app did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$B/msixapps-probe.exe" "$B/deploy-probe.exe" "$B/trust-probe.exe" "$C/"

# ---- the package --------------------------------------------------------------
# real PNG logos: the application's at three target sizes, the file type's at one scale
python3 - "$B" <<'EOF'
import struct, sys, zlib
def png(path, w, h):
    raw = b''.join(b'\0' + bytes((40, 80, 160, 255)) * w for _ in range(h))
    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
for n in (16, 32, 48): png(f'{sys.argv[1]}/logo-{n}.png', n, n)
png(f'{sys.argv[1]}/doc-100.png', 44, 44)
EOF
cat > "$B/ext.xml" <<'EOF'
<uap:Extension Category="windows.protocol"><uap:Protocol Name="sgappstest"/></uap:Extension>
<uap:Extension Category="windows.protocol"><uap:Protocol Name="SGAppsArgs" Parameters="-uri &quot;%1&quot;"/></uap:Extension>
<uap:Extension Category="windows.protocol"><uap:Protocol Name="sgtaken"/></uap:Extension>
<uap:Extension Category="windows.fileTypeAssociation">
  <uap:FileTypeAssociation Name="sgdoc">
    <uap:DisplayName>Stained Glass test document</uap:DisplayName>
    <uap:Logo>Assets\doc.png</uap:Logo>
    <uap:SupportedFileTypes><uap:FileType>.sgdoc</uap:FileType><uap:FileType>.SGDOC2</uap:FileType><uap:FileType>.sgowned</uap:FileType></uap:SupportedFileTypes>
  </uap:FileTypeAssociation>
</uap:Extension>
<desktop:Extension Category="windows.startupTask" Executable="msixapps-app.exe" EntryPoint="Windows.FullTrustApplication">
  <desktop:StartupTask TaskId="BootTask" Enabled="true" DisplayName="Apps test"/>
</desktop:Extension>
<desktop:Extension Category="windows.startupTask" Executable="msixapps-app.exe" EntryPoint="Windows.FullTrustApplication">
  <desktop:StartupTask TaskId="OffTask" Enabled="false" DisplayName="Apps test (off)"/>
</desktop:Extension>
EOF
python3 "$HERE/mkmsix.py" package "$B/apps.msix" --name StainedGlass.AppsTest --version 1.0.0.0 --display-name "Apps test" \
    --app "Main:msixapps-app.exe::Apps test" --extensions "$B/ext.xml" \
    --file "$B/msixapps-app.exe:msixapps-app.exe" --file "$B/logo-16.png:Assets\\logo.targetsize-16.png" \
    --file "$B/logo-32.png:Assets\\logo.targetsize-32.png" --file "$B/logo-48.png:Assets\\logo.targetsize-48.png" \
    --file "$B/doc-100.png:Assets\\doc.scale-100.png" || { fail "the package could not be made"; exit 1; }

openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/ca.key" -out "$T/ca.pem" -days 2 \
    -subj "/CN=Stained Glass Gate Root" -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null
openssl req -newkey rsa:2048 -nodes -keyout "$T/signer.key" -out "$T/signer.csr" \
    -subj "/CN=Stained Glass Test Publisher" 2>/dev/null
printf 'basicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=codeSigning\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/signer.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial \
    -out "$T/signer.pem" -days 2 -extfile "$T/ext.cnf" 2>/dev/null
openssl x509 -in "$T/ca.pem" -outform DER -out "$C/ca.der"
osslsigncode sign -certs "$T/signer.pem" -key "$T/signer.key" -h sha256 -in "$B/apps.msix" -out "$C/apps.msix" >/dev/null 2>&1 \
    || { fail "osslsigncode could not sign the package"; exit 1; }

run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
expect_like() {   # expect_like OUTPUT KEY GLOB WHAT
    v=$(printf '%s\n' "$1" | sed -n "s/^$2=//p" | head -1)
    # shellcheck disable=SC2254  # the pattern is a glob
    case "$v" in $3) pass "$4";; *) fail "$4: $2 was '$v', wanted $3";; esac
}
reg() { run msixapps-probe.exe reg "$@"; }
out_file() { tr -d '\r' < "$C/msixapps-out.txt" 2>/dev/null; }
PUBID=$(python3 -c "
import hashlib
h=hashlib.sha256('CN=Stained Glass Test Publisher'.encode('utf-16-le')).digest()[:8]
n=int.from_bytes(h,'big')<<1; a='0123456789abcdefghjkmnpqrstvwxyz'
print(''.join(a[(n>>(60-5*i))&31] for i in range(13)))")
PFN="StainedGlass.AppsTest_$PUBID"
FULL="StainedGlass.AppsTest_1.0.0.0_x64__$PUBID"
AUMID="$PFN!Main"
LOC='C:\Program Files\WindowsApps\'"$FULL"

out=$(run trust-probe.exe --root 'C:\ca.der')
expect "$out" root added "the test publisher's root is trusted in this prefix"
# another program's: a scheme and an extension the package also declares
cat > "$C/other.reg" <<'EOF'
REGEDIT4

[HKEY_LOCAL_MACHINE\Software\Classes\sgtaken]
@="URL:other program"
"URL Protocol"=""
[HKEY_LOCAL_MACHINE\Software\Classes\sgtaken\shell\open\command]
@="C:\\windows\\notepad.exe \"%1\""
[HKEY_LOCAL_MACHINE\Software\Classes\.sgowned]
@="otherfile"
[HKEY_LOCAL_MACHINE\Software\Classes\otherfile\shell\open\command]
@="C:\\windows\\notepad.exe \"%1\""
EOF
run regedit /S 'C:\other.reg' >/dev/null

out=$(run deploy-probe.exe add 'C:\apps.msix')
expect "$out" AddStatus 1 "the package installs"

# ---- integration --------------------------------------------------------------
out=$(reg HKCR 'sgappstest\shell\open\command')
expect "$out" Value "\"$LOC\\msixapps-app.exe\" \"%1\"" "a protocol runs the application with the URI"
out=$(reg HKCR 'sgappstest' 'URL Protocol')
expect "$out" Value "" "the scheme is a URL protocol"
out=$(reg HKCR 'sgappstest\Application' AppUserModelID)
expect "$out" Value "$AUMID" "the scheme names the application's AUMID"
out=$(reg HKCR 'sgappsargs\shell\open\command')
expect "$out" Value "\"$LOC\\msixapps-app.exe\" -uri \"%1\"" "a protocol's Parameters are its command line"
out=$(reg HKCR 'sgtaken\shell\open\command')
expect "$out" Value 'C:\windows\notepad.exe "%1"' "another program's scheme stays that program's"
progid=$(reg HKCR '.sgdoc' | sed -n 's/^Value=//p')
expect_like "Value=$progid" Value 'AppX??????????????????????????' "a file type of nobody else's gets the package's ProgID"
out=$(reg HKCR '.sgdoc2')
expect "$out" Value "$progid" "the type's extensions are lower case and share the ProgID"
out=$(reg HKCR '.sgowned')
expect "$out" Value "otherfile" "another program's extension keeps its default"
out=$(reg HKCR '.sgowned\OpenWithProgids' "$progid")
expect "$out" Value "(none)" "the package is offered for another program's extension"
out=$(reg HKCR "$progid\\shell\\open\\command")
expect "$out" Value "\"$LOC\\msixapps-app.exe\" \"%1\"" "the file type runs the application with the file"
out=$(reg HKCR "$progid")
expect "$out" Value "Stained Glass test document" "the file type has its DisplayName"
icon=$(reg HKCR "$progid\\DefaultIcon" | sed -n 's/^Value=//; s/,0$//p')
out=$(run msixapps-probe.exe icon "$icon")
expect "$out" Icons 1 "the file type's icon is its logo (an .ico ExtractIconEx reads)"
out=$(reg HKCU 'Software\Microsoft\Windows\CurrentVersion\Run' "$PFN!BootTask")
expect "$out" Value "\"$LOC\\msixapps-app.exe\"" "an enabled startup task runs at sign-in"
out=$(reg HKCU 'Software\Microsoft\Windows\CurrentVersion\Run' "$PFN!OffTask")
expect "$out" Value "(missing)" "a disabled startup task does not"
caps=$(reg HKCU 'Software\RegisteredApplications' "$AUMID" | sed -n 's/^Value=//p')
expect_like "Value=$caps" Value 'Software\\Classes\\AppX*\\Capabilities' "the application is in RegisteredApplications"
out=$(reg HKCU "$caps\\URLAssociations" sgappstest)
expect_like "$out" Value 'AppX*' "its Capabilities list its schemes"
out=$(reg HKCU "$caps\\FileAssociations" .sgdoc)
expect "$out" Value "$progid" "its Capabilities list its file types"
out=$(reg HKCU "$caps" ApplicationName)
expect "$out" Value "Apps test" "its Capabilities name it"
LNK="$(cd "$C/users" && ls -d */AppData/Roaming/Microsoft/Windows/Start\ Menu/Programs | head -1)"
out=$(run msixapps-probe.exe lnk "C:\\users\\$(printf '%s' "$LNK" | tr '/' '\\')\\Apps test.lnk")
expect "$out" AUMID "$AUMID" "the Start menu entry carries the application's AUMID"
expect "$out" IconImages 3 "the Start menu entry's icon is the logo at its three sizes"

# ---- activation ---------------------------------------------------------------
rm -f "$C/msixapps-out.txt"
out=$(run msixapps-probe.exe activate "$AUMID" "one two")
expect "$out" Activate 0x00000000 "ActivateApplication starts the application"
o=$(out_file)
expect "$o" args "one two" "the application gets the arguments"
expect "$o" family "$PFN" "the application has the package's identity"
expect "$o" dir "$LOC" "the application starts in its package's folder"
rm -f "$C/msixapps-out.txt"
out=$(run msixapps-probe.exe activatelocal "$AUMID" "surrogate")
expect "$out" Activate 0x00000000 "the activation manager also serves CLSCTX_LOCAL_SERVER callers"
expect "$(out_file)" args "surrogate" "... and starts the application"
printf 'x' > "$C/doc one.sgdoc"
rm -f "$C/msixapps-out.txt"
out=$(run msixapps-probe.exe activatefile "$AUMID" 'C:\doc one.sgdoc')
expect "$out" Activate 0x00000000 "ActivateForFile starts the application"
expect "$(out_file)" args '"C:\doc one.sgdoc"' "... with the file"
rm -f "$C/msixapps-out.txt"
out=$(run msixapps-probe.exe activate "StainedGlass.NoSuchApp_$PUBID!Main")
expect "$out" Pid none "an AUMID of no package starts nothing"
expect_like "$out" Activate '0x8*' "... and fails"
out=$(run msixapps-probe.exe activate "$PFN!NoSuchApp")
expect_like "$out" Activate '0x8*' "an application the package does not have is not started"
rm -f "$C/msixapps-out.txt"
out=$(run msixapps-probe.exe exec "shell:AppsFolder\\$AUMID" "via-shell")
expect "$out" Exec 1 "ShellExecuteEx starts shell:AppsFolder\\<AUMID>"
expect "$out" Process 1 "... and hands back its process"
expect "$(out_file)" args "via-shell" "... with the parameters"
rm -f "$C/msixapps-out.txt"
run explorer.exe "shell:AppsFolder\\$AUMID" >/dev/null
i=0; while [ ! -s "$C/msixapps-out.txt" ] && [ $i -lt 100 ]; do python3 -c "import time; time.sleep(0.2)"; i=$((i+1)); done
expect "$(out_file)" family "$PFN" "explorer.exe shell:AppsFolder\\<AUMID> starts the application"
rm -f "$C/msixapps-out.txt"
out=$(run msixapps-probe.exe exec "sgappstest:hello/world")
expect "$out" Exec 1 "a URI of the package's scheme opens"
expect "$(out_file)" args '"sgappstest:hello/world"' "... in the application, with the URI"
rm -f "$C/msixapps-out.txt"
out=$(run msixapps-probe.exe exec 'C:\doc one.sgdoc')
expect "$out" Exec 1 "a file of the package's type opens"
expect "$(out_file)" args '"C:\doc one.sgdoc"' "... in the application, with the file"

# ---- removal ------------------------------------------------------------------
out=$(run deploy-probe.exe remove "$FULL")
expect "$out" RemoveStatus 1 "the package is removed"
for k in sgappstest sgappsargs "$progid" '.sgdoc' '.sgdoc2' "$(printf '%s' "$caps" | sed 's/^Software\\Classes\\//; s/\\Capabilities$//')"; do
    out=$(run msixapps-probe.exe haskey HKCR "$k")
    expect "$out" Key 0 "removal takes $k away"
done
out=$(reg HKCR 'sgtaken\shell\open\command')
expect "$out" Value 'C:\windows\notepad.exe "%1"' "another program's scheme is still that program's"
out=$(reg HKCR '.sgowned')
expect "$out" Value "otherfile" "another program's extension still has its default"
out=$(reg HKCR '.sgowned\OpenWithProgids' "$progid")
expect "$out" Value "(missing)" "the package is no longer offered for it"
out=$(reg HKCU 'Software\Microsoft\Windows\CurrentVersion\Run' "$PFN!BootTask")
expect "$out" Value "(missing)" "the startup task goes"
out=$(reg HKCU 'Software\RegisteredApplications' "$AUMID")
expect "$out" Value "(missing)" "the application leaves RegisteredApplications"
if [ -e "$(printf '%s' "$icon" | sed 's|^C:|'"$C"'|; s|\\|/|g')" ]; then fail "the icons go with the package"; else pass "the icons go with the package"; fi
out=$(run msixapps-probe.exe activate "$AUMID")
expect_like "$out" Activate '0x8*' "a removed package's application is not started"

# ---- download -----------------------------------------------------------------
mkdir -p "$T/www/api"
cp "$C/apps.msix" "$T/www/api/package"
PORT=$(python3 -c "import socket; s=socket.socket(); s.bind(('127.0.0.1',0)); print(s.getsockname()[1])")
(cd "$T/www" && exec python3 -m http.server "$PORT" --bind 127.0.0.1 >/dev/null 2>&1) & HTTP_PID=$!
i=0; until python3 -c "import socket; socket.create_connection(('127.0.0.1',$PORT),1)" 2>/dev/null || [ $i -gt 50 ]; do
    python3 -c "import time; time.sleep(0.1)"; i=$((i+1)); done
out=$(run deploy-probe.exe add "http://127.0.0.1:$PORT/api/package?channel=release")
expect "$out" AddStatus 1 "a package downloaded from an address without .msix installs"
out=$(run deploy-probe.exe remove "$FULL")
expect "$out" RemoveStatus 1 "... and is removed"

[ "$RC" = 0 ] && echo "msixapps gate: PASS" || echo "msixapps gate: FAIL"
exit "$RC"
