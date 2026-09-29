#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ntdll's AVL generic tables and RtlIsNameInExpression (patches/sg/0498),
# kernelbase's FindFirstFileNameW / FindNextFileNameW (0499). Office's Word
# runs inside App-V's subsystems (AppVIsvSubsystems64.dll), which keep their
# maps in Rtl*GenericTableAvl tables -- stubs that held nothing -- match
# paths with RtlIsNameInExpression, and look for a file's names; Word handed
# off to Click-to-Run's error UI at once. A table takes 1000 elements in no
# order, finds, refuses a duplicate, enumerates in order (both ways), gets
# the n-th, deletes (freeing through the caller's routine); expressions with
# * ? and the DOS wildcards match as the file systems do; a file's first
# name is its path from the volume root, and there is no next.
#
#   WINE=/opt/wine-sg/bin/wine test/avl-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-avl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
printf 'LIBRARY ntdll.dll\nEXPORTS\nRtlInitializeGenericTableAvl\nRtlInsertElementGenericTableAvl\nRtlLookupElementGenericTableAvl\nRtlDeleteElementGenericTableAvl\nRtlEnumerateGenericTableAvl\nRtlEnumerateGenericTableWithoutSplayingAvl\nRtlGetElementGenericTableAvl\nRtlNumberGenericTableElementsAvl\nRtlIsGenericTableEmptyAvl\nRtlIsNameInExpression\nRtlInitUnicodeString\n' > "$T/n.def"
printf 'LIBRARY kernel32.dll\nEXPORTS\nFindFirstFileNameW\nFindNextFileNameW\n' > "$T/k.def"
DLLTOOL="${DLLTOOL:-x86_64-w64-mingw32-dlltool}"
"$DLLTOOL" -d "$T/n.def" -l "$T/libn.a" && "$DLLTOOL" -d "$T/k.def" -l "$T/libk.a" || { fail "no import libraries"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/avl-probe.c" "$T/libn.a" "$T/libk.a" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }

[ "$(v empty)" = 1 ] && [ "$(v count)" = 1000 ] && pass "a table takes 1000 elements (it was empty)" || fail "count: $(v empty) $(v count)"
[ "$(v again)" = "0 1" ] && pass "a duplicate is not inserted; the element there is returned" || fail "again: $(v again)"
[ "$(v lookup)" = 777 ] && [ "$(v missing)" = 1 ] && pass "lookup finds an element, and not a missing one" || fail "lookup: $(v lookup) $(v missing)"
[ "$(v enum)" = "1000 1" ] && [ "$(v nosplay)" = "1000 1" ] && pass "enumerated in order, both ways" || fail "enum: $(v enum) / $(v nosplay)"
[ "$(v element10)" = 10 ] && pass "the n-th element" || fail "element 10: $(v element10)"
[ "$(v deleted)" = "1 count 500" ] && [ "$(v delete-again)" = 0 ] && [ "$(v frees)" = 500 ] \
    && pass "delete removes (freed by the caller's routine), and says so" || fail "delete: $(v deleted) / $(v delete-again) / frees $(v frees)"
[ "$(v allocs)" = 1000 ] && pass "one allocation an element" || fail "allocs: $(v allocs)"
[ "$(v m1)" = 1 ] && [ "$(v m2)" = 0 ] && pass "RtlIsNameInExpression: *.TXT, ignoring case or not" || fail "case: $(v m1) $(v m2)"
[ "$(v m3)" = 1 ] && [ "$(v m4)" = 0 ] && [ "$(v m5)" = 1 ] && pass "? is one character; * across a path" || fail "?/*: $(v m3) $(v m4) $(v m5)"
[ "$(v m6)" = 1 ] && [ "$(v m8)" = 1 ] && [ "$(v m9)" = 1 ] && [ "$(v m10)" = 1 ] \
    && pass "the DOS wildcards (<, \", >) and * against nothing" || fail "DOS: $(v m6) $(v m8) $(v m9) $(v m10)"
[ "$(v small)" = "1 234 21" ] && pass "FindFirstFileNameW: a small buffer, ERROR_MORE_DATA and the length" || fail "small: $(v small)"
case "$(v first)" in "1 \\windows\\notepad.exe"|"1 \\WINDOWS\\notepad.exe") pass "the file's name from its volume root: $(v first)" ;; *) fail "first: $(v first)" ;; esac
[ "$(v next)" = "0 38" ] && [ "$(v close)" = 1 ] && pass "no next name (ERROR_HANDLE_EOF); FindClose takes the handle" || fail "next/close: $(v next) $(v close)"
[ "$(v nofile)" = "1 3" ] && pass "a file that is not there: ERROR_PATH_NOT_FOUND" || fail "nofile: $(v nofile)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
