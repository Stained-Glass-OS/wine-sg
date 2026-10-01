#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What pywin32 imports (patches/sg/0621): looked up by name, as a UPX-packed
# module's own loader does, and then used -- the thread's COM cancel object
# (reached from another thread too), transacted file calls, EFS answers for a
# file system without encryption, the classes' associations asked in order,
# a default extract-icon object. BleachBit's pythoncom312.dll failed to load
# on the first missing one. 64-bit and 32-bit (WoW64).
#
#   WINE=/opt/wine-sg/bin/wine test/pyimports-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-pyimports.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    "$a-w64-mingw32-gcc" -O2 -o "$T/pyimports-$a.exe" "$HERE/pyimports-probe.c" -lole32 -luuid -lshlwapi ||
        { fail "probe did not build ($a)"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/pyimports-*.exe "$WINEPREFIX/drive_c/"
v() { printf '%s\n' "$1" | sed -n "s/^$2=//p"; }
for a in x86_64 i686; do
    out=$(timeout 60 "$WINE" "C:\\pyimports-$a.exe" 2>/dev/null | tr -d '\r')
    case "$(v "$out" exports)" in 23/23) pass "$a: all 23 are exported" ;;
        *) fail "$a: exports $(v "$out" exports)"; continue ;; esac
    [ "$(v "$out" cancel-none)" = 0x80004002 ] && [ "$(v "$out" cancel-set)" = "0 ref=2" ] &&
        [ "$(v "$out" cancel-pending)" = 0x80010115 ] && [ "$(v "$out" cancel-call)" = "0 seconds=7" ] &&
        [ "$(v "$out" cancel-test)" = 0x80010002 ] && [ "$(v "$out" cancel-cleared)" = "0x80004002 ref=1" ] &&
        pass "$a: the thread's cancel object: set, tested, cancelled from another thread, cleared and released" ||
        fail "$a: cancel object: none $(v "$out" cancel-none) set $(v "$out" cancel-set) pending $(v "$out" cancel-pending) call $(v "$out" cancel-call) test $(v "$out" cancel-test) cleared $(v "$out" cancel-cleared)"
    [ "$(v "$out" tx-fullpath)" = same ] && [ "$(v "$out" tx-copy)" = "1 exists=1" ] &&
        pass "$a: transacted calls do the plain call" || fail "$a: transacted: $(v "$out" tx-fullpath) / $(v "$out" tx-copy)"
    case "$(v "$out" efs-users)" in "6007 list="*" missing=2") e=1 ;; *) e=0 ;; esac
    [ $e = 1 ] && [ "$(v "$out" efs-disable)" = 1 ] &&
        pass "$a: EFS: a file is not encrypted, a missing one is missing; EncryptionDisable writes Desktop.ini" ||
        fail "$a: EFS: $(v "$out" efs-users) disable $(v "$out" efs-disable)"
    [ "$(v "$out" assoc-second)" = '0 sgtest.exe "%1"' ] && [ "$(v "$out" assoc-missing)" = fails ] &&
        pass "$a: AssocCreateForClasses asks the classes in order" ||
        fail "$a: classes: second '$(v "$out" assoc-second)' missing $(v "$out" assoc-missing)"
    [ "$(v "$out" icon-create)" = 0 ] && [ "$(v "$out" icon-normal)" = 'C:\normal.dll,3 flags=0x4' ] &&
        [ "$(v "$out" icon-open)" = 'C:\open.dll,4' ] && [ "$(v "$out" icon-shortcut)" = 'C:\normal.dll,3' ] &&
        pass "$a: the default extract-icon object gives the state's icon and its flags" ||
        fail "$a: icon: $(v "$out" icon-create) normal $(v "$out" icon-normal) open $(v "$out" icon-open) shortcut $(v "$out" icon-shortcut)"
done
exit $RC
