#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml SplitView (patches/sg/6194): its Pane beside its Content (Inline, CompactInline) or
# over it (Overlay, CompactOverlay), OpenPaneLength wide when open, CompactPaneLength when closed in
# the compact modes; PaneBackground; PaneOpening/PaneOpened/PaneClosing (cancellable)/PaneClosed;
# TemplateSettings -- WinUI 2's NavigationView (Windows Terminal's settings page) is built on it.
# test/xamlsplitview-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlsplitview-gate.sh
# Mutants (dlls/windows.ui.xaml/splitview.c): SG_MUTANT_SPLITVIEW_PANE_OVER, SG_MUTANT_SPLITVIEW_NO_CANCEL.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlsplitview.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlsplitview-probe.exe" "$HERE/xamlsplitview-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlsplitview-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlsplitview-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^load=00000000$' "markup"
chk '^splitview=1$' "a SplitView"
chk '^inline_open=300 pane_red=1 content_blue=1$' "Inline, open: the pane (its background) beside the content"
chk '^overlay=400$' "Overlay: the content takes the whole width"
chk '^compact_inline_closed=370$' "CompactInline, closed: the compact pane beside the content"
chk '^compact_overlay=370$' "CompactOverlay: the content beside the compact pane"
chk '^cancelled open=1$' "a cancelled PaneClosing keeps the pane open"
chk '^events=1,1,2,1 open=0$' "PaneOpening, PaneOpened, PaneClosing, PaneClosed"
chk '^template_settings=-100$' "TemplateSettings.NegativeOpenPaneLength"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
