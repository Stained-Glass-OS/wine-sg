#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A native program that hosts the .NET runtime (patch 7733; Visual Studio 2022's devenv.exe):
#  - its RequestRuntimeLoadedNotification callback calls back into the runtime it is told about; the
#    callback ran before the runtime was loaded, so that call loaded it again and called the callback
#    again, until the stack overflowed (devenv right after "AppId starting registry detouring")
#  - its default domain did not use the program's configuration for binding: no <probing privatePath>
#    (devenv's PrivateAssemblies) and no <codeBase> (devenv's PublicAssemblies\...17.x copies)
#   WINE=/opt/wine-sg/bin/wine test/clrhost-gate.sh
# Mutants (dlls/mscoree): SG_MUTANT_LOADED_CB_BEFORE_LOAD, SG_MUTANT_HOST_NO_PRIVATE_PATH, SG_MUTANT_NO_CODEBASE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
MCS=""
for d in "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    f=$(ls "$d"/wine-mono-*/lib/mono/4.5/mcs.exe 2>/dev/null | head -1)
    [ -n "$f" ] && MCS=$f
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-clrhost.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
unset WAYLAND_DISPLAY
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe64.exe" "$HERE/clrhost-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
if command -v i686-w64-mingw32-gcc >/dev/null; then
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe32.exe" "$HERE/clrhost-probe.c" || rm -f "$T/probe32.exe"
fi
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
B="$T/b"; mkdir -p "$B/lib" "$B/sub/v2"
cp "$HERE/net/clrhost-sgcb.cs" "$HERE/net/clrhost-sgpriv.cs" "$HERE/net/clrhost-entry.cs" "$HERE/net/sgtest.snk" "$B/"
mcs() { (cd "$B" && timeout -s KILL 300 "$WINE" "$MCS" -nologo "$@" >>"$T/mcs.log" 2>&1 </dev/null); }
mcs -target:library -keyfile:sgtest.snk -out:lib/SgCb.dll clrhost-sgcb.cs
mcs -target:library -keyfile:sgtest.snk -define:V2 -out:sub/v2/SgCb.dll clrhost-sgcb.cs
mcs -target:library -out:lib/SgPriv.dll clrhost-sgpriv.cs
mcs -target:library -r:lib/SgCb.dll -r:lib/SgPriv.dll -out:Entry.dll clrhost-entry.cs
for f in lib/SgCb.dll sub/v2/SgCb.dll lib/SgPriv.dll Entry.dll; do
    [ -f "$B/$f" ] || { echo "FAIL  mcs did not build $f"; cat "$T/mcs.log"; exit 1; }
done
TOKEN=$(/usr/bin/python3 -c "import hashlib,sys; print(hashlib.sha1(open(sys.argv[1],'rb').read()).digest()[-8:][::-1].hex())" "$HERE/net/sgtest-pub.snk")
D="$WINEPREFIX/drive_c/clrhost"
mkdir -p "$D"
cp -r "$B/lib" "$B/sub" "$B/Entry.dll" "$D/"
cp "$T"/probe*.exe "$D/"
for p in probe64 probe32; do
    [ -f "$D/$p.exe" ] || continue
    cat > "$D/$p.exe.config" <<EOC
<?xml version="1.0"?>
<configuration>
  <startup><supportedRuntime version="v4.0"/></startup>
  <runtime>
    <assemblyBinding xmlns="urn:schemas-microsoft-com:asm.v1">
      <probing privatePath="lib"/>
      <dependentAssembly>
        <assemblyIdentity name="SgCb" publicKeyToken="$TOKEN" culture="neutral"/>
        <bindingRedirect oldVersion="1.0.0.0-1.9.0.0" newVersion="2.0.0.0"/>
        <codeBase version="2.0.0.0" href="sub/v2/SgCb.dll"/>
      </dependentAssembly>
    </assemblyBinding>
  </runtime>
</configuration>
EOC
done
RC=0
for p in probe64 probe32; do
    [ -f "$D/$p.exe" ] || continue
    echo "== $p"
    out=$(cd "$D" && timeout -s KILL 180 "$WINE" "C:\\clrhost\\$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    echo "$out"
    echo "$out" | grep -q '^RESULT: PASS' || RC=1
    echo "$out" | grep -q '^FAIL' && RC=1
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
