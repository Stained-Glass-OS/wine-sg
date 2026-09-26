#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell's desktop (patches/sg/0261, 0262; see CLAUDE.md), typed and clicked on X
# and read back from its dump (SG_DESKTOP_DUMP: the view settings, the selection,
# each icon's place and title):
#
#   B15  a file in the Desktop folder before sign-in is shown; files and folders
#        made in the user's and the public Desktop folders afterwards appear by
#        themselves; F5 reads the desktop again (a SortBy changed behind its back)
#   B16  a click selects an icon (drawn highlighted), a click on the empty desktop
#        clears it; right-clicking the empty desktop opens its menu: New > Folder
#        and New > Text Document make them (selected), View > Large icons,
#        View > Show desktop icons, Sort by > Date modified, Display settings and
#        Personalize (ms-settings:, a stand-in handler here); right-clicking an
#        icon opens its shell menu, whose Open opens it
#   B18  Alt+F4 on the desktop asks ("Shut Down") instead of signing out; Escape
#        keeps the session; Alt+F4 closes Notepad and then asks rather than
#        hiding the taskbar; Alt+F4 closes File Explorer (This PC); OK on Restart
#        restarts (a stand-in for the power helper)
#
#   WINE=/opt/wine-sg/bin/wine test/desktop-gate.sh    (ARTIFACTS=DIR keeps the screenshots)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run, xdotool, ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-desktop.XXXXXX)
# The gate makes and deletes files in the Desktop folder: Wine runs on the gate's
# own HOME (scratch-home.sh), and the gate refuses a Desktop that reaches the real one
REAL_HOME=$(getent passwd "$(id -un)" | cut -d: -f6); REAL_HOME=${REAL_HOME:-$SG_REAL_HOME}
# the Shut Down dialog's OK powers off or restarts through ExitWindowsEx's native helper
# (0241): here a stand-in that only writes down what it was asked
printf '#!/bin/sh\necho "$*" >> "%s/power.txt"\n' "$T" > "$T/powerctl"; chmod +x "$T/powerctl"
export SG_POWERCTL="$T/powerctl"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.txt "$ARTIFACTS"/ 2>/dev/null
    [ -f "$T/power.txt" ] && cp "$T/power.txt" "$T/power-asked.txt"
    rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
K='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\HideDesktopIcons\NewStartPanel'
"$WINE" reg add "$K" /v '{20D04FE0-3AEA-1069-A2D8-08002B30309D}' /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
# a stand-in for Settings: ms-settings: URIs are written to C:\settings.txt
"$WINE" reg add 'HKCU\Software\Classes\ms-settings' /v 'URL Protocol' /d '' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Classes\ms-settings\shell\open\command' /ve \
    /d 'C:\windows\system32\cmd.exe /c echo %1>>C:\settings.txt' /f >/dev/null 2>&1
"$WINESERVER" -w
DESK=$("$WINE" winepath -u 'C:\users\'"$(id -un)"'\Desktop' 2>/dev/null | tr -d '\r')
PUB=$("$WINE" winepath -u 'C:\users\Public\Desktop' 2>/dev/null | tr -d '\r')
sg_prefix_safe "$WINEPREFIX" || exit 1
for d in "$WINEPREFIX"/drive_c/users/*/* "$DESK" "$PUB"; do
    r=$(realpath -m "$d")
    case "$r/" in "$REAL_HOME"/*) echo "FAIL  refusing to run: $d is $r, in the real home"; exit 1;; esac
done
case "$(realpath -m "$DESK")" in "$T"/*|"$SG_GATE_HOME"/*) ;; *) echo "FAIL  the Desktop is outside the gate's folder: $DESK"; exit 1;; esac
mkdir -p "$DESK" "$PUB"
echo "before sign-in" > "$DESK/before.txt"

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.0) echo "refusing DISPLAY :0"; exit 1;; esac
T="$T"; WINE="$WINE"; DESK="$DESK"; PUB="$PUB"; DUMP="$WINEPREFIX/drive_c/desk.txt"
. "$HERE/desktop-gate-steps.sh"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 500 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"

cat "$T/results.txt" 2>/dev/null
grep -q '^PASS' "$T/results.txt" 2>/dev/null && ! grep -q '^FAIL' "$T/results.txt" || RC=1
[ "$(grep -c '^PASS' "$T/results.txt" 2>/dev/null)" -ge 33 ] || { echo "FAIL  not every check ran"; RC=1; }
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
