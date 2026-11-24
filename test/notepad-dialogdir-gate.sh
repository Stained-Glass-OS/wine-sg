#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Where Notepad's Open and Save As start (patches/sg/8902), as Windows' Notepad:
#  - the first Save As of a new document, Notepad started in a folder holding
#    text files (the profile folder, as from the Start menu): Documents, not
#    that folder (the common dialog's own rule took the current directory
#    whenever a file matched the filter, and "*.*" matches anything);
#  - then the folder its dialogs last used, for Save As and for Open;
#  - the first Open, with nothing remembered: Documents.
# Mutant SG_MUTANT_NOTEPAD_DIALOG_DIR (no starting folder given) must fail.
#
#   WINE=/opt/wine-sg/bin/wine test/notepad-dialogdir-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree: obj/server/wineserver)
#   XVFB_NUM=N  the X display number to use (default: the first free one)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-npdir.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/notepad-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
U="$C/users/$(id -un)"
cp "$T/probe64.exe" "$C/"
mkdir -p "$C/t" "$U/Documents"
DOCS=$(CDPATH='' cd -- "$U/Documents" && pwd -P)
# the profile folder holds a text file, as a real one does
printf 'notes\r\n' > "$U/notes.txt"
printf 'open me\r\n' > "$C/t/open-me.txt"
printf 'docs only\r\n' > "$DOCS/docs-only.txt"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
P() { printf '%s\n' "\$*" >> "$T/log.out"; "$WINE" "$C/probe64.exe" "\$@" > "$T/p.out" 2>/dev/null; tr -d '\r' < "$T/p.out" >> "$T/log.out"; echo >> "$T/log.out"; }
focus() { w=\$(xdotool search --name 'Notepad\$' | head -1); [ -n "\$w" ] && xdotool windowfocus "\$w"; sleep 0.4; }
dlg() { i=0; while [ \$i -lt 20 ]; do d=\$(xdotool search --name "\$1" | head -1); [ -n "\$d" ] && { xdotool windowfocus "\$d"; sleep 0.8; return 0; }; sleep 0.3; i=\$((i + 1)); done; echo "no \$1 dialog" >> "$T/log.out"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
# started in the profile folder, as from the Start menu
cd "$U"
"$WINE" notepad.exe > /dev/null 2>&1 &
P wait; sleep 1
# 1. the first Save As of a new document
focus; xdotool type --delay 20 'one'; xdotool key ctrl+s; dlg '^Save As$'; xdotool type --delay 20 'first'; xdotool key Return; sleep 1.5
# 2. saved elsewhere by a full path ...
focus; xdotool key ctrl+n; sleep 0.5; xdotool type --delay 20 'two'; xdotool key ctrl+s; dlg '^Save As$'; xdotool type --delay 15 'C:\\t\\second'; xdotool key Return; sleep 1.5
# 3. ... the next Save As starts there
focus; xdotool key ctrl+n; sleep 0.5; xdotool type --delay 20 'three'; xdotool key ctrl+s; dlg '^Save As$'; xdotool type --delay 20 'third'; xdotool key Return; sleep 1.5
# 4. and so does Open
focus; xdotool key ctrl+o; dlg '^Open$'; xdotool type --delay 20 'open-me.txt'; xdotool key Return; sleep 1.5
P tabs
for k in 0 1 2 3 4 5; do P dump \$k "C:\\\\t\\\\dump-open\$k"; done
P close; sleep 2
# 5. nothing remembered: the first Open starts in Documents
"$WINE" reg delete 'HKCU\\Software\\Microsoft\\Notepad' /v sgDialogFolder /f > /dev/null 2>&1
"$WINE" notepad.exe > /dev/null 2>&1 &
P wait; sleep 1
focus; xdotool key ctrl+o; dlg '^Open$'; xdotool type --delay 20 'docs-only.txt'; xdotool key Return; sleep 1.5
P tabs
for k in 0 1 2 3 4 5; do P dump \$k "C:\\\\t\\\\dump-docs\$k"; done
P close; sleep 2
EOF
chmod +x "$T/session.sh"
if [ -n "${XVFB_NUM:-}" ]; then XV="-n $XVFB_NUM"; else XV="-a"; fi
# shellcheck disable=SC2086
timeout -s KILL 300 xvfb-run $XV -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" 2>/dev/null

[ -f "$DOCS/first.txt" ] && [ ! -f "$U/first.txt" ] \
    && pass "the first Save As starts in Documents, not the profile folder Notepad was started in" \
    || fail "first Save As: Documents $(ls "$DOCS" | tr '\n' ' ') / profile $(ls "$U" | tr '\n' ' ')"
[ -f "$C/t/second.txt" ] && pass "a full path in Save As is taken" || fail "C:\\t\\second.txt not written"
[ -f "$C/t/third.txt" ] && [ ! -f "$DOCS/third.txt" ] \
    && pass "the next Save As starts in the folder last used" || fail "third Save As: C:\\t $(ls "$C/t" | tr '\n' ' ')"
cat "$C"/t/dump-open* 2>/dev/null | grep -q 'open me' \
    && pass "Open starts in the folder last used" || fail "Open from the last folder: not C:\\t"
cat "$C"/t/dump-docs* 2>/dev/null | grep -q 'docs only' \
    && pass "with nothing remembered, Open starts in Documents" || fail "first Open: not Documents"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
