#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window's menu Move and Size follow the pointer until a click
# (patches/sg/0476). David: taskbar right-click > Move "moves it to the
# mouse and lets go right away"; Size "resizes once". The move/size loop
# ended at the first message that was not a mouse move or key as soon as it
# found the left button up -- which it always is when the move was chosen
# from a menu. Here: Move, the pointer moves (with a pause, when timers and
# paints arrive), a click ends it; the window moved by what the pointer did.
# Size likewise grows it.
#
#   WINE=/opt/wine-sg/bin/wine test/movesize-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-movesize.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/movesize-probe.exe" "$HERE/movesize-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/movesize-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" movesize-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
"$WINE" notepad >/dev/null 2>&1 &
i=0; while [ \$i -lt 60 ] && [ "\$(P rect)" = none ]; do sleep 1; i=\$((i + 1)); done
P place; sleep 2
set -- \$(P rect); echo "before \$*" >> "$T/log.out"
# Move: from the caption's middle, 200 right and 80 down, pausing half-way
cx=\$(( (\$1 + \$3) / 2 )); cy=\$(( \$2 + 12 ))
xdotool mousemove \$cx \$cy; sleep 1
P move; sleep 1
xdotool mousemove \$((cx + 50)) \$((cy + 20)); sleep 0.3; xdotool mousemove \$((cx + 100)) \$((cy + 40)); sleep 2
xdotool mousemove \$((cx + 150)) \$((cy + 60)); sleep 0.3; xdotool mousemove \$((cx + 200)) \$((cy + 80)); sleep 1
xdotool click 1; sleep 1
xdotool mousemove 5 5; sleep 1
set -- \$(P rect); echo "moved \$*" >> "$T/log.out"
# Size: onto the right edge, then 120 further right, pausing half-way
ry=\$(( (\$2 + \$4) / 2 ))
xdotool mousemove \$(( (\$1 + \$3) / 2 )) \$ry; sleep 1
P size; sleep 1
xdotool mousemove \$((\$3 + 5)) \$ry; sleep 0.5; xdotool mousemove \$((\$3 + 60)) \$ry; sleep 2
xdotool mousemove \$((\$3 + 120)) \$ry; sleep 1
xdotool click 1; sleep 1
xdotool mousemove 5 5; sleep 1
set -- \$(P rect); echo "sized \$*" >> "$T/log.out"
"$WINESERVER" -k
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" 2>/dev/null
set -- $(sed -n 's/^before //p' "$T/log.out"); L0=${1:-0}; T0=${2:-0}; R0=${3:-0}; B0=${4:-0}
set -- $(sed -n 's/^moved //p' "$T/log.out"); L1=${1:-0}; T1=${2:-0}; R1=${3:-0}; B1=${4:-0}
set -- $(sed -n 's/^sized //p' "$T/log.out"); L2=${1:-0}; T2=${2:-0}; R2=${3:-0}; B2=${4:-0}
# (the pointer starts near, not exactly at, the point Wine takes as the
# caption's middle: a few pixels either way)
near() { d=$(( $1 - $2 )); [ "$d" -ge -16 ] && [ "$d" -le 16 ]; }
near $((L1 - L0)) 200 && near $((T1 - T0)) 80 && near $((R1 - L1)) $((R0 - L0)) \
    && pass "Move follows the pointer until the click (moved by $((L1 - L0)),$((T1 - T0)))" \
    || fail "Move: moved by $((L1 - L0)),$((T1 - T0)), not 200,80"
near $((R2 - L2)) $(( (R1 - L1) + 120 )) && [ "$L2" = "$L1" ] \
    && pass "Size follows the pointer until the click (wider by $(( (R2 - L2) - (R1 - L1) )))" \
    || fail "Size: wider by $(( (R2 - L2) - (R1 - L1) )), not about 120"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
