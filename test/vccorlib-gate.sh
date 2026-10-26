#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Wine's vccorlib140.dll, the C++/CX runtime (patches/sg/7009). Programs written in C++/CX import
# it; HexChat's toast plugin (hcnotifications-winrt.dll) did not load ("Library vccorlib140.DLL not
# found"). The probe (test/vccorlib-probe.c, 64-bit: the exports are the 64-bit decorated names)
# checks module start-up, activation by class name, and that each raiser throws a C++ exception
# a "catch (Platform::Exception^ e)" block takes: the catchable types are the handle types of the
# class and its bases (by decorated name), the catch's copy holds a reference, e->HResult and
# e->Message answer, and the thrown handle is released after the catch. A vectored handler does
# what the C++ runtime's catch does.
#   WINE=/opt/wine-sg/bin/wine test/vccorlib-gate.sh
# Mutant: SG_MUTANT_VCCORLIB_NO_BASE_CLASS (dlls/vccorlib140/vccorlib.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vccorlib.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/vccorlib-probe.c" -lruntimeobject \
    || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" "C:\\probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed "s/^/      /"
check() {
    if printf '%s\n' "$out" | grep -Eq "(^| )$1=$2( |$)"; then pass "$3"
    else fail "$3 ($(printf '%s\n' "$out" | grep -Eo "(^| )$1=[^ ]*" | head -1 || echo none))"; fi
}
check loaded 1 "vccorlib140.dll loads"
check exports 1 "it exports the decorated names C++/CX programs import"
check init 1 "Platform::Details::InitializeData succeeds"
check factory 1 "GetActivationFactoryByPCWSTR activates a class by name"
check factory_unknown 1 "an unknown class fails and leaves no factory"
for k in oom cast wrongthread com_mapped com_other; do
    check ${k}_thrown 1 "$k: a C++ exception is thrown"
    check ${k}_cxx 1 "$k: it is an MSVC C++ exception"
    check ${k}_type 1 "$k: it is caught as its own class"
    check ${k}_catch_exception 1 "$k: catch (Platform::Exception^) takes it"
    check ${k}_catch_com 1 "$k: catch (Platform::COMException^) takes it"
    check ${k}_catch_object 1 "$k: catch (Platform::Object^) takes it"
    check ${k}_ref_in_catch 2 "$k: the catch's copy holds its own reference"
    check ${k}_hr 1 "$k: e->HResult is the error"
    check ${k}_message 1 "$k: e->Message has text"
    check ${k}_released 1 "$k: the thrown handle is released after the catch"
done
check done 1 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
