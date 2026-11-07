#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Network services Windows has running (patches/sg/6266): DNS Client (Dnscache), IP Helper (iphlpsvc), Network List
# Service (netprofm) and WinHTTP Web Proxy Auto-Discovery Service (WinHttpAutoProxySvc), with Windows' display names
# and start types, shared-process services of svchost; a service depending on all four is started after them, and
# they run. Tailscale's installer stopped at its CheckServiceDependencies (Dnscache missing, 1060), and its service
# names the four as dependencies. The service manager starts a service's dependencies first, as Windows does (it
# did not), and fails with ERROR_SERVICE_DEPENDENCY_FAIL when one cannot start. cfgmgr32's SwDeviceCreate and DevCreateObjectQuery fail cleanly (Wintun's delay
# loads of them ended tailscaled.exe). test/netsvcs-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/netsvcs-gate.sh
# Mutants: SG_MUTANT_NETSVC_NOT_RUNNING (dlls/sgnetsvc/main.c), SG_MUTANT_NO_DEPENDENCY_START (programs/services).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-netsvcs.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/netsvcs-probe.exe" "$HERE/netsvcs-probe.c" -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/netsvcs-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" netsvcs-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^Dnscache=1,1,1$' "DNS Client: display name, automatic start, a shared-process service"
chk '^iphlpsvc=1,1,1$' "IP Helper"
chk '^netprofm=1,1,1$' "Network List Service, started on demand"
chk '^WinHttpAutoProxySvc=1,1,1$' "WinHTTP Web Proxy Auto-Discovery Service, started on demand"
chk '^started=1$' "a service depending on the four starts"
chk '^running=1,4$' "it runs, and so do the four"
chk '^depfail=1,0,1068$' "a dependency that cannot start (disabled): the service is not started, ERROR_SERVICE_DEPENDENCY_FAIL"
chk '^swdevice=1,1,80070032,1$' "SwDeviceCreate and DevCreateObjectQuery exist and fail cleanly"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
