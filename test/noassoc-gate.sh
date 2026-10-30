#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A file whose type has no program, opened from the shell (patches/sg/6542). A double-click in
# File Explorer (the shell's folder view) or on the desktop of such a file showed "There is no
# program configured to open this type of file."; Windows asks "How do you want to open this
# file?" (Open with). Now the shell's own open shows Open with, with "Look for an app in the
# Store" (SG Store, --search EXT); "Always use this app" makes the choice the type's (UserChoice and the user's
# class, as Settings' Default apps). A program's own ShellExecuteEx asks too, in a process of
# its own (patches/sg/5965), unless it passes SEE_MASK_FLAG_NO_UI.
# Made-up types .sgzq/.sgzr/.sgzs; real mouse double-clicks (test/noassoc-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/noassoc-gate.sh
# Mutants: SG_MUTANT_NOASSOC_ERROR_BOX (dlls/shell32/shlexec.c: the shell's open as a program's),
# SG_MUTANT_PROGRAM_NOASSOC_ERROR (a program's open keeps the error box).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-noassoc.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/noassoc-probe.c" -lshell32 -lole32 -luuid -lcomctl32 ||
    { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
for n in probe sgowapp store; do cp "$T/probe.exe" "$WINEPREFIX/drive_c/$n.exe"; done
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
}
check first_dialog       "double-clicking a file of a type with no program shows Open with"
check store_row          "Open with offers \"Look for an app in the Store\""
check always_default     "\"Always use this app\" is ticked for a type with no program"
check app_listed         "Open with lists the app that says it takes the type"
check launched           "the chosen app opens the file"
check userchoice         "\"Always use this app\" sets the type's UserChoice"
check userclass          "and the user's class for the type, as Default apps does"
check always_used        "the next double-click opens it with that app, no dialog"
check store_opened       "\"Look for an app in the Store\" starts SG Store"
check store_search       "SG Store searches for the type (--search sgzr)"
check store_no_choice    "the Store chooses no app for the type"
check dialog_closed      "the dialog closes for the Store"
check cancel_quiet       "Cancel shows nothing more"
check program_open_with  "a program's own ShellExecuteEx of it asks too (mutant PROGRAM_NOASSOC_ERROR)"
check program_own_process "and Open with runs in a process of its own (rundll32 OpenAs_RunDLL), not the caller's"
check program_ok         "and ShellExecuteEx succeeds"
check noui_error         "with SEE_MASK_FLAG_NO_UI it fails with SE_ERR_NOASSOC"
check noui_quiet         "and shows nothing"
check done               "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
