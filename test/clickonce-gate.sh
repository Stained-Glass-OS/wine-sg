#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mscoree (6071, NET-HOST): ICLRRuntimeHost::ExecuteApplication gave E_NOTIMPL and CoEEShutDownCOM was a FIXME.
#  - ExecuteApplication runs a ClickOnce application already in place: of the manifests given, the one with an
#    entry point names the program (relative to it) and its parameters; the program runs with the parameters and
#    the activation data as arguments, and its exit code comes back; no entry point: 0x80070002; NULL: E_POINTER;
#  - CoEEShutDownCOM, with the runtime started, collects and runs the pending finalizers (the COM objects nothing
#    references are released with them); before any runtime it does nothing.
# Needs Wine Mono (its mcs.exe builds the application). SKIP without.
#   WINE=/opt/wine-sg/bin/wine test/clickonce-gate.sh   (mutants SG_MUTANT_CLICKONCE_NO_EXIT_CODE,
#   SG_MUTANT_EESHUTDOWN_NOTHING)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
MCS=""
for d in "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    f=$(ls "$d"/wine-mono-*/lib/mono/4.5/mcs.exe 2>/dev/null | head -1)
    [ -n "$f" ] && MCS=$f
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-clickonce.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/clickonce-probe.exe" "$HERE/clickonce-probe.c" -lole32 || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/app"
cp "$HERE/clickonce-app.cs" "$T/app.cs"
(cd "$T" && timeout -s KILL 300 "$WINE" "$MCS" -out:App.exe app.cs >/dev/null 2>&1 </dev/null)
[ -f "$T/App.exe" ] || { fail "mcs did not build the application"; exit 1; }
cp "$T/App.exe" "$C/app/App.exe"
cat > "$C/app.application" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<asmv1:assembly xmlns="urn:schemas-microsoft-com:asm.v2" xmlns:asmv1="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity name="app.application" version="1.0.0.0" language="neutral" processorArchitecture="msil" />
  <deployment install="false" />
  <dependency><dependentAssembly dependencyType="install" codebase="app\app.exe.manifest">
    <assemblyIdentity name="App.exe" version="1.0.0.0" language="neutral" processorArchitecture="msil" type="win32" />
  </dependentAssembly></dependency>
</asmv1:assembly>
XML
cat > "$C/app/app.exe.manifest" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<asmv1:assembly xmlns="urn:schemas-microsoft-com:asm.v2" xmlns:asmv1="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <asmv1:assemblyIdentity name="App.exe" version="1.0.0.0" processorArchitecture="msil" type="win32" />
  <application />
  <entryPoint>
    <assemblyIdentity name="App" version="1.0.0.0" language="neutral" processorArchitecture="msil" />
    <commandLine file="App.exe" parameters="p1" />
  </entryPoint>
</asmv1:assembly>
XML
cp "$T/clickonce-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 180 "$WINE" clickonce-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect ShutdownEarly 1 "CoEEShutDownCOM before any runtime does nothing"
expect NullReturn 0x80004003 "ExecuteApplication without a return value pointer: E_POINTER"
expect NoManifests 0x80070057 "without manifests: E_INVALIDARG"
expect NoEntryPoint 0x80070002 "manifests without an entry point: the file is not found"
expect Execute "0 10 p1,one,two" "the application runs with its parameters and the activation data; its exit code"
expect FinalizedBefore 0 "its unreferenced object is not finalized yet"
expect FinalizedAfter 1 "CoEEShutDownCOM collects and runs the finalizers"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
