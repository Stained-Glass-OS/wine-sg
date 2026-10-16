#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# loadperf: LoadPerfCounterTextStringsW / UnloadPerfCounterTextStringsW /
# InstallPerfDllW (patch 5533). lodctr's .ini + symbol file put counter and
# help strings into the registry (Perflib\<lang> Counter/Help, indices after
# "Last Counter"/"Last Help") and the range into the driver's Performance key;
# unlodctr takes the strings out again and keeps Perflib's Last values.
# Wine's Perflib\009 is the virtual text key, so English is kept in
# Perflib\CurrentLanguage and answered through HKEY_PERFORMANCE_TEXT.
#
#   WINE=/opt/wine-sg/bin/wine test/loadperf-gate.sh
# Mutant: SG_MUTANT_PERF_KEEPTEXT (dlls/loadperf: unload keeps the strings).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-loadperf.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
export HOME="$T/home" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
mkdir -p "$HOME"

cat > "$T/expect" <<'X'
load1 0
test1: 1848 1852 1849 1853
009 Counter: 1848=Sg Object 1850=Sg One 1852=Sg Two
009 Help: 1849=Sg object help 1851=Sg one help 1853=Sg two help
007 Counter: 1848=Sg Objekt 1850=Sg Eins
007 Help: 1849=Sg Objekthilfe 1851=Sg Einshilfe
install2 0
test2: 1854 1854 1855 1855
reinstall1 0
test1b: 1856 1860 1857 1861
unload1 0
test1c: - - - -
009 Counter: 1854=Other Object
007 Counter:
perflib last: 1860 1861
unload-again 2
load-missing 2
X

for a in 64:x86_64 32:i686; do
    b=${a%%:*}; cc=${a##*:}-w64-mingw32-gcc
    "$cc" -O2 -o "$T/probe$b.exe" "$HERE/loadperf-probe.c" -lntdll -ladvapi32 || { echo "FAIL  build $b"; exit 1; }
    export WINEPREFIX="$T/prefix$b"
    DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    cp "$T/probe$b.exe" "$WINEPREFIX/drive_c/"
    (cd "$WINEPREFIX/drive_c" && DISPLAY= timeout -s KILL 120 "$WINE" "./probe$b.exe" >"$T/out$b.txt" 2>&1 </dev/null)
    tr -d '\r' <"$T/out$b.txt" | grep -v 'XDG_RUNTIME' >"$T/got$b.txt"
    if cmp -s "$T/expect" "$T/got$b.txt"; then
        echo "PASS  $b-bit: strings installed, indices allocated, replaced, removed, errors"
    else
        echo "FAIL  $b-bit output differs:"; diff "$T/expect" "$T/got$b.txt" | head -12; RC=1
    fi
    "$WINESERVER" -k 2>/dev/null
done
[ $RC = 0 ] && echo "loadperf-gate: all passed" || echo "loadperf-gate: FAILURES"
exit $RC
