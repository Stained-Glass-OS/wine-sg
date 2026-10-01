#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Rounded style's controls (patches/sg/0740, theme/rounded.py). David:
# in the Rounded style edit boxes stayed square, the rest being round. Their
# frames are a bordered fill (BgType = BorderFill), which uxtheme drew as a
# square only; the Rounded schemes now ask for BorderType = RoundRect and
# uxtheme draws the corners anti-aliased over the parent's background (and
# says the part is partly transparent, so the controls draw that first).
# Edit boxes, list boxes, list views and combo boxes get round frames; a focused edit
# box's frame is the accent blue. And radio buttons: rounded.py doubled every
# rx/ry, an <ellipse>'s too, so the Rounded radio button was a broken arc
# -- only rectangles' radii are doubled now.
#
# The probe paints the controls on a green window; the checks read the
# screen. Rounded light, Rounded dark (Rounded Dark scheme), and the Classic
# style, which keeps square frames.
#
#   WINE=/opt/wine-sg/bin/wine test/roundedctl-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
WINDRES="${WINDRES:-x86_64-w64-mingw32-windres}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
unset DISPLAY WAYLAND_DISPLAY
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run import convert; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-roundedctl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
printf '1 24 "%s"\n' "$HERE/theme-gallery.manifest" > "$T/p.rc"
"$WINDRES" "$T/p.rc" -O coff -o "$T/p.o" &&
"$MINGW" -O2 -municode -o "$T/roundedctl-probe.exe" "$HERE/roundedctl-probe.c" "$T/p.o" -luxtheme -lcomctl32 -lgdi32 -luser32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/roundedctl-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
PERS='HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
"$WINE" reg add 'HKCU\Software\Stained Glass\Style' /v Rounded /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINE" reg add "$PERS" /v AppsUseLightTheme /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
P() { "$WINE" roundedctl-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
look() {   # look NAME: the colour scheme, then the probe's window
    P colour > "$T/\$1.colour"
    "$WINE" roundedctl-probe.exe show & pid=\$!
    sleep 4; import -window root "$T/\$1.png"
    kill \$pid; sleep 1
}
look rlight
"$WINE" reg add '$PERS' /v AppsUseLightTheme /t REG_DWORD /d 0 /f >/dev/null 2>&1; P notify; sleep 2
look rdark
"$WINE" reg add 'HKCU\\Software\\Stained Glass\\Style' /v Rounded /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINE" reg add '$PERS' /v AppsUseLightTheme /t REG_DWORD /d 1 /f >/dev/null 2>&1; P notify; sleep 2
look classic
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > "$T/session.out" 2>&1

px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
green() { [ "$(px "$@")" = "0,255,0" ]; }
blue() { px "$@" | awk -F, '{ exit !($3 > $1 + 60 && $3 > $2 + 20) }'; }
grey() { px "$@" | awk -F, '{ d1 = $1 - $3; d2 = $2 - $3; exit !(d1 < 16 && d1 > -16 && d2 < 16 && d2 > -16 && $2 != 255) }'; }
for s in rlight rdark classic; do [ -f "$T/$s.png" ] || { fail "no screenshot $s: $(tail -3 "$T/session.out")"; echo "RESULT: FAIL"; exit 1; }; done

for s in rlight rdark; do
    want=Rounded; [ $s = rdark ] && want=RoundedDark
    [ "$(cat "$T/$s.colour")" = "colour=$want" ] && pass "$s: the $want scheme is in use" || fail "$s: scheme $(cat "$T/$s.colour")"
    green $s 20 20 && green $s 219 45 && ! green $s 21 22 && ! green $s 120 20 \
        && pass "$s: an edit box's frame has round corners, the parent's green showing through, the edge drawn" \
        || fail "$s: edit corners $(px $s 20 20) $(px $s 219 45), edge $(px $s 120 20) near $(px $s 21 22)"
    grey $s 120 20 && blue $s 120 60 \
        && pass "$s: a focused edit box's frame is the accent blue, an unfocused one's grey" \
        || fail "$s: frame colours unfocused $(px $s 120 20) focused $(px $s 120 60)"
    green $s 240 20 && green $s 389 99 && ! green $s 315 20 \
        && pass "$s: a list box's frame too" || fail "$s: list box corners $(px $s 240 20) $(px $s 389 99) edge $(px $s 315 20)"
    green $s 240 120 && green $s 389 145 && pass "$s: and a list view's" || fail "$s: list view corners $(px $s 240 120) $(px $s 389 145)"
    green $s 240 170 && ! green $s 300 170 && pass "$s: and a combo box's with an edit box" \
        || fail "$s: combo box corner $(px $s 240 170) edge $(px $s 300 170)"
    green $s 420 60 && ! green $s 480 60 && pass "$s: DrawThemeBackground EP_EDITTEXT leaves the corners to what is below" \
        || fail "$s: drawn EP_EDITTEXT corner $(px $s 420 60) edge $(px $s 480 60)"
    # the radio button's image: a ring round a fill -- left middle is the ring
    [ "$(px $s 420 26)" != "$(px $s 426 26)" ] && ! green $s 426 26 && [ "$(px $s 426 26)" = "$(px $s 425 25)" ] \
        && pass "$s: a radio button is a whole ring ($(px $s 420 26) round $(px $s 426 26))" \
        || fail "$s: radio button ring $(px $s 420 26) centre $(px $s 426 26) $(px $s 425 25)"
done
[ "$(cat "$T/classic.colour")" != "colour=Rounded" ] && pass "classic: the Rounded scheme is not in use ($(cat "$T/classic.colour"))" \
    || fail "classic: still $(cat "$T/classic.colour")"
! green classic 20 20 && ! green classic 240 20 && ! green classic 420 60 && ! green classic 240 170 \
    && pass "classic: square frames, as before" || fail "classic: corners $(px classic 20 20) $(px classic 240 20) $(px classic 420 60) $(px classic 240 170)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
