#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A file type's icon handler (its class's shellex\IconHandler) gives each file
# its own icon, as on Windows (patches/sg/0767): Wine took the type's
# DefaultIcon for every file -- a Linux app's .desktop file on the desktop
# showed the Store's icon, not the app's (Steam's, David 2026-10-02).
#   - two files of a type with a handler: each gets the handler's icon, for
#     itself (the handler loaded with that file)
#   - a type without one: its DefaultIcon, as before
#
#   WINE=/opt/wine-sg/bin/wine test/iconhandler-gate.sh
# Mutation: build shell32 with -DSG_MUTANT_NO_ICON_HANDLER: DefaultIcon.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-iconhandler.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
printf 'LIBRARY iconhandler.dll\nEXPORTS\n    DllGetClassObject PRIVATE\n    DllCanUnloadNow PRIVATE\n' > "$T/h.def"
"$MINGW" -O2 -municode -shared -o "$T/iconhandler.dll" "$HERE/iconhandler-dll.c" "$T/h.def" -lole32 -luuid -lshlwapi \
    || { fail "handler did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$T/probe.exe" "$HERE/iconhandler-probe.c" -lshell32 -lole32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/iconhandler.dll" "$T/probe.exe" "$C/"
: > "$C/alpha.sgicontest"; : > "$C/beta.sgicontest"; : > "$C/plain.sgnohandler"
cat > "$T/reg.reg" <<'REG'
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\Software\Classes\.sgicontest]
@="SG.IconTest"

[HKEY_LOCAL_MACHINE\Software\Classes\SG.IconTest\DefaultIcon]
@="C:\\default-type.ico,0"

[HKEY_LOCAL_MACHINE\Software\Classes\SG.IconTest\shellex\IconHandler]
@="{5A1C0E11-2B6D-4C11-9A01-1C0DE57A110E}"

[HKEY_LOCAL_MACHINE\Software\Classes\CLSID\{5A1C0E11-2B6D-4C11-9A01-1C0DE57A110E}\InprocServer32]
@="C:\\iconhandler.dll"
"ThreadingModel"="Apartment"

[HKEY_LOCAL_MACHINE\Software\Classes\.sgnohandler]
@="SG.NoHandler"

[HKEY_LOCAL_MACHINE\Software\Classes\SG.NoHandler\DefaultIcon]
@="C:\\default-plain.ico,3"
REG
"$WINE" regedit /S 'Z:'"$(printf '%s' "$T/reg.reg" | tr '/' '\\')" >/dev/null 2>&1
"$WINESERVER" -w
q() { "$WINE" "$C/probe.exe" "$1" 2>/dev/null | tr -d '\r'; }
a=$(q 'C:\alpha.sgicontest'); b=$(q 'C:\beta.sgicontest')
[ "$a" = 'C:\handler-alpha.ico,0' ] && [ "$b" = 'C:\handler-beta.ico,0' ] \
    && pass "each file gets its own icon from the type's icon handler ($a; $b)" || fail "handler icons: '$a' '$b'"
p=$(q 'C:\plain.sgnohandler')
[ "$p" = 'C:\default-plain.ico,3' ] && pass "a type without a handler: its DefaultIcon ($p)" || fail "no handler: '$p'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
