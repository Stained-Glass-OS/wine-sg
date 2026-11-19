#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemprox/wbemdisp/wscript: WMI event queries (8002). ExecNotificationQuery failed and
# ExecNotificationQueryAsync ran the event query as a data query, so no program that watches WMI
# (.NET ManagementEventWatcher, VBScript NextEvent/SWbemSink, C++) ever got an event.
#   - __InstanceCreation/Deletion/Modification/OperationEvent WITHIN n WHERE TargetInstance ISA 'Class'
#     [AND TargetInstance.Prop ...]: polled as Windows polls classes without an event provider
#   - Win32_ProcessStartTrace/StopTrace/ProcessTrace, Win32_DeviceChangeEvent, Win32_VolumeChangeEvent
#   - semisynchronous enumerator (Next with a timeout, WBEM_S_TIMEDOUT), async sink, CancelAsyncCall
#   - SWbemServices.ExecNotificationQuery(...).NextEvent and ExecNotificationQueryAsync with an
#     SWbemSink whose OnObjectReady/OnCompleted run on the script's thread while WScript.Sleep pumps
#   - SWbemSink EnumConnectionPoints/EnumConnections
#   WINE=/opt/wine-sg/bin/wine test/wmievent-gate.sh
# Mutants: SG_MUTANT_WMI_NO_EVENTS (wbemprox/events.c), SG_MUTANT_WBEM_NO_ASYNC_EVENTS,
#   SG_MUTANT_WBEM_NO_CP_ENUM (wbemdisp), SG_MUTANT_WSH_SLEEP_NO_PUMP (wscript).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wmievent.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

for a in x86_64 i686; do
    mkdir -p "$T/$a"
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O2 -o "$T/$a/probe.exe" "$HERE/wmievent-probe.c" -municode -lole32 -loleaut32 -lwbemuuid -luuid 2>/dev/null ||
        { echo "FAIL  the $a probe did not build"; exit 1; }
done

mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
for a in x86_64 i686; do
    echo "== $a"
    timeout -s KILL 120 "$WINE" "$T/$a/probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
    tr -d '\r' <"$T/out.raw" >"$T/out"
    cat "$T/out"
    grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1
done

# VBScript: NextEvent, then an async query with an SWbemSink while WScript.Sleep
cp "$T/x86_64/probe.exe" "$WINEPREFIX/drive_c/sgvbschild.exe"
cat > "$WINEPREFIX/drive_c/ev.vbs" <<'VBS'
Set svc = GetObject("winmgmts:\\.\root\cimv2")
Set sh = CreateObject("WScript.Shell")
Set src = svc.ExecNotificationQuery("SELECT * FROM Win32_ProcessStartTrace WHERE ProcessName = 'sgvbschild.exe'")
sh.Run "C:\sgvbschild.exe child 1500", 0, False
Set e = src.NextEvent(8000)
nextname = e.ProcessName
Set sink = WScript.CreateObject("WbemScripting.SWbemSink", "SINK_")
n = 0 : nm = "" : done = 0 : code = 0
svc.ExecNotificationQueryAsync sink, "SELECT * FROM __InstanceCreationEvent WITHIN 0.5 WHERE TargetInstance ISA 'Win32_Process' AND TargetInstance.Name = 'sgvbschild.exe'"
sh.Run "C:\sgvbschild.exe child 1500", 0, False
For i = 1 To 30
  WScript.Sleep 300
  If n > 0 Then Exit For
Next
sink.Cancel
For i = 1 To 20
  WScript.Sleep 200
  If done Then Exit For
Next
WScript.Echo "next=" & nextname & " events=" & n & " name=" & nm & " completed=" & done & " code=" & Hex(code)
Sub SINK_OnObjectReady(obj, ctx)
  n = n + 1
  nm = obj.TargetInstance.Name
End Sub
Sub SINK_OnCompleted(hr, err, ctx)
  done = 1
  code = hr
End Sub
VBS
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\ev.vbs' 2>/dev/null </dev/null | tr -d '\r')
echo "vbs: $out"
case "$out" in
    *"next=sgvbschild.exe "*) echo "ok    VBScript NextEvent gives the started process" ;;
    *) echo "FAIL  VBScript NextEvent"; rc=1 ;;
esac
case "$out" in
    *"events=1 name=sgvbschild.exe"*) echo "ok    an SWbemSink's OnObjectReady ran during WScript.Sleep with the event" ;;
    *) echo "FAIL  SWbemSink OnObjectReady"; rc=1 ;;
esac
case "$out" in
    *"completed=1 code=80041032"*) echo "ok    Cancel ends with OnCompleted(wbemErrCallCancelled)" ;;
    *) echo "FAIL  OnCompleted after Cancel"; rc=1 ;;
esac

[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
