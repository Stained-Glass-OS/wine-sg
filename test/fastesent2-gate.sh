#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# esent: a database written by a 64-bit program is read by a 32-bit one and back
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
M64=x86_64-w64-mingw32-gcc; M32=i686-w64-mingw32-gcc
command -v $M64 >/dev/null && command -v $M32 >/dev/null || { echo "SKIP: needs both mingw compilers"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastesent2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp $M64 -O1 -o "$T/p64.exe" "$HERE/fastesent2-probe.c" -lesent || { echo "FAIL probe 64"; exit 1; }
TMPDIR=/var/tmp $M32 -O1 -o "$T/p32.exe" "$HERE/fastesent2-probe.c" -lesent || { echo "FAIL probe 32"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/p64.exe" "$T/p32.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
for step in "p64 write" "p32 read" "p32 write" "p64 read"; do
  set -- \$step
  echo "\$1 \$2: \$(timeout -s KILL 120 "$WINE" \$1.exe \$2 2>/dev/null </dev/null | tr -d '\r' | tail -1)"
done > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
[ "$(grep -c 'RESULT: PASS' "$T/probe.out")" = 4 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
