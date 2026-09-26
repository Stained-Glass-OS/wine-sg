#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A task dialog from a program without a common controls manifest
# (patches/sg/0401): comctl32 uses its own version 6 context, so command
# links are comctl32 6's command links, with room for their text -- KeePass's
# first-run question had its two choices 4 pixels high, and so unclickable.
#
#   WINE=/opt/wine-sg/bin/wine test/taskdialog-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" xvfb-run; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-taskdialog.XXXXXX)
# a scratch home: a prefix links its user folders to $HOME's, and a test must
# never reach the real one
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" XDG_DESKTOP_DIR="$T/home/Desktop"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/taskdialog-probe.exe" "$HERE/taskdialog-probe.c" -lcomctl32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
cat > "$T/run.sh" <<RUN
#!/bin/sh
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/taskdialog-probe.exe" > "$T/out" 2>/dev/null
RUN
chmod +x "$T/run.sh"
xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
out=$(tr -d '\r' < "$T/out")
echo "$out"
case "$out" in *"hr=00000000 links=2 "*) pass "both choices have the command-link style" ;; *) fail "command links: $out" ;; esac
h=$(printf '%s' "$out" | sed -n 's/.*min_height=\([0-9]*\).*/\1/p')
[ "${h:-0}" -ge 30 ] && pass "each is tall enough to read and click (${h}px)" || fail "command link height ${h:-?}px"
case "$out" in *"v6=2"*) pass "they are comctl32 6's buttons (they answer BCM_GETIDEALSIZE)" ;; *) fail "version 6: $out" ;; esac
exit $RC
