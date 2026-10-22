#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll (patch 5957): a native DLL mapped only to be read (LOAD_LIBRARY_AS_IMAGE_RESOURCE:
# GetFileVersionInfo, icons, strings) is that file, even when Wine has a builtin of the same
# name; mapped to run, the builtin still takes its place. Windows Installer read an
# application's own vcruntime140.dll as the builtin's newer version and kept it at every
# uninstall (Temurin, Corretto, Foxit; qa5 2026-10-22).
#   - a DLL of ours named vcruntime140.dll, version 9.8.7.6, in an application folder:
#     GetFileVersionInfo gives 9.8.7.6, its string resource is its own
#   - the same file named plain.dll (no builtin): 9.8.7.6 (control)
#   - LoadLibrary of it still gives the builtin
# Mutant SG_MUTANT_READVIEW_BUILTIN (dlls/ntdll/unix/virtual.c).
#
#   WINE=/opt/wine-sg/bin/wine test/readview-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
command -v "$MINGW" >/dev/null && command -v "$WINDRES" >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-readview.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$T/app"
export TMPDIR=/var/tmp
"$MINGW" -municode -O1 -o "$T/readview-probe.exe" "$HERE/readview-probe.c" -lversion \
    && "$WINDRES" -o "$T/res.o" "$HERE/readview-dll.rc" \
    && printf 'int __stdcall DllMain(void *h, unsigned r, void *p) { return 1; }\n' > "$T/dll.c" \
    && "$MINGW" -shared -nostdlib -e DllMain -o "$T/app/vcruntime140.dll" "$T/dll.c" "$T/res.o" \
    || { echo "FAIL  probe did not build"; exit 1; }
cp "$T/app/vcruntime140.dll" "$T/app/plain.dll"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
A=$(printf 'Z:%s' "$T/app" | tr / '\\')
out=$(timeout -s KILL 120 "$WINE" "$T/readview-probe.exe" "$A\\vcruntime140.dll" "$A\\plain.dll" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/   /'
v1=$(printf '%s\n' "$out" | sed -n '1p'); v2=$(printf '%s\n' "$out" | sed -n '2p')
[ "$v2" = "version 9.8.7.6" ] && pass "a DLL with no builtin of its name: its own version" || fail "control plain.dll: $v2"
[ "$v1" = "version 9.8.7.6" ] && pass "an application's vcruntime140.dll read for its version: its own (9.8.7.6), not the builtin's" \
    || fail "an application's vcruntime140.dll read for its version: $v1"
printf '%s\n' "$out" | grep -qx 'resource readview' && pass "its resources, read as an image resource, are its own" \
    || fail "its resources: $(printf '%s\n' "$out" | grep '^resource')"
printf '%s\n' "$out" | grep -qx 'load builtin' && pass "loaded to run, the builtin still takes its place" \
    || fail "loaded to run: $(printf '%s\n' "$out" | grep '^load')"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
