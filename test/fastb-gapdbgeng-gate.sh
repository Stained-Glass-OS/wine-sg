#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dbgeng gap batch (patch 3636): CreateClient, DEBUG_REQUEST create options / implicit command line / captured event code, RemoveSyntheticSymbol, kernel connection options.
#
#   WINE=/path/to/build/wine test/fastb-dbgeng-gate.sh      SG_XVFB_N=261 to name the display
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
BUILD=$(CDPATH='' cd -- "$(dirname -- "$WINE")" && pwd)
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x "$BUILD/tools/winegcc/winegcc" ] || { echo "SKIP: needs a build tree with winegcc"; exit 77; }
SRC=$(sed -n 's/^srcdir *= *//p' "$BUILD/Makefile" | head -1)
case "$SRC" in /*) ;; *) SRC="$BUILD/$SRC" ;; esac
T=$(mktemp -d /var/tmp/sg-fastb-gapdbgeng.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rc=0
for arch in x86_64 i386; do
  case $arch in i386) triple=i686-w64-mingw32 ;; *) triple=x86_64-w64-mingw32 ;; esac
  [ -f "$BUILD/dlls/dbgeng/$arch-windows/libdbgeng.a" ] || { echo "SKIP $arch: not built"; continue; }
  command -v $triple-gcc >/dev/null || { echo "SKIP $arch: no $triple-gcc"; continue; }
  ( cd "$BUILD" && tools/winegcc/winegcc -o "$T/probe-$arch.exe" --wine-objdir . -b $triple -Iinclude -I"$SRC/include" \
      -I"$SRC/include/msvcrt" -D_MSVCR_VER=0 -D__WINE_PE_BUILD -gdwarf-4 -O0 -mno-cygwin -Wno-format \
      "$HERE/fastb-gapdbgeng-probe.c" dlls/dbgeng/$arch-windows/libdbgeng.a dlls/winecrt0/$arch-windows/libwinecrt0.a \
      dlls/msvcrt/$arch-windows/libmsvcrt.a dlls/kernel32/$arch-windows/libkernel32.a dlls/ntdll/$arch-windows/libntdll.a ) \
      >"$T/build-$arch.log" 2>&1 || { cat "$T/build-$arch.log"; echo "FAIL  probe did not build ($arch)"; rc=1; continue; }
  cp "$T/probe-$arch.exe" "$WINEPREFIX/drive_c/probe.exe"
  cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" probe.exe 2>/dev/null </dev/null | tr -d '\\r' > "$T/probe-$arch.out"
EOS
  chmod +x "$T/run.sh"
  XN=${SG_XVFB_N:+-n $SG_XVFB_N}
  [ -n "$XN" ] || XN=-a
  timeout -s KILL 400 xvfb-run $XN -s '-screen 0 1024x768x24' "$T/run.sh"
  "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
  grep -v '^ok' "$T/probe-$arch.out"
  grep -qx 'RESULT: PASS' "$T/probe-$arch.out" || { echo "FAIL  $arch"; rc=1; }
done
[ $rc = 0 ] && echo "RESULT: PASS"
exit $rc
