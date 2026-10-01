#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What winex11 tells the desktop's compositor (patches/sg/0744; sg-compositor's
# sg-deskcomp draws the Windows programs' windows with drop shadows and real
# alpha -- David: "The apps need to have drop shadows like in windows. We also
# should support alpha in windows.").
#   - the desktop's picture: _SG_DESKTOP_PIXMAP on the virtual desktop's X
#     window names a pixmap that stays alive, showing the desktop (here a flat
#     green);
#   - _SG_SHADOW on each window: 1 for a window with a title bar, 2 for a
#     popup menu (its class has CS_DROPSHADOW), 0 for the taskbar.
# The third part, no backdrop for alpha popups while the compositor runs, is
# checked by sg-compositor's deskcomp gate (pixels change with what is below).
#
#   WINE=/opt/wine-sg/bin/wine test/deskprops-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
unset DISPLAY WAYLAND_DISPLAY
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run xprop xwininfo cc; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-deskprops.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/rounded-probe.exe" "$HERE/rounded-probe.c" -ldwmapi -lgdi32 -lcomctl32 || { fail "probe did not build"; exit 1; }
cc -O2 -o "$T/pixmap-pixel" "$HERE/pixmap-pixel.c" -lX11 2>/dev/null || { echo "SKIP: cannot build pixmap-pixel (libx11-dev)"; exit 77; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/rounded-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '0 255 0' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
"$WINE" rounded-probe.exe win Framed 100 100 300 200 &
sleep 3
"$WINE" rounded-probe.exe menu 500 100 &
sleep 3
D=\$(xwininfo -root -tree | awk '/"shell - Wine Desktop"/ { print \$1; exit }')
xprop -id \$D _SG_DESKTOP_PIXMAP > "$T/pixmap.out" 2>&1
P=\$(sed -n 's/.*# //p' "$T/pixmap.out")
[ -n "\$P" ] && "$T/pixmap-pixel" \$P 400 300 > "$T/pixel.out" 2>&1
for c in \$(xwininfo -id \$D -children | awk '/^ +0x/ { print \$1 }'); do
    printf '%s %s %s\\n' "\$c" "\$(xwininfo -id \$c | awk '/Map State/ { print \$3 }')" "\$(xprop -id \$c _SG_SHADOW WM_NAME 2>&1 | tr '\\n' ' ')"
done > "$T/children.out"
xwininfo -id \$D -children > "$T/tree.out"
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > "$T/session.out" 2>&1

grep -q 'pixmap id #' "$T/pixmap.out" && pass "the desktop names its picture: $(tr -d '\n' < "$T/pixmap.out")" || fail "no _SG_DESKTOP_PIXMAP: $(cat "$T/pixmap.out")"
[ "$(cat "$T/pixel.out" 2>/dev/null)" = "0,255,0" ] && pass "and the pixmap is alive and shows the desktop (green)" \
    || fail "the desktop's pixmap: $(cat "$T/pixel.out" 2>/dev/null) (freed?)"
viewable() { grep ' IsViewable ' "$T/children.out"; }
viewable | grep '"Framed"' | grep -q '_SG_SHADOW(CARDINAL) = 1' && pass "a window with a title bar: _SG_SHADOW 1" \
    || fail "framed window: $(viewable | grep '"Framed"')"
viewable | grep -q '_SG_SHADOW(CARDINAL) = 2' && pass "a popup menu: _SG_SHADOW 2 (CS_DROPSHADOW)" \
    || fail "menu: $(viewable | grep -v 'Framed' | head -5)"
tb=$(awk '/800x[0-9]+\+0\+[0-9]+/ && $0 !~ /800x600/ { print $1; exit }' "$T/tree.out")
grep "^$tb " "$T/children.out" | grep -q '_SG_SHADOW(CARDINAL) = 0' && pass "the taskbar: _SG_SHADOW 0" \
    || fail "taskbar ($tb): $(grep "^$tb " "$T/children.out")"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
