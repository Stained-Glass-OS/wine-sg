#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# rundll32 url.dll,OpenURL URL opens URL in the default browser (patches/sg/0791).
# Programs open pages that way -- Claude Code's sign-in among them (David
# 2026-10-04: "windows apps/command line can't seem to open firefox for
# linux"; Claude Code then printed its sign-in address instead). Wine's
# OpenURL was a stub that did nothing. Here an https handler of the gate's
# records what it is given: the whole URL, &s and all, unquoted as a program
# passes it and quoted as a batch file does; FileProtocolHandler still works.
#
#   WINE=/opt/wine-sg/bin/wine test/openurl-gate.sh   (mutant SG_MUTANT_OPENURL_STUB)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-openurl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/handler.exe" "$HERE/openurl-handler.c" || { fail "handler did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$T/handler.exe" "$C/"
timeout 60 "$WINE" reg add 'HKCR\https\shell\open\command' /ve /d '"C:\handler.exe" "%1"' /f >/dev/null 2>&1
timeout 60 "$WINE" reg add 'HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\https\UserChoice' /v ProgId /d https /f >/dev/null 2>&1
U='https://claude.com/cai/oauth/authorize?code=true&client_id=gate&response_type=code&scope=a%3Ab+c&state=x3_c'
opened() {   # opened N: wait for the handler's Nth line, print it
    i=0; while [ $i -lt 40 ] && [ "$(tr -d '\r' 2>/dev/null < "$C/opened.txt" | wc -l)" -lt "$1" ]; do sleep 0.25; i=$((i + 1)); done
    tr -d '\r' 2>/dev/null < "$C/opened.txt" | sed -n "${1}p"
}
# as a program spawns it: rundll32 url,OpenURL URL (no quotes: no blanks in it)
timeout 60 "$WINE" rundll32 url,OpenURL "$U" >/dev/null 2>&1
got=$(opened 1)
[ "$got" = "$U" ] && pass "rundll32 url,OpenURL URL opens the whole URL in the browser" || fail "OpenURL: handler got '$got'"
# from a batch file, quoted
printf '@rundll32.exe url.dll,OpenURL "%s"\r\n' "$(printf '%s' "$U&n=2" | sed 's/%/%%/g')" > "$C/open.bat"   # %% in a batch file: %
timeout 60 "$WINE" cmd /c 'C:\open.bat' >/dev/null 2>&1
got=$(opened 2)
[ "$got" = "$U&n=2" ] && pass "quoted, from a batch file: the URL without its quotes" || fail "quoted OpenURL: handler got '$got'"
# FileProtocolHandler, as before
timeout 60 "$WINE" rundll32 url.dll,FileProtocolHandler "$U&n=3" >/dev/null 2>&1
got=$(opened 3)
[ "$got" = "$U&n=3" ] && pass "FileProtocolHandler still opens the URL" || fail "FileProtocolHandler: handler got '$got'"
# the other ways a program or a user opens a page: start, explorer
printf '@start "" "%s"\r\n' "$(printf '%s' "$U&n=4" | sed 's/%/%%/g')" > "$C/start.bat"
timeout 60 "$WINE" cmd /c 'C:\start.bat' >/dev/null 2>&1
got=$(opened 4)
[ "$got" = "$U&n=4" ] && pass "start \"\" \"URL\" in a batch file opens the whole URL" || fail "start: handler got '$got'"
timeout 60 "$WINE" explorer "$U&n=5" >/dev/null 2>&1
got=$(opened 5)
[ "$got" = "$U&n=5" ] && pass "explorer URL opens the whole URL" || fail "explorer: handler got '$got'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
