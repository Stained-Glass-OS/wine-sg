#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# services.exe gap-A batch (patch 3213): the ANSI RPC entry points of the SCM interface (OpenSCManagerA ...
# QueryServiceConfig2A, EnumServicesStatus[Ex]A, EnumDependentServicesA, CreateService[WOW64]A, StartServiceA,
# Get*NameA), EnumServiceGroupW, ControlServiceEx[AW], QueryServiceConfigEx, QueryServiceStatus,
# QueryServiceLockStatus[AW], NotifyBootConfigStatus, SERVICE_CONFIG_PREFERRED_NODE, reserved opnums.
#
#   WINE=/opt/wine-sg/bin/wine test/fastservices-gate.sh        (MINGW=i686-w64-mingw32-gcc for 32 bit)
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   WIDL=... SVCCTL_INC=<wine include dir>   to generate the client stubs (defaults: /opt/wine-sg)
# Mutants (programs/services/rpc.c): SG_MUTANT_SVC_* (ENUMA DNAMEA PREFNODE CTLEX CFGEX BOOT LOCK GROUP CFG2A CREATEA).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WIDL="${WIDL:-/opt/wine-sg/bin/widl}"
SVCCTL_INC="${SVCCTL_INC:-/opt/wine-sg/include}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x "$WIDL" ] || { echo "SKIP: no widl at $WIDL"; exit 77; }
IDL="$SVCCTL_INC/wine/svcctl.idl"
[ -f "$IDL" ] || IDL="$SVCCTL_INC/wine/windows/../svcctl.idl"
[ -f "$IDL" ] || { echo "SKIP: no svcctl.idl under $SVCCTL_INC"; exit 77; }
case "$MINGW" in i686*) WIDLARCH=--win32 ;; *) WIDLARCH=--win64 ;; esac
T=$(mktemp -d /var/tmp/sg-fastservices.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG="${SG_GATE_DEBUG:--all}" WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$T/inc"
# mingw's winsvc.h lacks a few of the types the interface uses
cat > "$T/inc/winsvc.h" <<'EOS'
#ifndef SHIM_WINSVC_H
#define SHIM_WINSVC_H
#include_next <winsvc.h>
typedef struct _SERVICE_TRIGGER_SPECIFIC_DATA_ITEM { DWORD dwDataType; DWORD cbData; BYTE *pData; } SERVICE_TRIGGER_SPECIFIC_DATA_ITEM, *PSERVICE_TRIGGER_SPECIFIC_DATA_ITEM;
typedef struct _SERVICE_TRIGGER { DWORD dwTriggerType; DWORD dwAction; GUID *pTriggerSubtype; DWORD cDataItems; SERVICE_TRIGGER_SPECIFIC_DATA_ITEM *pDataItems; } SERVICE_TRIGGER, *PSERVICE_TRIGGER;
typedef struct _SERVICE_TRIGGER_INFO { DWORD cTriggers; SERVICE_TRIGGER *pTriggers; BYTE *pReserved; } SERVICE_TRIGGER_INFO, *PSERVICE_TRIGGER_INFO;
typedef struct _SERVICE_PREFERRED_NODE_INFO { USHORT usPreferredNode; BOOLEAN fDelete; } SERVICE_PREFERRED_NODE_INFO, *LPSERVICE_PREFERRED_NODE_INFO;
typedef struct _SERVICE_LAUNCH_PROTECTED_INFO { DWORD dwLaunchProtected; } SERVICE_LAUNCH_PROTECTED_INFO, *PSERVICE_LAUNCH_PROTECTED_INFO;
#endif
EOS
# (widl writes -o names into the current directory)
( cd "$T" && "$WIDL" -I"$SVCCTL_INC" -I"$SVCCTL_INC/wine" $WIDLARCH -c -o svcctl_c.c -h -H svcctl.h "$IDL" ) \
    || { echo "FAIL  widl could not generate the client stubs"; exit 1; }
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastservices-probe.exe" "$HERE/fastservices-probe.c" "$T/svcctl_c.c" -I"$T" -I"$T/inc" \
    -lrpcrt4 -ladvapi32 -Wl,--kill-at || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastservices-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 240 "$WINE" fastservices-probe.exe 2>"$T/err" </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
