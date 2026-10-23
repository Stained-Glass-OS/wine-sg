#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The file dialogs' preview pane (patches/sg/6512; GetOpenFileNamePreview/GetSaveFileNamePreview were
# the plain dialogs): an MCI window beside the dialog's controls plays the selected file, with its
# size and length; the program's hook keeps working (test/ofnpreview-probe.c), under Xvfb.
#   WINE=/opt/wine-sg/bin/wine test/ofnpreview-gate.sh
# Mutants: SG_MUTANT_PREVIEW_NO_OPEN (msvfw32 msvideo_main.c), SG_MUTANT_MCIWND_FAILED_OPEN_KEPT
# (msvfw32 mciwnd.c), SG_MUTANT_AVI_FRAME_TIME_INVERTED (avifil32 avifile.c).
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
T=$(mktemp -d /var/tmp/sg-ofnpreview.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/ofnpreview-probe.c" -lmsvfw32 -lavifil32 -lcomdlg32 -lcomctl32 -lole32 -luuid -luser32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check dialog          "GetOpenFileNamePreviewW opens the dialog"
check preview_window  "with an MCI window in it"
check preview_placed  "beside the dialog's own controls"
check previews_avi    "selecting an AVI opens it there, its size and length below"
check previews_avi_length "its length is the file's (2 s; the AVI writer wrote the frame time inverted)"
check no_preview_text "a file MCI cannot play shows nothing"
check previews_again  "selecting the AVI again previews it again"
check chosen          "OK returns the file, with its name's offset"
check app_hook        "the program's hook sees WM_INITDIALOG with its OPENFILENAME and the selection changes"
check flags_kept      "the program's structure keeps its own flags, hook and data"
check done         "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
