#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mscoree: a mixed-mode (C++/CLI) DLL stays loaded until the process ends (wine-sg 9452), as with the CLR:
# its native code and data are the process's, whichever application domain loaded it. Wine Mono freed it
# when the domain that loaded it unloaded, so its native DllMain ran inside the loader lock on the
# runtime's unloading thread; OpenOffice's CLI-UNO bridge (cli_uno.dll) deadlocked there, and
# AppDomain.Unload never returned. The probe (test/net/mixpin.cs, 32- and 64-bit) loads a library whose
# CLR header says it is not IL-only (test/net/mixpin-lib.cs, the flag cleared below) in a domain:
#   - it works there; after AppDomain.Unload the DLL is still loaded
#   - loaded again in a second domain, and in the default one, it works
#   WINE=/opt/wine-sg/bin/wine test/mixpin-gate.sh
# Mutant (dlls/mscoree): SG_MUTANT_MSCOREE_NO_PIN.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 not installed"; exit 77; }
MCS=""
for d in "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    f=$(ls "$d"/wine-mono-*/lib/mono/4.5/mcs.exe 2>/dev/null | head -1)
    [ -n "$f" ] && MCS=$f
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-mixpin.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER DISPLAY=
unset WAYLAND_DISPLAY
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
B="$T/b"
for a in x86 x64; do
    mkdir -p "$B/$a"
    cp "$HERE/net/mixpin.cs" "$HERE/net/mixpin-lib.cs" "$B/$a/"
    (cd "$B/$a" && timeout -s KILL 300 "$WINE" "$MCS" -nologo -platform:$a -target:library -out:mixpinlib.dll mixpin-lib.cs &&
        timeout -s KILL 300 "$WINE" "$MCS" -nologo -platform:$a -out:mixpin.exe mixpin.cs) >>"$T/mcs.log" 2>&1 </dev/null
    [ -f "$B/$a/mixpinlib.dll" ] && [ -f "$B/$a/mixpin.exe" ] || { echo "FAIL  mcs did not build the probe"; cat "$T/mcs.log"; exit 1; }
    # COMIMAGE_FLAGS_ILONLY cleared: an image with native code, as a C++/CLI compiler makes
    python3 - "$B/$a/mixpinlib.dll" <<'PY' || { echo "FAIL  the CLR header was not changed"; exit 1; }
import struct, sys
p = sys.argv[1]; d = bytearray(open(p, 'rb').read())
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec, optsz = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
opt = pe + 24
dd = opt + (96 if struct.unpack_from('<H', d, opt)[0] == 0x10b else 112)
rva = struct.unpack_from('<I', d, dd + 14 * 8)[0]
for i in range(nsec):
    vsz, va, rawsz, raw = struct.unpack_from('<IIII', d, opt + optsz + 40 * i + 8)
    if va <= rva < va + max(vsz, rawsz):
        off = raw + rva - va + 16
        struct.pack_into('<I', d, off, struct.unpack_from('<I', d, off)[0] & ~1)
        open(p, 'wb').write(d); sys.exit(0)
sys.exit(1)
PY
done
for a in x86 x64; do
    echo "== $a"
    out=$(cd "$B/$a" && timeout -s KILL 120 "$WINE" mixpin.exe mixpinlib.dll 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    expect() {   # expect KEY VALUE WHAT
        if printf '%s\n' "$out" | grep -qxF "$1=$2"; then printf 'PASS  %s\n' "$3"
        else printf 'FAIL  %s: wanted %s=%s, got %s\n' "$3" "$1" "$2" "$(printf '%s\n' "$out" | grep "^$1=" || echo none)"; RC=1; fi
    }
    expect first "42,first" "the mixed-mode DLL works in the domain that loaded it"
    expect after_unload 1 "after that domain unloads, the DLL is still loaded"
    expect second "42,second" "loaded again in another domain, it works"
    expect after_second 1 "and stays loaded after that one unloads too"
    expect default "42,mixpin.exe" "and in the default domain"
    expect done 1 "the probe ran to the end"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
