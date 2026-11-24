#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvcm80, msvcm90: the C++/CLI runtime's helpers (wine-sg 9455). They were stubs, so a mixed-mode module
# that called one aborted ("unimplemented function msvcm90.dll..."):
#   - the C++ library's _Mtxinit/_Mtxlock/_Mtxunlock/_Mtxdst, _Once, _Cosh/_Sinh (msvcm80) and __Wcrtomb_lk are
#     msvcp80's/msvcp90's
#   - the managed new handler (_set_new_handler, __query_new_handler_m) and set_terminate are kept and
#     handed back
#   - __get_default_appdomain: an error without a runtime; with one, its default application domain,
#     released by __release_appdomain
# The probe (test/msvcmrt-probe.c), 64- and 32-bit, for msvcm90 and msvcm80.
#   WINE=/opt/wine-sg/bin/wine test/msvcmrt-gate.sh
# Mutant (dlls/msvcm80): SG_MUTANT_MSVCM_NO_APPDOMAIN.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ARCHS="x86_64 i686"
for a in $ARCHS; do command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc not installed"; exit 77; }; done
RC=0
T=$(mktemp -d /var/tmp/sg-msvcmrt.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER DISPLAY=
unset WAYLAND_DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/msvcmrt-probe.c" -luuid || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in $ARCHS; do
    for m in msvcm90 msvcm80; do
        echo "== $a $m"
        out=$(cd "$T" && timeout -s KILL 180 "$WINE" "$T/probe-$a.exe" $m.dll 2>/dev/null </dev/null | tr -d '\r')
        printf '%s\n' "$out" | sed 's/^/      /'
        expect() {   # expect KEY VALUE WHAT
            if printf '%s\n' "$out" | grep -qxF "$1=$2"; then printf 'PASS  %s\n' "$m: $3"
            else printf 'FAIL  %s: wanted %s=%s, got %s\n' "$m: $3" "$1" "$2" "$(printf '%s\n' "$out" | grep "^$1=" || echo none)"; RC=1; fi
        }
        expect loaded 1 "loads"
        expect mtxinit "1 -1" "_Mtxinit initializes a critical section"
        expect mtxlock "1 1" "_Mtxlock enters it"
        expect mtxunlock "1 0" "_Mtxunlock leaves it"
        expect mtxdst 1 "_Mtxdst deletes it"
        expect once "1 1 2" "_Once runs the function once"
        if [ $m = msvcm80 ]; then   # msvcm90 has no _Cosh and _Sinh
            expect cosh "1 3.0862" "_Cosh(x, y) is y cosh x"
            expect sinh "1 2.3504" "_Sinh(x, y) is y sinh x"
        fi
        expect wcrtomb "1 1 A" "__Wcrtomb_lk converts a character"
        expect newhandler "1 1 1 1" "the managed new handler is kept and handed back"
        expect terminate "1 1" "set_terminate keeps the managed handler"
        expect nodomain 1 "__get_default_appdomain without a runtime: an error"
        expect domain "1 1" "with a runtime: its default domain"
        expect domain_unk 1 "which is an object, released by __release_appdomain"
        expect done 1 "the probe ran to the end"
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
