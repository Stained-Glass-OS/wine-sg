#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ICLRStrongName and the strong name image functions (wine-sg 8202, NET-HOST). ICLRRuntimeInfo::GetInterface
# (CLSID_CLRStrongName) gave CLASS_E_CLASSNOTAVAILABLE, and StrongNameGetBlob, StrongNameGetBlobFromImage and
# StrongNameSignatureVerificationFromImage were stubs. The probe (test/clrsn-probe.c), 64- and 32-bit:
#   the interface from the v4 runtime; its methods answer HRESULTs (hash size, verification of a signed
#   assembly, a missing file's error, the public key token, comparison); GetBlob (too small a buffer: the size;
#   then the file's bytes); a SEC_IMAGE mapping of the assembly: GetBlobFromImage, its signature verified from
#   the image (interface and export, SN_OUTFLAG_WAS_VERIFIED), a byte of its code changed:
#   CORSEC_E_INVALID_STRONGNAME; not an image: refused.
# Fixture: test/net/sgmeta-signed.dll (Wine Mono's mcs, key sgtest.snk).
#   WINE=/opt/wine-sg/bin/wine test/clrsn-gate.sh
# Mutants (mscoree): SG_MUTANT_SN_NO_IFACE (corruntimehost.c), SG_MUTANT_SN_IMAGE_FLAT, SG_MUTANT_SN_IFACE_OK
# (strongname.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ARCHS="x86_64 i686"
for a in $ARCHS; do command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc not installed"; exit 77; }; done
RC=0
T=$(mktemp -d /var/tmp/sg-clrsn.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/clrsn-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
done
cp "$HERE"/net/sgmeta-signed.dll "$T/"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in $ARCHS; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    expect() {   # expect KEY VALUE WHAT
        if printf '%s\n' "$out" | grep -qxF "$1=$2"; then printf 'PASS  %s\n' "$3"
        else printf 'FAIL  %s: wanted %s=%s, got %s\n' "$3" "$1" "$2" "$(printf '%s\n' "$out" | grep "^$1=" || echo none)"; RC=1; fi
    }
    expect Exports "1 1" "StrongNameGetBlob and StrongNameSignatureVerificationFromImage are exported"
    expect Runtime 0 "the v4 runtime (ICLRMetaHost::GetRuntime)"
    expect Interface "0 1" "ICLRRuntimeInfo::GetInterface(CLSID_CLRStrongName): ICLRStrongName"
    expect QueryInterface 0 "it answers IUnknown"
    expect HashSize "0 20" "StrongNameHashSize(SHA-1): 20"
    expect Verify "0 1" "a signed assembly verifies (forced)"
    expect VerifyMissing 1 "a missing file: its error as the HRESULT"
    expect Token "0 8" "the assembly's public key token: 8 bytes"
    expect Compare "0 1" "an assembly compared with itself: identical"
    expect GetBlob.Small "1 1" "GetBlob with too small a buffer: fails, gives the size"
    expect GetBlob "0 1 1" "GetBlob: the file's bytes"
    expect Mapped 1 "the assembly mapped as an image"
    expect GetBlobFromImage "0 64 1" "GetBlobFromImage: the image's bytes"
    expect VerifyImage "0 1" "its signature verified from the mapped image (was verified)"
    expect VerifyImage.Export "1 1" "the same through the export"
    expect VerifyTampered 1 "a byte of its code changed: CORSEC_E_INVALID_STRONGNAME"
    expect VerifyGarbage 1 "not an image: refused"
    expect done 1 "the probe ran to the end"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
