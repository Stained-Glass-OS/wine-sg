#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A user's Software\Classes as on Windows 7 and later (6248): CLSID, DirectShow, Interface, Media Type
# and MediaFoundation are redirected for 32-bit programs (HKCU\Software\Classes\Wow6432Node), the rest
# is shared. Wine had no Wow6432Node in a user's hive, so OneDrive's 32-bit registration wrote the
# i386 FileSyncShell.dll over the 64-bit CLSID key and Explorer loaded none of its overlay handlers.
# A hive from before the change gets a 32-bit view that starts as a copy of the 64-bit keys.
# test/userclasswow-probe.c (built 32- and 64-bit).
# Mutants (server/registry.c): SG_MUTANT_NO_USER_CLASSES_WOW, SG_MUTANT_NO_CLASSES_MIGRATE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $cc >/dev/null || { echo "SKIP: $cc missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-userclasswow.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/userclasswow-probe.c" -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/userclasswow-probe.c" -ladvapi32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
# a hive from before: no Wow6432Node, and a 32-bit program's server in the 64-bit key
U="$WINEPREFIX/user.reg"
awk 'BEGIN{skip=0} /^\[/{skip=(index(tolower($0),"software\\\\classes\\\\wow6432node")==2)} !skip{print}' "$U" > "$U.new"
printf '\n[Software\\\\Classes\\\\CLSID\\\\{5B1D0E11-2B6D-4C11-9A01-0E57A11062AA}\\\\InprocServer32] 1700000000\n@="old"\n' >> "$U.new"
mv "$U.new" "$U"
grep -qi 'classes\\\\wow6432node' "$U" && { echo "FAIL  could not make an old hive"; exit 1; }
run() { timeout -s KILL 60 "$WINE" "$T/$1.exe" "$2" 2>/dev/null </dev/null | tr -d '\r'; }
run p32 w32 >/dev/null
run p64 w64 >/dev/null
r64=$(run p64 r64); r32=$(run p32 r32)
printf '%s\n' "$r64" "$r32" | sed 's/^/      /'
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
chk() { v="$1"; line="$2"; shift 2; printf '%s\n' "$v" | grep -qxF "$line" && pass "$*" || fail "$* (want '$line')"; }
chk "$r64" 'CLSID64 x64' "a 64-bit program's server stays in the 64-bit CLSID key"
chk "$r64" 'NODE x86' "a 32-bit program's server is in HKCU\\Software\\Classes\\Wow6432Node\\CLSID"
chk "$r64" 'CLSID32KEY x86' "KEY_WOW64_32KEY opens the 32-bit key"
chk "$r64" 'HKCR64 x64' "HKEY_CLASSES_ROOT of a 64-bit program has the 64-bit server"
chk "$r32" 'CLSID x86' "a 32-bit program reads its own server"
chk "$r32" 'HKCR x86' "HKEY_CLASSES_ROOT of a 32-bit program has the 32-bit server"
chk "$r64" 'SHARED shared32' "a file type a 32-bit program registers is shared"
chk "$r32" 'SHAREDCR shared32' "and seen through HKEY_CLASSES_ROOT"
chk "$r64" 'KIDS 11111 0' "Wow6432Node has CLSID, DirectShow, Interface, Media Type, MediaFoundation and nothing shared"
chk "$r64" 'IF64 if64' "Interface: the 64-bit key"
chk "$r64" 'IF32 if32' "Interface with KEY_WOW64_32KEY: the 32-bit key"
chk "$r32" 'IF if32' "a 32-bit program reads the 32-bit Interface key"
chk "$r32" 'OLD old' "a hive from before: a 32-bit program still finds a server it registered before"
chk "$r64" 'OLD64 old' "and the 64-bit key keeps it"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
