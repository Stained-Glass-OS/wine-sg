#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# New > the file types programs register (patches/sg/0613). SG Office says
# HKCR\.docx\SGOffice.Document.12\ShellNew (NullFile), as programs do on
# Windows, but the New menus had only Folder and Text Document. Here two
# types are registered -- one through its ProgID with NullFile, one on the
# extension itself with Data -- and the folder background menu's New must
# offer them by name and make "New <name>.ext" as they say.
#
#   WINE=/opt/wine-sg/bin/wine test/shellnew-gate.sh
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
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-shellnew.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/shellnew-probe.exe" "$HERE/shellnew-probe.c" -lole32 -lshell32 -luuid -luser32 ||
    { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/shellnew-probe.exe" "$WINEPREFIX/drive_c/"
mkdir -p "$WINEPREFIX/drive_c/work"
r() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
r 'HKCU\Software\Classes\.sgdoc' /ve /d SGGate.Document
r 'HKCU\Software\Classes\SGGate.Document' /ve /d 'Gate Document'
r 'HKCU\Software\Classes\.sgdoc\SGGate.Document\ShellNew' /v NullFile /d ''
r 'HKCU\Software\Classes\.sgnote' /ve /d SGGate.Note
r 'HKCU\Software\Classes\SGGate.Note' /ve /d 'Gate Note'
r 'HKCU\Software\Classes\.sgnote\ShellNew' /v Data /d 'hello gate'
# SG Office's case: the type opens in another program (WordPad), its ShellNew is under SG Office's ProgID
r 'HKCU\Software\Classes\.sgsheet' /ve /d SGGate.Viewer
r 'HKCU\Software\Classes\SGGate.Viewer' /ve /d 'Gate Viewer'
r 'HKCU\Software\Classes\SGGate.Sheet' /ve /d 'Gate Sheet'
r 'HKCU\Software\Classes\.sgsheet\SGGate.Sheet\ShellNew' /v NullFile /d ''
"$WINESERVER" -w
P() { "$WINE" 'C:\shellnew-probe.exe' 'C:\work' "$@" 2>/dev/null | tr -d '\r'; }
P > "$T/list.out"
echo "      New: $(sed -n 's/^item //p' "$T/list.out" | tr '\n' '|')"
[ "$(grep -cx 'item Gate Document' "$T/list.out")" = 1 ] && [ "$(grep -cx 'item Gate Note' "$T/list.out")" = 1 ] &&
    [ "$(grep -cx 'item Gate Sheet' "$T/list.out")" = 1 ] &&
    pass "New offers the registered types by name, once each (one under another ProgID than the default)" || fail "New: $(tr '\n' '|' < "$T/list.out")"
P 'Gate Document' >/dev/null; P 'Gate Note' >/dev/null; P 'Gate Note' >/dev/null; P 'Gate Sheet' >/dev/null
W="$WINEPREFIX/drive_c/work"
[ -f "$W/New Gate Document.sgdoc" ] && [ ! -s "$W/New Gate Document.sgdoc" ] &&
    pass "NullFile: an empty \"New Gate Document.sgdoc\"" || fail "NullFile: $(ls "$W" | tr '\n' '|')"
[ "$(cat "$W/New Gate Note.sgnote" 2>/dev/null)" = "hello gate" ] && [ -f "$W/New Gate Note (2).sgnote" ] &&
    pass "Data: the file holds it; a second is numbered (2)" || fail "Data: $(ls "$W" | tr '\n' '|')"
[ -f "$W/New Gate Sheet.sgsheet" ] && pass "under another ProgID: \"New Gate Sheet.sgsheet\"" || fail "other ProgID: $(ls "$W" | tr '\n' '|')"
exit $RC
