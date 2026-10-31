#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Storage.Provider.StorageProviderSyncRootManager (patches/sg/6222): a sync root registered
# through it is kept where cldapi keeps its own and reads back by id, by folder and in the current
# list; CfConnectSyncRoot accepts it; Unregister removes it. OneDrive registers its folder this way.
# test/syncroot-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/syncroot-gate.sh
# Mutant: SG_MUTANT_SYNC_ROOT_NOT_KEPT (dlls/windows.storage/provider.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-syncroot.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/syncroot-probe.exe" "$HERE/syncroot-probe.c" -lruntimeobject -lole32 -luuid -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/syncroot-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" syncroot-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^supported=1$' "IsSupported"
chk '^register=00000000$' "Register"
chk '^byid=Probe Cloud,defs=1:7,context=4,path=C:\\sync$' "by id: its name, property definition, context and folder"
chk '^byfolder=00000000,Probe!S-1-5-21!Account1$' "by a folder under it"
chk '^outside=80070490$' "a folder outside it: not found"
chk '^current=1$' "in the current sync roots"
chk '^regpath=C:\\sync$' "kept where cldapi reads sync roots"
chk '^connect=00000000$' "CfConnectSyncRoot connects to it"
chk '^unregister=00000000,80070490$' "Unregister removes it"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
