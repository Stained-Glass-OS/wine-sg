#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# winex11 synthetic ConfigureNotify in a virtual desktop (patch 3305): a window manager's synthetic
# ConfigureNotify has the position in root coordinates; the driver maps it into the desktop.
# test/fastxcfg-app.c is a window in "explorer /desktop=", test/fastxcfg-xclient.c moves the desktop's
# window on the root and sends the window a synthetic event; the driver's trace (warn+x11drv) names the
# position it took.
#
#   WINE=/opt/wine-sg/bin/wine test/fastxcfg-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v gcc >/dev/null || { echo "SKIP: gcc not installed"; exit 77; }
[ -f /usr/include/X11/Xlib.h ] || { echo "SKIP: no Xlib headers"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastxcfg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/app.exe" "$HERE/fastxcfg-app.c" || { echo "FAIL  app did not build"; exit 1; }
TMPDIR=/var/tmp gcc -O1 -o "$T/xclient" "$HERE/fastxcfg-xclient.c" -lX11 || { echo "FAIL  X client did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$T"
WINEDEBUG=warn+x11drv "$WINE" explorer /desktop=sgtest,1024x768 'Z:'\$(echo "$T" | tr / '\\\\')'\\\\app.exe' 'Z:'\$(echo "$T" | tr / '\\\\')'\\\\app.out' >/dev/null 2>"$T/wine.err" </dev/null &
WP=\$!
i=0
while [ \$i -lt 600 ] && ! grep -q XID= app.out 2>/dev/null; do sleep 0.1; i=\$((i+1)); done
xid=\$(sed -n 's/^XID=//p' app.out | head -1)
echo "xid=\$xid"
./xclient \$xid 50 40 150 120 300 200
sleep 1
./xclient \$xid 30 20 200 160 300 200
sleep 1
i=0
while [ \$i -lt 300 ] && ! grep -q DONE app.out 2>/dev/null; do sleep 0.1; i=\$((i+1)); done
kill \$WP 2>/dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1280x1024x24' "$T/run.sh" > "$T/run.out" 2>&1
cat "$T/run.out"
rc=0
check() { if grep -q "$1" "$T/wine.err"; then echo "PASS  $2"; else echo "FAIL  $2"; rc=1; fi; }
check 'unexpected config (100,80)-(400,280)' "the first event (root 150,120, desktop at 50,40) is the desktop position 100,80"
check 'unexpected config (170,140)-(470,340)' "the second (root 200,160, desktop at 30,20) is 170,140"
if grep -q 'FIXME\|synthetic event mapping not implemented' "$T/wine.err"; then echo "FAIL  the not-implemented FIXME is logged"; rc=1; else echo "PASS  no not-implemented FIXME"; fi
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
