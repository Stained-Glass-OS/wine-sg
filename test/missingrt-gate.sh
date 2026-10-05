#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A missing DLL SG Store offers is named for sg-notify's question (patches/sg
# /0817): Meedio's plug-ins import msvbvm60.dll, the Visual Basic 6 runtime
# Windows includes and Wine does not have; LoadLibrary failed and nobody
# said why. The loader now names such a DLL -- one HKLM\Software\Stained
# Glass\Store\Runtimes lists -- with the module that needed it in
# HKCU\Software\Stained Glass\Runtimes\Missing; any other missing DLL is not
# named. A 32-bit program (as Meedio's is): the store's 64-bit keys are read.
#
#   WINE=/opt/wine-sg/bin/wine test/missingrt-gate.sh   (mutant SG_MUTANT_NO_RUNTIME_NOTE)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
DLLTOOL="${DLLTOOL:-i686-w64-mingw32-dlltool}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in "$MINGW32" "$DLLTOOL"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-missingrt.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
(cd "$T" && printf 'LIBRARY msvbvm60.dll\nEXPORTS\nsg_runtime_entry@0\n' > vb.def && printf 'LIBRARY sgnosuch.dll\nEXPORTS\nsg_runtime_entry@0\n' > no.def &&
 "$DLLTOOL" -k -d vb.def -l libvb.a && "$DLLTOOL" -k -d no.def -l libno.a &&
 "$MINGW32" -shared -O2 -o VbPlugin.dll "$HERE/missingrt-plugin.c" libvb.a &&
 "$MINGW32" -shared -O2 -o OtherPlugin.dll "$HERE/missingrt-plugin.c" libno.a &&
 "$MINGW32" -O2 -o probe.exe "$HERE/missingrt-probe.c") || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/VbPlugin.dll" "$T/OtherPlugin.dll" "$T/probe.exe" "$WINEPREFIX/drive_c/"
# what sg-shell's store says it offers (defaults/85-sg-store.reg)
"$WINE" reg add 'HKLM\Software\Stained Glass\Store\Runtimes' /v msvbvm60.dll /d 127 /f /reg:64 >/dev/null 2>&1
"$WINESERVER" -w
out=$("$WINE" 'C:\probe.exe' 'C:\VbPlugin.dll' 'C:\OtherPlugin.dll' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
miss=$("$WINE" reg query 'HKCU\Software\Stained Glass\Runtimes\Missing' 2>/dev/null | tr -d '\r')
printf '%s\n' "$miss" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q 'load C:\\VbPlugin.dll 0' || fail "the plug-in loaded (is msvbvm60.dll in the prefix?)"
printf '%s\n' "$miss" | grep -qi 'msvbvm60.dll.*REG_SZ.*VbPlugin.dll' \
    && pass "a plug-in needing msvbvm60.dll (offered by the store) is named, with the plug-in" || fail "msvbvm60.dll not named"
printf '%s\n' "$miss" | grep -qi 'sgnosuch' && fail "a DLL the store does not offer was named" \
    || pass "a missing DLL the store does not offer is not named"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
