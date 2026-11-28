#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A task dialog's expando control ("See details" / "Hide details") was as wide as its text alone
# while it draws its arrow first: the text was cut to "See" and "Hide" in every task dialog with
# expanded information -- File Explorer's Network Error among them (patches/sg/9662). Probe
# test/taskexpando-probe.c (common controls 6) measures the control against its text.
#   WINE=... test/taskexpando-gate.sh      mutant: SG_MUTANT_EXPANDO_TEXT_ONLY (comctl32/taskdialog.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW" x86_64-w64-mingw32-windres; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-taskexpando.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX" 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null && wait "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY#:}-lock"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
cat > "$T/m.manifest" <<'M'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
<dependency><dependentAssembly><assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0"
 processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/></dependentAssembly></dependency>
</assembly>
M
printf '1 24 "%s"\n' "$T/m.manifest" > "$T/m.rc"
x86_64-w64-mingw32-windres "$T/m.rc" -O coff -o "$T/m.o" && TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/taskexpando-probe.c" "$T/m.o" -lcomctl32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
unset DISPLAY XAUTHORITY
# a display of its own, far from any session's (run it in a private network namespace with its own
# /tmp/.X11-unix: /var/tmp/sgwork/netns-run.sh)
DPY=":${SG_TASKEXPANDO_DPY:-4716}"
Xvfb "$DPY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
sleep 1
export DISPLAY="$DPY"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS'
