#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Selecting and dragging (patches/sg/0630-0632), with the mouse on X, in a shell
# session, read back from the desktop's dump (SG_DESKTOP_DUMP) and File
# Explorer's list view (drag-probe):
#
#   the desktop   a rubber band from the empty desktop selects the icons it
#                 touches (drawn translucent); Ctrl+click adds and removes,
#                 Shift+click selects a range; Ctrl+C and the icon menu take every
#                 selected icon; an icon dragged moves to where it is dropped, two
#                 selected icons move together, and they stay there after F5; an
#                 icon dropped on a folder icon (drawn as the target) moves into it;
#                 an icon dropped on File Explorer moves into its folder, and a
#                 file dragged from File Explorer onto the desktop moves into the
#                 Desktop folder where it was dropped; Delete deletes every
#                 selected icon; Sort by lines them up again
#   File Explorer a rubber band on the empty view selects in Details and in
#                 Large icons (drawn translucent, not a dotted frame); a file
#                 dragged onto a folder moves into it (the folder highlighted); a
#                 file dragged a little way in its own folder is not copied; two
#                 files dragged from one window to another move, and both
#                 windows show it
#
#   WINE=/opt/wine-sg/bin/wine test/drag-gate.sh    (ARTIFACTS=DIR keeps screenshots and dumps)
set -u
unset DISPLAY XAUTHORITY
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run, xdotool, ImageMagick"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: needs python3-pil"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-drag.XXXXXX)
REAL_HOME=$(getent passwd "$(id -un)" | cut -d: -f6); REAL_HOME=${REAL_HOME:-$SG_REAL_HOME}
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.txt "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { echo "FAIL  vdesk-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/drag-probe.exe" "$HERE/drag-probe.c" -lcomctl32 || { echo "FAIL  drag-probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/explorer-probe.exe" "$HERE/explorer-probe.c" -lole32 -lshell32 -lshlwapi -luuid -lgdi32 || { echo "FAIL  explorer-probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/*.exe "$WINEPREFIX/drive_c/"
K='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\HideDesktopIcons\NewStartPanel'
"$WINE" reg add "$K" /v '{20D04FE0-3AEA-1069-A2D8-08002B30309D}' /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
U=$(ls "$WINEPREFIX/drive_c/users" | grep -v -e '^Public$' | head -1)
DESK=$("$WINE" winepath -u "C:\\users\\$U\\Desktop" 2>/dev/null | tr -d '\r')
DOCS=$("$WINE" winepath -u "C:\\users\\$U\\Documents" 2>/dev/null | tr -d '\r')
MUSIC=$("$WINE" winepath -u "C:\\users\\$U\\Music" 2>/dev/null | tr -d '\r')
sg_prefix_safe "$WINEPREFIX" || exit 1
for d in "$WINEPREFIX"/drive_c/users/*/* "$DESK" "$DOCS" "$MUSIC"; do
    r=$(realpath -m "$d")
    case "$r/" in "$REAL_HOME"/*) echo "FAIL  refusing to run: $d is $r, in the real home"; exit 1;; esac
done
case "$(realpath -m "$DESK")" in "$T"/*|"$SG_GATE_HOME"/*) ;; *) echo "FAIL  the Desktop is outside the gate's folder: $DESK"; exit 1;; esac
mkdir -p "$DESK/Box" "$DOCS/Sub" "$MUSIC"
for f in a b c d; do echo "$f" > "$DESK/$f.txt"; done
for f in f1 f2 f3; do echo "$f" > "$DOCS/$f.txt"; done

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.0|"") echo "refusing DISPLAY \$DISPLAY"; exit 1;; esac
T="$T"; WINE="$WINE"; DESK="$DESK"; DOCS="$DOCS"; MUSIC="$MUSIC"; U="$U"; DUMP="$WINEPREFIX/drive_c/desk.txt"
. "$HERE/drag-gate-steps.sh"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"

cat "$T/results.txt" 2>/dev/null
grep -q '^PASS' "$T/results.txt" 2>/dev/null && ! grep -q '^FAIL' "$T/results.txt" || RC=1
[ "$(grep -c '^PASS' "$T/results.txt" 2>/dev/null)" -ge 33 ] || { echo "FAIL  not every check ran"; RC=1; }
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
