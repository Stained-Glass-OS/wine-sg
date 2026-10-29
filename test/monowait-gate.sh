#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A prefix update does not wait for ever on the Wine Mono download dialog (patches/sg/6186): when no
# Wine Mono is found, mscoree's registration (wineboot -u) runs appwiz.cpl install_mono, whose
# download dialog waits for an answer; mscoree waited for it without a bound. Now it waits 120 s and
# goes on, the dialog left to whoever can answer it. Wine Mono is hidden here by bind-mounting an
# empty directory over its directories in a user and mount namespace (unshare).
#   WINE=/opt/wine-sg/bin/wine test/monowait-gate.sh
# Mutant (dlls/mscoree/mscoree_main.c): SG_MUTANT_APPWIZ_WAIT_FOREVER.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
command -v unshare >/dev/null || { echo "SKIP: unshare missing"; exit 77; }
unshare -rm true 2>/dev/null || { echo "SKIP: no user namespaces"; exit 77; }
T=$(mktemp -d /var/tmp/sg-monowait.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$T/prefix" "$T/empty"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
unset WAYLAND_DISPLAY
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
i=0; while ! DISPLAY=":$DPY" xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
# every directory Wine Mono may be found in, hidden
HIDE=""
for d in "$(dirname "$WINE")/../share/wine/mono" "$(dirname "$WINE")/../../mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono /opt/wine/mono; do
    [ -d "$d" ] && HIDE="$HIDE $(realpath "$d")"
done
cat > "$T/run.sh" <<EOF
for d in $HIDE; do mount --bind "$T/empty" "\$d" || exit 9; done
exec unshare -U --map-user=$(id -u) --map-group=$(id -g) sh "$T/inner.sh"
EOF
cat > "$T/inner.sh" <<EOF
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
s=\$(date +%s)
DISPLAY=":$DPY" timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1
echo "rc=\$? \$(( \$(date +%s) - s ))"
"$WINESERVER" -k 2>/dev/null
EOF
out=$(unshare -rm sh "$T/run.sh" 2>&1 | tail -1)
echo "      update $out"
RC=0
rc=${out#rc=}; rc=${rc%% *}; secs=${out##* }
if [ "$rc" = 0 ]; then echo "PASS  wineboot -u finishes with no Wine Mono and its download dialog up"
else echo "FAIL  wineboot -u did not finish (rc $rc)"; RC=1; fi
if [ "$secs" -ge 100 ] 2>/dev/null; then echo "PASS  it waited for the dialog a while (${secs}s)"
else echo "FAIL  no wait for the dialog seen (${secs}s): the install_mono path was not taken"; RC=1; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
