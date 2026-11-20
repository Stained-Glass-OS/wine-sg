#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# LdrLoadDll called the Windows way (wine-sg 7725): the first parameter is a search path or, below 64K,
# LOAD_LIBRARY_SEARCH_* flags (bit 0 a tag), the second points to DllCharacteristics. Programs that hook
# LoadLibrary call the original like that -- Parsec's client asks for mfplat.dll with
# LOAD_LIBRARY_SEARCH_SYSTEM32 -- and Wine read the flags as a string: wcslen(0x800), a crash every
# second. 64- and 32-bit; test/ldrflags-probe.c with a DLL that is only beside the probe.
#   WINE=/opt/wine-sg/bin/wine test/ldrflags-gate.sh
# Mutant: SG_MUTANT_LDR_FLAGS_PATH (dlls/ntdll/loader.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ldrflags.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/p64" "$T/p32"
printf '__declspec(dllexport) int attached;\nint __stdcall DllMain(void *h, unsigned long r, void *p) { if (r == 1) attached = 1; return 1; }\n' > "$T/dll.c"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64/probe.exe" "$HERE/ldrflags-probe.c" -lntdll &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32/probe.exe" "$HERE/ldrflags-probe.c" -lntdll &&
    TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -shared -nostdlib -e DllMain -o "$T/p64/sgldrflags.dll" "$T/dll.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -shared -nostdlib -e _DllMain@12 -o "$T/p32/sgldrflags.dll" "$T/dll.c" &&
    for n in 2 3; do cp "$T/p64/sgldrflags.dll" "$T/p64/sgldrflags$n.dll" && cp "$T/p32/sgldrflags.dll" "$T/p32/sgldrflags$n.dll" || exit 1; done ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    out=$(cd "$T/$p" && timeout -s KILL 120 "$WINE" "$T/$p/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
