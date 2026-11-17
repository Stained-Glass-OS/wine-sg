#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# While a Wine update waits for the machine's wineserver to restart, programs start from the Wine
# that server runs, as Windows keeps the old binaries until the restart (wine-sg 7840). A protocol
# bump (877 -> 880 on David's machine) made every program started between the package's unpack and
# the server's restart die with "version mismatch": the lock screen too (Win+L, an empty screen).
#   the loader: follows a root-owned, not group/world-writable redirect file to a root-owned loader,
#     with the arguments and argv[0] of the kept tree; not a user's file, not a writable one, not
#     with SG_WINE_REDIRECT=0, not to a user's loader; the kept tree's own loader does not loop
#   tools/wine-sg-redirect: start keeps a tree whose wineserver runs (hard links) and names it;
#     a second start while it is pending keeps it; configure ends it when the protocol is the same
#     and keeps it when it differs; clear ends it
#   WINE=.../bin/wine test/runningtree-gate.sh   (needs passwordless sudo for root-owned files)
# Mutant (loader/main.c): SG_MUTANT_NO_LOADER_REDIRECT -> nothing followed.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
HELPER="$HERE/../tools/wine-sg-redirect"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: needs passwordless sudo"; exit 77; }
T=$(mktemp -d /var/tmp/sg-runningtree.XXXXXX)
SRV=
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; sudo -n rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

# --- the loader
sudo -n install -d -o root -m 755 "$T/kept" "$T/kept/bin"
mkdir -p "$T/userkept/bin"
printf '#!/bin/sh\necho "KEPT0=$0"\necho "KEPTARGS=$*"\n' > "$T/fake"
sudo -n install -o root -m 755 "$T/fake" "$T/kept/bin/wine"
install -m 755 "$T/fake" "$T/userkept/bin/wine"
sudo -n sh -c "printf '%s\n' '$T/kept' > '$T/redirect'; chmod 644 '$T/redirect'"
out=$(SG_WINE_REDIRECT_FILE="$T/redirect" "$WINE" one "two words" 2>&1)
printf '%s\n' "$out" | grep -qx "KEPTARGS=one two words" && printf '%s\n' "$out" | grep -qx "KEPT0=$T/kept/bin/wine" \
    && pass "a root's redirect starts the kept tree's loader, with the arguments" || fail "redirect not followed: $out"
cp "$T/redirect" "$T/userredirect"
out=$(SG_WINE_REDIRECT_FILE="$T/userredirect" "$WINE" --version 2>&1)
case "$out" in wine-*) pass "a user's redirect file is not followed" ;; *) fail "user's file: $out" ;; esac
sudo -n cp "$T/redirect" "$T/wredirect"; sudo -n chmod 666 "$T/wredirect"
out=$(SG_WINE_REDIRECT_FILE="$T/wredirect" "$WINE" --version 2>&1)
case "$out" in wine-*) pass "a world-writable one is not either" ;; *) fail "writable file: $out" ;; esac
out=$(SG_WINE_REDIRECT=0 SG_WINE_REDIRECT_FILE="$T/redirect" "$WINE" --version 2>&1)
case "$out" in wine-*) pass "SG_WINE_REDIRECT=0 starts the installed Wine" ;; *) fail "SG_WINE_REDIRECT=0: $out" ;; esac
sudo -n sh -c "printf '%s\n' '$T/userkept' > '$T/uredirect'; chmod 644 '$T/uredirect'"
out=$(SG_WINE_REDIRECT_FILE="$T/uredirect" "$WINE" --version 2>&1)
case "$out" in wine-*) pass "a redirect to a user's loader is not followed" ;; *) fail "user's loader: $out" ;; esac
# the kept tree's own (real) loader, named by the redirect: no loop
sudo -n install -d -o root -m 755 "$T/real/bin"
sudo -n install -o root -m 755 "$WINE" "$T/real/bin/wine"
sudo -n sh -c "printf '%s\n' '$T/real' > '$T/rredirect'; chmod 644 '$T/rredirect'"
out=$(SG_WINE_REDIRECT_FILE="$T/rredirect" timeout 20 "$T/real/bin/wine" --version 2>&1); rc=$?
[ $rc != 124 ] && pass "the kept tree's own loader does not redirect to itself" || fail "loop: $out"

# --- tools/wine-sg-redirect
TREE="$T/tree"; KEPT="$T/tree-running"; RF="$T/run/redirect"
mkdir -p "$TREE/bin" "$TREE/share/wine" "$T/run" "$T/pfx"
cp "$WINESERVER" "$TREE/bin/wineserver"
install -m 755 "$T/fake" "$TREE/bin/wine"
echo 880 > "$TREE/share/wine/sg-server-protocol"
WINEPREFIX="$T/pfx" "$TREE/bin/wineserver" -f -p >/dev/null 2>&1 &
SRV=$!
sleep 2
h() { WINE_SG_TREE="$TREE" WINE_SG_KEPT="$KEPT" WINE_SG_REDIRECT_FILE="$RF" WINE_SG_MACHINE_PREFIX="$T/none" sh "$HELPER" "$@"; }
h start
[ -f "$RF" ] && [ "$(cat "$RF")" = "$KEPT" ] && [ "$(stat -c %i "$TREE/bin/wineserver")" = "$(stat -c %i "$KEPT/bin/wineserver")" ] \
    && pass "start keeps the running server's tree (hard links) and names it" || fail "start: $(ls -la "$T" "$T/run" 2>&1 | tr '\n' ' ')"
# the update lands: new files replace the tree's (by rename, as dpkg does)
cp "$TREE/bin/wineserver" "$TREE/bin/wineserver.new" && mv "$TREE/bin/wineserver.new" "$TREE/bin/wineserver"
echo 880 > "$TREE/share/wine/sg-server-protocol.new" && mv "$TREE/share/wine/sg-server-protocol.new" "$TREE/share/wine/sg-server-protocol"
h configure >/dev/null
[ ! -e "$RF" ] && [ ! -e "$KEPT" ] && pass "configure: the same protocol, programs start from the new Wine at once" || fail "same protocol kept"
# a later update with a new protocol, the server still the one from before the first update
# (its file replaced since: "(deleted)")
h start
echo 883 > "$TREE/share/wine/sg-server-protocol.new" && mv "$TREE/share/wine/sg-server-protocol.new" "$TREE/share/wine/sg-server-protocol"
out=$(h configure)
[ -f "$RF" ] && [ "$(cat "$KEPT/share/wine/sg-server-protocol")" = 880 ] \
    && pass "configure: a new protocol keeps the running tree until the server restarts (a server whose file an earlier update replaced)" || fail "new protocol: $out"
h start
[ -f "$RF" ] && [ "$(cat "$KEPT/share/wine/sg-server-protocol")" = 880 ] \
    && pass "a second update while the restart is pending keeps the running server's tree" || fail "second start replaced the kept tree"
h clear
[ ! -e "$RF" ] && [ ! -e "$KEPT" ] && pass "clear (the server starts) ends it" || fail "clear"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
