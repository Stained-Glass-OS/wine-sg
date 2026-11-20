#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A packaged program's application user model ID without an app execution alias (wine-sg 7755):
# GetCurrentApplicationUserModelId and GetApplicationUserModelId give "<family>!<application>" from
#   - the package's AppxManifest.xml, the Application whose Executable is the program, and
#   - for a program in a package's external location (a sparse package), the applicationId its own
#     manifest's msix element gives.
# They said APPMODEL_ERROR_NO_APPLICATION, and the Windows App SDK runtime loaded by Office's aimgr.exe
# (the sparse package aimgr, its manifest naming applicationId "aimgr") stopped the process (c0000409)
# at every start: Word's AI manager never came up ("Manager transmit was not enabled after 10000 ms").
# Package records in HKCU\Software\Wine\AppModel\Packages; test/sparseaumid-probe.c, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/sparseaumid-gate.sh
# Mutants (dlls/kernelbase/version.c): SG_MUTANT_AUMID_NO_MANIFEST, SG_MUTANT_AUMID_NO_SPARSE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc x86_64-w64-mingw32-windres i686-w64-mingw32-windres; do
    command -v $t >/dev/null || { echo "SKIP: $t not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sparseaumid.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
PUB="CN=Stained Glass Aumid Test"
cat > "$T/app.manifest" <<XML
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="1.0.0.0" name="StainedGlass.AumidApp" type="win32"/>
  <msix xmlns="urn:schemas-microsoft-com:msix.v1" publisher="$PUB" packageName="Sg.SparseAumid" applicationId="SparseApp"/>
</assembly>
XML
printf '1 24 "%s"\n' "$T/app.manifest" > "$T/app.rc"
for b in 64 32; do
    a=x86_64; [ $b = 32 ] && a=i686
    TMPDIR=/var/tmp $a-w64-mingw32-windres -O coff -i "$T/app.rc" -o "$T/app$b.res" &&
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -municode -o "$T/plain$b.exe" "$HERE/sparseaumid-probe.c" &&
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -municode -o "$T/sparse$b.exe" "$HERE/sparseaumid-probe.c" "$T/app$b.res" ||
        { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
mkdir -p "$C/pkg/bin" "$C/sparsepkg" "$C/ext"
cp "$T/plain64.exe" "$T/plain32.exe" "$C/pkg/bin/"
cp "$T/plain64.exe" "$T/plain32.exe" "$C/pkg/"
cp "$T/sparse64.exe" "$T/sparse32.exe" "$T/plain64.exe" "$T/plain32.exe" "$C/ext/"
cp "$T/plain64.exe" "$T/plain32.exe" "$C/"
cat > "$C/pkg/AppxManifest.xml" <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10">
  <Applications>
    <Application Id="Other" Executable="other.exe"/>
    <Application Id="Tool64" Executable="bin/plain64.exe"/>
    <Application Id="Tool32" Executable="bin\plain32.exe"/>
  </Applications>
</Package>
XML
FULL=Sg.ManifestAumid_1.0.0.0_x64__8wekyb3d8bbwe
SPARSE=Sg.SparseAumid_1.0.0.0_x64__8wekyb3d8bbwe
K="HKCU\\Software\\Wine\\AppModel\\Packages"
reg() { timeout -s KILL 60 "$WINE" reg "$@" >/dev/null 2>&1; }
reg add "$K\\$FULL" /v InstallLocation /t REG_SZ /d 'C:\pkg' /f
reg add "$K\\$SPARSE" /v InstallLocation /t REG_SZ /d 'C:\sparsepkg' /f
reg add "$K\\$SPARSE" /v ExternalLocation /t REG_SZ /d 'C:\ext' /f
reg add "$K\\$SPARSE" /v Name /t REG_SZ /d Sg.SparseAumid /f
reg add "$K\\$SPARSE" /v Publisher /t REG_SZ /d "$PUB" /f
"$WINESERVER" -w
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}
for b in 64 32; do
    out=$(run "C:\\pkg\\bin\\plain$b.exe" self)
    expect "$out" Current "0 Sg.ManifestAumid_8wekyb3d8bbwe!Tool$b" "$b: in a package: the Application whose Executable it is"
    expect "$out" OfSelf "0 Sg.ManifestAumid_8wekyb3d8bbwe!Tool$b" "$b: GetApplicationUserModelId of itself"
    out=$(run "C:\\pkg\\plain$b.exe" self)
    expect "$out" Current "15703 -" "$b: a program of the package no Application names: APPMODEL_ERROR_NO_APPLICATION"
    out=$(run "C:\\ext\\sparse$b.exe" self)
    expect "$out" Current "0 Sg.SparseAumid_8wekyb3d8bbwe!SparseApp" "$b: in an external location, its manifest's applicationId"
    out=$(run "C:\\ext\\plain$b.exe" self)
    expect "$out" Current "15703 -" "$b: one there whose manifest does not name the package: none"
    out=$(run "C:\\plain$b.exe" of "C:\\ext\\sparse$b.exe")
    expect "$out" Of "0 Sg.SparseAumid_8wekyb3d8bbwe!SparseApp" "$b: GetApplicationUserModelId of the sparse program from outside"
    out=$(run "C:\\plain$b.exe" self)
    expect "$out" Current "15703 -" "$b: a program outside any package: APPMODEL_ERROR_NO_APPLICATION"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
