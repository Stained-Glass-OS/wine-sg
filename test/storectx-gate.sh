#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Services.Store.StoreContext (patches/sg/6150): a machine with no Store account and nothing
# bought -- no products or add-ons, an empty collection, an unlicensed non-trial app licence,
# purchases NotPurchased, no updates; each a completed operation. There was no StoreContext:
# NanaZip's sponsor check ended the program. test/storectx-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/storectx-gate.sh
# Mutant (dlls/windows.services.store/main.c): SG_MUTANT_STORE_NO_EMPTY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-storectx.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/storectx-probe.exe" "$HERE/storectx-probe.c" -lruntimeobject -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/storectx-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" storectx-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^factory=00000000$' "Windows.Services.Store.StoreContext's statics"
chk '^default=00000000 context=1$' "GetDefault"
chk '^products=1,0,00000000$' "GetStoreProductsAsync: completed, no products, ExtendedError S_OK"
chk '^addons=1,0$' "GetAssociatedStoreProductsAsync: no add-ons"
chk '^collection=1,0$' "GetUserCollectionAsync: nothing bought"
chk '^license=1,0,0,0,0$' "GetAppLicenseAsync: not active, not a trial, no add-on licences"
chk '^purchase=1,2$' "RequestPurchaseAsync: NotPurchased"
chk '^updates=1,0$' "GetAppAndOptionalStorePackageUpdatesAsync: no updates"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
