#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A native program that customizes the default domain of the .NET runtime it hosts (patch 7734; Visual
# Studio 2022's devenv.exe): ICLRControl::GetCLRManager had no managers, so devenv never named its
# AppDomainManager (ICLRDomainManager), the manager never ran, and devenv's per-instance configuration
# (the manager points the domain at it) with its extensions' code bases was never used.
#   WINE=/opt/wine-sg/bin/wine test/clrmgr-gate.sh
# Mutants (dlls/mscoree): SG_MUTANT_NO_ONEVENT_MANAGER, SG_MUTANT_NO_DOMAIN_MANAGER,
# SG_MUTANT_MANAGER_CONFIG_IGNORED, SG_MUTANT_NO_HOST_SET_MANAGER.
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
T=$(mktemp -d /var/tmp/sg-clrmgr.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
unset WAYLAND_DISPLAY
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe64.exe" "$HERE/clrmgr-probe.c" -luuid || { echo "FAIL  probe did not build"; exit 1; }
if command -v i686-w64-mingw32-gcc >/dev/null; then
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -Wno-format -o "$T/probe32.exe" "$HERE/clrmgr-probe.c" -luuid || rm -f "$T/probe32.exe"
fi
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
B="$T/b"; mkdir -p "$B/deep"
cp "$HERE/net/clrmgr-manager.cs" "$HERE/net/clrmgr-entry.cs" "$HERE/net/clrmgr-other.cs" "$HERE/net/sgtest.snk" "$B/"
mcs() { (cd "$B" && timeout -s KILL 300 "$WINE" "$MCS" -nologo "$@" >>"$T/mcs.log" 2>&1 </dev/null); }
mcs -target:library -out:SgMgr.dll clrmgr-manager.cs
mcs -target:library -keyfile:sgtest.snk -out:deep/SgOther.dll clrmgr-other.cs
mcs -target:library -r:SgMgr.dll -r:deep/SgOther.dll -out:Entry2.dll clrmgr-entry.cs
for f in SgMgr.dll deep/SgOther.dll Entry2.dll; do
    [ -f "$B/$f" ] || { echo "FAIL  mcs did not build $f"; cat "$T/mcs.log"; exit 1; }
done
TOKEN=$(/usr/bin/python3 -c "import hashlib,sys; print(hashlib.sha1(open(sys.argv[1],'rb').read()).digest()[-8:][::-1].hex())" "$HERE/net/sgtest-pub.snk")
D="$WINEPREFIX/drive_c/clrmgr"
mkdir -p "$D"
cp -r "$B/deep" "$B/SgMgr.dll" "$B/Entry2.dll" "$D/"
cp "$T"/probe*.exe "$D/"
for p in probe64 probe32; do
    [ -f "$D/$p.exe" ] || continue
    printf '<?xml version="1.0"?>\n<configuration><startup><supportedRuntime version="v4.0"/></startup></configuration>\n' > "$D/$p.exe.config"
done
cat > "$D/other.config" <<EOC
<?xml version="1.0"?>
<configuration>
  <runtime>
    <assemblyBinding xmlns="urn:schemas-microsoft-com:asm.v1">
      <dependentAssembly>
        <assemblyIdentity name="SgOther" publicKeyToken="$TOKEN" culture="neutral"/>
        <codeBase version="3.0.0.0" href="deep/SgOther.dll"/>
      </dependentAssembly>
    </assemblyBinding>
  </runtime>
</configuration>
EOC
RC=0
for p in probe64 probe32; do
    [ -f "$D/$p.exe" ] || continue
    echo "== $p"
    out=$(cd "$D" && timeout -s KILL 180 "$WINE" "C:\\clrmgr\\$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    echo "$out"
    echo "$out" | grep -q '^RESULT: PASS' || RC=1
    echo "$out" | grep -q '^FAIL' && RC=1
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
