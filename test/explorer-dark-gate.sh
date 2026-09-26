#!/bin/sh
# File Explorer follows the app mode, live (patches/sg/0264, QA B19/B33).
#
# A File Explorer window on a folder with an item selected, in light mode;
# then, as Settings does, AppsUseLightTheme=0 + WM_SETTINGCHANGE
# "ImmersiveColorSet" (the shell switches the visual style, 0160-0163): the
# command bar, the address bar, the navigation pane, the folder view and the
# status bar all go dark -- not only the title bar -- and the selected item's
# tint is a dark one its white name reads on, not the light tint; then back
# to light. By pixels, and the selection by colour anywhere on the screen.
#
#   WINE=/opt/wine-sg/bin/wine test/explorer-dark-gate.sh   (ARTIFACTS=DIR keeps screenshots)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run, ImageMagick and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-explorer-dark.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

printf '1 24 "%s"\n' "$HERE/theme-gallery.manifest" > "$T/p.rc"
"$WINDRES" "$T/p.rc" -O coff -o "$T/p.o" &&
"$MINGW" -municode -O2 -o "$T/darkmode-probe.exe" "$HERE/darkmode-probe.c" "$T/p.o" \
    -lcomctl32 -luxtheme -ldwmapi -lgdi32 -luser32 -ladvapi32 &&
"$MINGW" -O2 -o "$T/explorer-probe.exe" "$HERE/explorer-probe.c" -lole32 -lshell32 -lshlwapi -luuid -lgdi32 \
    || { fail "probes did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/darkmode-probe.exe" "$T/explorer-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v AppsUseLightTheme /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Explorer' /v WindowWidth /t REG_DWORD /d 1000 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Stained Glass\Explorer' /v WindowHeight /t REG_DWORD /d 600 /f >/dev/null 2>&1
"$WINESERVER" -w
D="$WINEPREFIX/drive_c/sgdark"
mkdir -p "$D/Folder one" "$D/Folder two"
for f in alpha beta gamma delta; do echo "$f" > "$D/$f.txt"; done

px() { convert "$T/$V-$1.png" -format "%[fx:int(255*(0.299*p{$2,$3}.r+0.587*p{$2,$3}.g+0.114*p{$2,$3}.b))]" info: 2>/dev/null; }
# how many pixels of an exact colour the screen has
count() { convert "$T/$V-$1.png" -fill black +opaque "$2" -fill white -opaque "$2" -format '%[fx:int(mean*w*h+0.5)]' info: 2>/dev/null; }
origin() { sed -n 's/origin=//p' "$T/$V-$1.origin" 2>/dev/null; }


# Large icons (FVM_ICON | 96 << 8), where the selection tints an icon's whole
# box (QA B33), then Details (FVM_DETAILS | 16 << 8), where the list's own background
# must follow too (comctl32, 0265)
for mode in 24577:icons 4100:details; do
V=${mode#*:}
echo "-- $V"
"$WINE" reg add 'HKCU\Software\Stained Glass\Explorer\FolderViews' /v 'C:\sgdark' /t REG_DWORD /d "${mode%%:*}" /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize' /v AppsUseLightTheme /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" explorer-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
shot() { sleep 2; import -window root "$T/$V-\$1.png"; P origin > "$T/$V-\$1.origin"; }
"$WINE" explorer /desktop=shell,1280x800 > "$T/$V-desktop.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/$V-desktop.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" explorer '/select,C:\\sgdark\\gamma.txt' > "$T/$V-explorer.out" 2>&1 &
echo "open: \$(P wait-title sgdark 20)" >> "$T/$V-log.out"
sleep 2
shot light
"$WINE" darkmode-probe.exe mode apps dark >> "$T/$V-log.out" 2>&1
shot dark
# a window opened in dark mode: This PC, dark from the start
"$WINE" explorer > "$T/$V-explorer2.out" 2>&1 &
echo "thispc: \$(P wait-title 'This PC' 20)" >> "$T/$V-log.out"
shot thispc-dark
"$WINE" darkmode-probe.exe mode apps light >> "$T/$V-log.out" 2>&1
shot back
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1280x800x24' "$T/session.sh"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
grep -q 'open: title=sgdark' "$T/$V-log.out" && pass "$V: File Explorer opened the folder" || fail "$V: File Explorer did not open: $(cat "$T/$V-log.out")"
O=$(origin light); OX=${O%,*}; OY=${O#*,}
# points in the client area (1000x600 window): the command bar, the address
# bar's edge, the navigation pane, the folder view's empty space, the status bar
CMD="$((OX + 700)) $((OY + 20))"; NAV="$((OX + 60)) $((OY + 500))"
VIEW="$((OX + 700)) $((OY + 480))"; STATUS="$((OX + 700)) $((OY + ${SG_STATUS_Y:-560}))"
for part in CMD NAV VIEW STATUS; do
    eval "xy=\$$part"
    l=$(px light $xy); d=$(px dark $xy); b=$(px back $xy)
    if [ "${l:-0}" -gt 200 ] && [ "${d:-255}" -lt 70 ] && [ "${b:-0}" -gt 200 ]; then
        pass "$V: $part: light ($l), dark ($d), light again ($b)"
    else fail "$V: $part at $xy: light $l, dark $d, back $b"; fi
done
ls=$(count light '#e4d6f5'); ds=$(count dark '#e4d6f5'); dd=$(count dark '#4b3a63')
[ "${ls:-0}" -gt 200 ] && pass "$V: light: the selected item has the light accent tint ($ls px)" || fail "$V: light: no selection tint ($ls px)"
if [ "${ds:-1}" -lt 20 ] && [ "${dd:-0}" -gt 200 ]; then pass "$V: dark: the selection is a dark tint its white name reads on ($dd px), no light tint left ($ds px)"
else fail "$V: dark: light tint $ds px, dark tint $dd px"; fi

O2=$(origin thispc-dark); X2=$((${O2%,*} + 700)); Y2=$((${O2#*,} + 480))
t=$(px thispc-dark $X2 $Y2); tb=$(px back $X2 $Y2)
if [ "${t:-255}" -lt 70 ] && [ "${tb:-0}" -gt 200 ]; then pass "$V: a window opened in dark mode is dark from the start (This PC: $t), and follows back to light ($tb)"
else fail "$V: This PC opened in dark mode: $t, after light: $tb"; fi
done

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
