#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A cloaked window is still painted (patches/sg/0629). Cloaking (0067) took a
# window off the screen and also stopped painting it: a window cloaked before
# it was shown got no WM_NCPAINT or WM_PAINT. Brave cloaks each new window in
# WM_NCCREATE and uncloaks it in WM_NCPAINT, so its windows never appeared
# (the taskbar showed "Welcome to Brave", the screen nothing). On Windows a
# cloaked window is painted and composed, just not shown. The probe does what
# Brave does; the gate requires the paint messages, the uncloak, and the
# window on the X server's screen, green as it painted it. (cloak-gate.sh
# keeps covering what cloaking hides.)
#
#   WINE=/opt/wine-sg/bin/wine test/cloakpaint-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-cloakpaint.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/cloakpaint-probe.exe" "$HERE/cloakpaint-probe.c" -ldwmapi -lgdi32 ||
    { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX" "$T/sync"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/cloakpaint-probe.exe" "$WINEPREFIX/drive_c/"
# programs join the shell's desktop, as in a session (sg-session's sg-run-explorer)
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w
SYNC=$("$WINE" winepath -w "$T/sync" 2>/dev/null)

cat > "$T/session.sh" <<EOF
#!/bin/sh
"$WINE" explorer /desktop=shell,800x600 >/dev/null 2>&1 &
sleep 5
cd "$WINEPREFIX/drive_c" && "$WINE" cloakpaint-probe.exe '$SYNC' > "$T/probe.out" 2>/dev/null &
i=0
while ! grep -q now= "$T/probe.out" 2>/dev/null && [ \$i -lt 600 ]; do sleep 0.1; i=\$((i + 1)); done
sleep 1
import -window root "$T/shot.png"
touch "$T/sync/done"
sleep 1
EOF
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
out=$(tr -d '\r' < "$T/probe.out" 2>/dev/null)
printf '      %s\n' "$out"
f() { printf '%s\n' "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
px() { convert "$T/shot.png" -format "%[fx:int(255*p{$1,$2}.r)],%[fx:int(255*p{$1,$2}.g)],%[fx:int(255*p{$1,$2}.b)]" info: 2>/dev/null; }

[ "$(f cloaked)" = 00000000 ] && pass "the window is cloaked in WM_NCCREATE" || fail "cloak in WM_NCCREATE: $(f cloaked)"
[ "${ncpaint:=$(f ncpaint)}" -ge 1 ] 2>/dev/null && pass "shown while cloaked, it gets WM_NCPAINT ($ncpaint)" ||
    fail "no WM_NCPAINT while cloaked (${ncpaint:-none})"
[ "$(f uncloak)" = 00000000 ] && [ "$(f now)" = 0 ] && pass "and uncloaks itself there" ||
    fail "uncloak $(f uncloak), DWMWA_CLOAKED now $(f now)"
[ "$(f paint)" -ge 1 ] 2>/dev/null && pass "it gets WM_PAINT ($(f paint))" || fail "no WM_PAINT ($(f paint))"
[ "$(px 400 300)" = "0,255,0" ] && pass "it is on the screen, painted" || fail "screen at the window: $(px 400 300)"
exit $RC
