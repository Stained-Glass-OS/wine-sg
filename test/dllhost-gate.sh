#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# COM surrogates: dllhost.exe (patches/sg/5530, 5531). A class whose AppID
# has an (empty) DllSurrogate value runs in the system's dllhost, which must
# have the bitness of the class's server -- the 64-bit one (system32) for a
# class registered in the 64-bit registry view, the 32-bit one (syswow64) for
# the 32-bit view -- whichever the caller is. combase started syswow64's
# dllhost for every 64-bit caller, so a 64-bit class failed ("class not
# registered": the 32-bit dllhost looks in the other view). The surrogate
# also marshals the DLL's class factory with a real size bound
# (IMarshal::GetMarshalSizeMax was E_NOTIMPL).
# test/dllhost-server.c is the server (an IDispatch that answers the path of
# its process), test/dllhost-probe.c the client; 64- and 32-bit of each.
#
#   WINE=/opt/wine-sg/bin/wine test/dllhost-gate.sh
# Mutants: SG_MUTANT_DLLHOST_ARCH (dlls/combase/rpc.c: the old choice),
# SG_MUTANT_DLLHOST_SIZE (programs/dllhost: GetMarshalSizeMax E_NOTIMPL).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dllhost.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
for a in 64:x86_64 32:i686; do
    b=${a%%:*}; cc=${a##*:}-w64-mingw32-gcc
    "$cc" -O2 -shared -o "$T/sgsurr$b.dll" "$HERE/dllhost-server.c" "$HERE/dllhost-server.def" -lole32 -loleaut32 -luuid &&
    "$cc" -O2 -o "$T/probe$b.exe" "$HERE/dllhost-probe.c" -lole32 -loleaut32 -luuid || { echo "FAIL  build $b"; exit 1; }
done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T"/sgsurr64.dll "$T"/sgsurr32.dll "$T"/probe64.exe "$T"/probe32.exe "$C/"

# register: CLSID_BITS with its server in that bitness's registry view
reg() { DISPLAY= "$WINE" reg add "$@" /f >/dev/null 2>&1; }
for b in 64 32; do
    clsid="{0b8f6c10-3a77-4d2e-9a31-5c6e1f2a7b$b}"
    appid="{5d0e7a41-9c2b-4e18-8f66-1a2b3c4d5e$b}"
    reg "HKLM\\Software\\Classes\\CLSID\\$clsid" /ve /d "SgSurr$b" /reg:$b
    reg "HKLM\\Software\\Classes\\CLSID\\$clsid\\InprocServer32" /ve /d "C:\\sgsurr$b.dll" /reg:$b
    reg "HKLM\\Software\\Classes\\CLSID\\$clsid" /v AppID /d "$appid" /reg:$b
    reg "HKLM\\Software\\Classes\\AppID\\$appid" /v DllSurrogate /d "" /reg:$b
done

for pb in 64 32; do
    for cb in 64 32; do
        clsid="{0b8f6c10-3a77-4d2e-9a31-5c6e1f2a7b$cb}"
        # a file, not a pipe: the surrogate inherits the output and outlives the probe
        (cd "$C" && WINEDEBUG=+dllhost,fixme-all DISPLAY= timeout -s KILL 90 "$WINE" "./probe$pb.exe" "$clsid" >"$T/out.txt" 2>&1 </dev/null)
        out=$(tr -d '\r' <"$T/out.txt")
        "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
        want=$([ $cb = 64 ] && echo 'system32\dllhost.exe' || echo 'syswow64\dllhost.exe')
        if printf '%s\n' "$out" | grep -q "^invoke 00000000 .*$(printf '%s' "$want" | sed 's/\\/\\\\/g')\$"; then
            pass "a $pb-bit caller, a $cb-bit class: runs in $want"
        else
            fail "$pb-bit caller, $cb-bit class: $(printf '%s\n' "$out" | grep '^create\|^invoke' | tr '\n' ' ')"
        fi
        if [ $pb = 64 ] && [ $cb = 64 ]; then
            if printf '%s\n' "$out" | grep -q "returning 0, size [1-9][0-9]*" && ! printf '%s\n' "$out" | grep -q "fixme:dllhost"; then
                pass "the surrogate marshals the DLL's class factory with a size bound (GetMarshalSizeMax)"
            else
                fail "GetMarshalSizeMax: $(printf '%s\n' "$out" | grep 'GetMarshalSizeMax\|returning' | head -3 | tr '\n' ' ')"
            fi
            printf '%s\n' "$out" | grep -q "^factory-create 00000000" && pass "and its class factory creates the object through the proxy" || fail "factory-create: $(printf '%s\n' "$out" | grep '^getclass\|^factory')"
        fi
    done
done

[ $RC = 0 ] && echo "dllhost-gate: all passed" || echo "dllhost-gate: FAILURES"
exit $RC
