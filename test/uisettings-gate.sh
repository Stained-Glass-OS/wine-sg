#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.ViewManagement.UISettings and Windows.UI.Core.CoreWindow's
# statics (patches/sg/0521). Microsoft OneDrive subscribes to
# TextScaleFactorChanged at start and ended on the stub's E_NOTIMPL
# (winrt::hresult_not_implemented); its React Native host then asks
# CoreWindow.GetForCurrentThread() -- null for a desktop thread on Windows --
# and ended on "class not available". The probe checks the IUISettings values
# against the Win32 settings they mirror, TextScaleFactor, AdvancedEffects-
# Enabled and AutoHideScrollBars, and that each Changed event is raised when
# its setting changes (and not once its handler is removed).
#
#   WINE=/opt/wine-sg/bin/wine test/uisettings-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-uisettings.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/uisettings-probe.c" -ladvapi32 -luser32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1=//p" "$T/out"; }
chk() { [ "$(v "$1")" = "$2" ] && pass "$3" || fail "$3: $1='$(v "$1")' (expected '$2')"; }

chk interfaces 6 "UISettings has IUISettings to IUISettings6"
chk caretblink "00000000 1" "CaretBlinkRate is GetCaretBlinkTime()"
chk doubleclick "00000000 1" "DoubleClickTime is GetDoubleClickTime()"
chk scrollbar "00000000 1" "ScrollBarSize is SM_CXVSCROLL x SM_CYHSCROLL"
chk windowcolor "00000000 1" "UIElementColor(Window) is COLOR_WINDOW"
chk messageduration "00000000 1" "MessageDuration"
chk textscale "00000000 100" "TextScaleFactor 1.0 by default (was E_NOTIMPL)"
chk textscaleevent "00000000 1 1 150" "TextScaleFactorChanged raised once, by the object, and the new factor read (was E_NOTIMPL)"
chk textscaleremoved 1 "not raised once its handler is removed"
chk colorevent "00000000 1" "ColorValuesChanged raised when the app mode changes"
chk effects "00000000 1" "AdvancedEffectsEnabled (transparency) on by default"
chk effectsevent "1 0" "AdvancedEffectsEnabledChanged raised when transparency is turned off"
chk autohide "00000000 1" "AutoHideScrollBars on by default"
chk autohideevent "1 1 0" "AutoHideScrollBarsChanged raised with its event args"
chk settings6 "00000000 00000000" "IUISettings6 events register and unregister"
chk corewindow "00000000 00000000 1" "CoreWindow.GetForCurrentThread(): the statics exist, null for a desktop thread"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
