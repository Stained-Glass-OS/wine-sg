#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A desktop made after the server's shared session mapping grew has its shared memory (7821). The server
# grows the mapping by blocks when it fills (grow_session_mapping); an object in a later block was given
# its offset in that block, not in the mapping, so a client looked for it in the first block, found
# another object there ("Session object id doesn't match"), and the desktop's shared memory was missing:
# its processes knew no monitor (GetSystemMetrics(SM_CXSCREEN) 0). On a machine whose shared server had
# grown its mapping (many desktops and threads over a day), the consent prompt -- the machine account,
# a fresh desktop on its own X server -- was a 0x0 window on a black secure screen.
#   A. one account: 1200 desktops kept open (more than the first block holds), then a new process on a
#      new desktop of its own asks for the screen: the screen's width and a monitor, as before them;
#   B. a second account on a shared (system) prefix, as the machine account's prompts are, in a user
#      namespace with two users (unshare --map-users with the subordinate ids): after the first account
#      grew the mapping, its new desktop has the screen and a monitor too.
#   WINE=/opt/wine-sg/bin/wine test/sessgrow-gate.sh   (mutant SG_MUTANT_SESSION_OFFSET_IN_BLOCK, server mapping.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-sessgrow.XXXXXX)
chmod 755 "$T"
XV=
cleanup() { [ -n "$XV" ] && kill "$XV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T" 2>/dev/null; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/sessgrow-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
chmod 755 "$T/probe.exe"

# --- A: one account
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
Xvfb :266 -nolisten tcp -screen 0 1280x800x24 >/dev/null 2>&1 &
XV=$!
sleep 1
mkdir -p "$WINEPREFIX"
DISPLAY=:266 timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && DISPLAY=:266 timeout -s KILL 240 "$WINE" probe.exe 1200 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'Before=1280 1' && pass "before: a new desktop's process has the screen (1280) and a monitor" \
    || fail "before: $(printf '%s\n' "$out" | grep '^Before=' || echo none)"
printf '%s\n' "$out" | grep -qx 'Desktops=1200' && pass "1200 desktops kept open (the mapping has to grow)" || fail "desktops: $(printf '%s\n' "$out" | grep '^Desktops=')"
printf '%s\n' "$out" | grep -qx 'After=1280 1' && pass "after the mapping grew: a new desktop's process still has the screen and a monitor" \
    || fail "after the mapping grew: $(printf '%s\n' "$out" | grep '^After=' || echo none) (wanted 1280 1)"
"$WINESERVER" -k 2>/dev/null; kill "$XV" 2>/dev/null; XV=

# --- B: a second account on a shared prefix
SUB=$(awk -F: -v u="$(id -un)" '$1 == u { print $2; exit }' /etc/subuid)
SUBG=$(awk -F: -v u="$(id -un)" '$1 == u { print $2; exit }' /etc/subgid)
MAP="--map-users=0:$(id -u):1 --map-users=1000:${SUB:-0}:100 --map-groups=0:$(id -g):1 --map-groups=1000:${SUBG:-0}:100"
if [ -z "$SUB" ] || [ -z "$SUBG" ] || ! command -v setpriv >/dev/null || ! unshare $MAP -m true 2>/dev/null; then
    echo "SKIP  B: no two-user namespaces (subordinate ids, newuidmap, setpriv)"
else
    export T WINE WINESERVER
    cat > "$T/inner.sh" <<'EOS'
set -u
export WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WAYLAND_DISPLAY=sg-no-wayland
Xvfb :267 -ac -nolisten tcp -screen 0 1280x800x24 >/dev/null 2>&1 &
XV=$!
sleep 1
P="$T/shared"; mkdir -p "$P" "$T/h0" "$T/h1" "$T/x0" "$T/x1"
chown 0:1050 "$P"; chmod 2770 "$P"; touch "$P/.sg-system-prefix"
chown 1001:1001 "$T/h1" "$T/x1"
export WINEPREFIX="$P" DISPLAY=:267
HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" "$WINESERVER" -p 300 &
sleep 1
cp "$T/probe.exe" "$P/drive_c/"; chmod 755 "$P/drive_c/probe.exe"
# the first account grows the mapping and holds it
(cd "$P/drive_c" && HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" timeout -s KILL 240 "$WINE" probe.exe 1200 60 > "$T/b-grow.out" 2>/dev/null) &
G=$!
for i in $(seq 120); do grep -q '^After=' "$T/b-grow.out" 2>/dev/null && break; sleep 1; done
tr -d '\r' < "$T/b-grow.out" | sed 's/^/      first account: /'
out=$(cd "$P/drive_c" && setpriv --reuid=1001 --regid=1001 --groups=1050 env HOME="$T/h1" XDG_RUNTIME_DIR="$T/x1" \
      timeout -s KILL 120 "$WINE" probe.exe child Second 2>/dev/null | tr -d '\r')
echo "      second account: $out"
kill "$G" 2>/dev/null
"$WINESERVER" -k 2>/dev/null
kill "$XV" 2>/dev/null
case "$out" in "Second=1280 1") echo "PASS  B: the second account's new desktop, after the mapping grew, has the screen and a monitor" ;;
               *) echo "FAIL  B: the second account's new desktop: '$out' (wanted Second=1280 1)"; exit 1 ;; esac
exit 0
EOS
    unshare $MAP -m sh "$T/inner.sh" || RC=1
    unshare $MAP rm -rf "$T/shared" "$T/h1" "$T/x1" 2>/dev/null
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
