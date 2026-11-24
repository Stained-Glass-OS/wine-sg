#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcm80, msvcm90: <CrtImplementationDetails>::DoCallBackInDefaultDomain (wine-sg 9453). A C++/CLI module
# first loaded in an application domain other than the default one runs its native and per-process
# initialization in the default domain through this call; it was a stub, so the module's startup
# aborted the program ("unimplemented function msvcm90.dll.?DoCallBackInDefaultDomain..."). As the
# CRT does, the callback runs in the default domain through ICLRRuntimeHost::ExecuteInAppDomain.
# The probe: test/net/mixcb.cs calls test/mixcb-probe.c (a native DLL) from the default domain and from
# a second one, 32- and 64-bit; the native DLL calls DoCallBackInDefaultDomain and reports the domain
# (ICLRRuntimeHost::GetCurrentAppDomainId) before, inside and after the callback.
#   WINE=/opt/wine-sg/bin/wine test/mixcb-gate.sh
# Mutant (dlls/msvcm80): SG_MUTANT_MSVCM_CALLBACK_HERE (the callback runs in the caller's domain).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for a in x86_64 i686; do command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc not installed"; exit 77; }; done
MCS=""
for d in "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    f=$(ls "$d"/wine-mono-*/lib/mono/4.5/mcs.exe 2>/dev/null | head -1)
    [ -n "$f" ] && MCS=$f
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-mixcb.XXXXXX)
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
    cp "$HERE/net/mixcb.cs" "$B/$a/"
    if [ $a = x86 ]; then cc=i686-w64-mingw32-gcc; else cc=x86_64-w64-mingw32-gcc; fi
    TMPDIR=/var/tmp $cc -O1 -shared -Wl,--kill-at -o "$B/$a/mixcb.dll" "$HERE/mixcb-probe.c" || { echo "FAIL  the probe DLL did not build"; exit 1; }
    (cd "$B/$a" && timeout -s KILL 300 "$WINE" "$MCS" -nologo -platform:$a -out:mixcb.exe mixcb.cs) >>"$T/mcs.log" 2>&1 </dev/null
    [ -f "$B/$a/mixcb.exe" ] || { echo "FAIL  mcs did not build the probe"; cat "$T/mcs.log"; exit 1; }
done
for a in x86 x64; do
    echo "== $a"
    out=$(cd "$B/$a" && timeout -s KILL 120 "$WINE" mixcb.exe 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    for m in msvcm90 msvcm80; do
        # before,called,inside,after: the domain's id before, inside and after the callback
        if printf '%s\n' "$out" | grep -qx "$m.default=0,1,0,0"; then printf 'PASS  %s\n' "$m: from the default domain, the callback runs (there)"
        else printf 'FAIL  %s\n' "$m: from the default domain: $(printf '%s\n' "$out" | grep "^$m.default=" || echo nothing)"; RC=1; fi
        v=$(printf '%s\n' "$out" | sed -n "s/^$m.second=//p")
        b=${v%%,*}; rest=${v#*,}
        if [ -n "$v" ] && [ "$b" != 0 ] && [ "$rest" = "1,0,$b" ]; then printf 'PASS  %s\n' "$m: from another domain, the callback runs in the default one, and the caller is back in its own"
        else printf 'FAIL  %s\n' "$m: from another domain: ${v:-nothing}"; RC=1; fi
    done
    if printf '%s\n' "$out" | grep -qx "done=1"; then printf 'PASS  %s\n' "the probe ran to the end"
    else printf 'FAIL  %s\n' "the probe did not run to the end"; RC=1; fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
