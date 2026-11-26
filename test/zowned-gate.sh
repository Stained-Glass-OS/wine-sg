#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An owned window stays above its owner, whatever X exposes (patches/sg/9970).
#
# Brave and Chrome with the GPU process: after an unclean exit the "quit
# unexpectedly" dialog (an owned popup; its owner, the browser window, is
# disabled behind it) was invisible and took no clicks. The browser window's X
# window got an Expose over its whole area -- the desktop's compositor
# (sg-deskcomp) redirects the Wine desktop's X windows, so an exposure is no
# longer cut by the windows over it -- and winex11 tells the server "make this
# rectangle visible" (update_window_zorder), which moved the browser window
# above its own dialog. Windows never puts an owner above the windows it owns.
#
# The probe shows an owner and the popup it owns; this gate redirects the
# desktop's X windows as a compositor does, exposes the owner's X window and
# asks the probe for the Z order again.
#
#   WINE=/opt/wine-sg/bin/wine test/zowned-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CC="${CC:-cc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xdotool >/dev/null || { echo "SKIP: needs xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/include/X11/extensions/Xcomposite.h ] || { echo "SKIP: needs libxcomposite-dev"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-zowned.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
HELPER_PID=
cleanup() { [ -n "$HELPER_PID" ] && kill "$HELPER_PID" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/zowned-probe.exe" "$HERE/zowned-probe.c" -luser32 -lgdi32 || { fail "probe did not build"; exit 1; }
"$CC" -O2 -o "$T/zowned-xhelper" "$HERE/zowned-xhelper.c" -lX11 -lXcomposite || { fail "X helper did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/zowned-probe.exe" "$C/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x768 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x768 > "$T/explorer.out" 2>&1 &
i=0; while ! xdotool search --name 'shell - Wine Desktop' >/dev/null 2>&1 && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
DESK=$(xdotool search --name 'shell - Wine Desktop' 2>/dev/null | head -1)
[ -n "$DESK" ] || { fail "the virtual desktop did not open"; exit 1; }

# the desktop's windows redirected, as the session's compositor has them
"$T/zowned-xhelper" redirect "$DESK" > "$T/helper.out" 2>&1 &
HELPER_PID=$!
i=0; while ! grep -q redirected "$T/helper.out" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
grep -q redirected "$T/helper.out" || { fail "could not redirect the desktop's X windows (Composite)"; exit 1; }

LOG="$C/zowned.log"
(cd "$C" && "$WINE" zowned-probe.exe 'C:\zowned.log' >/dev/null 2>&1 &)
i=0; while ! grep -q '^BEFORE' "$LOG" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
grep -q '^BEFORE' "$LOG" || { fail "the probe's windows did not open"; exit 1; }
XWIN=$(grep '^XWIN' "$LOG" | tr -d '\r' | cut -d' ' -f2)
SHOWN=$(grep '^SHOWN' "$LOG" | tr -d '\r' | cut -d' ' -f2)
BEFORE=$(grep '^BEFORE' "$LOG" | tr -d '\r' | cut -d' ' -f2)
# the popup shown first, then its owner: mapping the owner's X window exposes it
[ "$SHOWN" = owned-above ] && pass "popup then owner shown: the owned popup is above its owner" \
    || fail "popup then owner shown: want the owned popup above its owner, got $SHOWN"
[ "$BEFORE" = owned-above ] || { fail "the probe could not put its popup on top ($BEFORE)"; exit 1; }
[ -n "$XWIN" ] && [ "$XWIN" != 0 ] || { fail "the owner has no X window"; exit 1; }

"$T/zowned-xhelper" expose "$XWIN" || fail "could not expose the owner's X window"
sleep 1
: > "$LOG.go"
i=0; while ! grep -q '^AFTER' "$LOG" 2>/dev/null && [ $i -lt 30 ]; do sleep 0.5; i=$((i + 1)); done
AFTER=$(grep '^AFTER' "$LOG" | tr -d '\r' | cut -d' ' -f2)
[ "$AFTER" = owned-above ] && pass "the owner's X window exposed: the owned popup stays above it" \
    || fail "the owner's X window exposed: want the owned popup above its owner, got '$AFTER'"
exit $RC
