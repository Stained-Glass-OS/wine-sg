#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Configuration manager notifications and service device events
# (patches/sg/1698), 64- and 32-bit: test/cmnotify-probe.c registers with
# CM_Register_Notification (CR_CALL_NOT_IMPLEMENTED before) for an interface
# class, all classes, a device instance, all of them and a handle, sends
# what Wine's PnP manager sends through Wine's plug and play service (a
# client widl makes from test/cmnotify-plugplay.idl), and runs itself as a
# service registered with RegisterDeviceNotification and its status handle
# (a FIXME that never told the service anything).
#
#   WINE=/opt/wine-sg/bin/wine test/cmnotify-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   WIDL=... when it is not beside $WINE or in the build tree's tools/widl
# Mutants: SG_MUTANT_NO_CM_NOTIFY (cfgmgr32/main.c),
# SG_MUTANT_NO_SERVICE_DEVEVENT, SG_MUTANT_NO_LISTEN_WAIT (sechost/service.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
WIDL="${WIDL:-$(dirname "$WINE")/widl}"
[ -x "$WIDL" ] || WIDL="$(dirname "$WINE")/tools/widl/widl"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x "$WIDL" ] || { echo "SKIP: no widl at $WIDL"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cmnotify.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$WIDL" --win64 -c -C "$T/pp64_c.c" -h -H "$T/pp.h" "$HERE/cmnotify-plugplay.idl" &&
    "$WIDL" --win32 -c -C "$T/pp32_c.c" -H "$T/pp.h" "$HERE/cmnotify-plugplay.idl" || { echo "FAIL  widl"; exit 1; }
LIBS="-lsetupapi -lcfgmgr32 -ladvapi32 -lrpcrt4 -lntdll -luser32"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -I"$T" -o "$T/p64.exe" "$HERE/cmnotify-probe.c" "$T/pp64_c.c" $LIBS &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -I"$T" -o "$T/p32.exe" "$HERE/cmnotify-probe.c" "$T/pp32_c.c" $LIBS || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 180 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
