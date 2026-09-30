#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Switching virtual desktops slides (patches/sg/0521): the desktop being left
# moves off to the side over the other (David 2026-09-29: "apps panning when
# you switch desktop"), as on Windows 10. Stretched to 4 s (SG_DESKTOP_SLIDE_MS)
# so screenshots see it: during the switch the slide is on the screen and moving,
# afterwards gone; HKCU\Software\Stained Glass\Effects SlideDesktops=0 turns it off.
#
#   WINE=/opt/wine-sg/bin/wine test/desktop-slide-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb import compare "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-slide.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
SG_DESKTOP_SLIDE_MS=4000 "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
cd "$WINEPREFIX/drive_c"
P() { "$WINE" vdesk-probe.exe "$@" 2>/dev/null | tr -d '\r'; }
"$WINE" vdesk-probe.exe window Alpha 60 60 >/dev/null 2>&1 &
sleep 3
P hotkey right >/dev/null
sleep 1;   during=$(P exists SgDesktopSlide); import -window root "$T/a.png"
sleep 1.5; import -window root "$T/b.png"
sleep 4;   after=$(P exists SgDesktopSlide)
d=$(compare -metric AE "$T/a.png" "$T/b.png" /dev/null 2>&1 | cut -d' ' -f1)
echo "      during: $during, after: $after, moved: $d px"
[ "$during" = "exists=1" ] && [ "${d%.*}" -gt 2000 ] 2>/dev/null && pass "switching desktops slides: the desktop left moves off to the side" \
    || fail "no slide while switching (during $during, $d px)"
[ "$after" = "exists=0" ] && pass "and the slide is gone when it has run" || fail "the slide stayed ($after)"
# off in Effects: no slide
"$WINE" reg add 'HKCU\Software\Stained Glass\Effects' /v SlideDesktops /t REG_DWORD /d 0 /f >/dev/null 2>&1
P hotkey left >/dev/null
sleep 1; off=$(P exists SgDesktopSlide)
[ "$off" = "exists=0" ] && pass "SlideDesktops=0 switches without the slide" || fail "slide shown with SlideDesktops=0 ($off)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
