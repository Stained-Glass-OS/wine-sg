#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# (run with a fake session: the person's runtime dir has a wayland-0 that
# counts who connects; the helper must take the gate away from it)
if [ -z "${SG_SH_FAKE_SESSION:-}" ]; then
    _f=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-shfake.XXXXXX"); chmod 700 "$_f"
    python3 -c 'import socket,sys,os
s=socket.socket(socket.AF_UNIX); s.bind(sys.argv[1]); s.listen(8)
while True:
    c,_=s.accept(); open(sys.argv[2],"a").write("x\n"); c.close()' "$_f/wayland-0" "$_f/hits" &
    _p=$!; sleep 1
    env -u WAYLAND_DISPLAY -u DISPLAY -u SG_GATE_HOME SG_SH_FAKE_SESSION=$_f XDG_RUNTIME_DIR=$_f sh "$0" "$@"; _rc=$?
    kill $_p 2>/dev/null
    if [ -s "$_f/hits" ]; then echo "FAIL: $(wc -l < "$_f/hits") connection(s) to the person's wayland-0"; _rc=1
    else echo "PASS: nothing connected to the person's wayland-0"; fi
    rm -rf "$_f" "$SG_GATE_HOME"; exit $_rc
fi
# A gate's prefix must not reach the real HOME: its user folders (Desktop,
# Documents, ..., Templates) resolve into the gate's own HOME, and
# winemenubuilder is off; the gate has its own runtime directory, no
# Wayland display and no DISPLAY, and Wine connects to nothing of the
# person's session (a fake wayland-0 counts). Mutation: SG_GATE_HOME=x HOME=<a home with
# Desktop, Documents...> skips the helper, and the folders link there.
WINE=${WINE:-wine}
T=$(mktemp -d "${TMPDIR:-/var/tmp}/scratch-home-gate.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
cleanup() { "$(sg_wineserver)" -k 2>/dev/null; rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM
rc=0
pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; rc=1; }
timeout 120 "$WINE" wineboot -i >/dev/null 2>&1
"$(sg_wineserver)" -w
[ -d "$WINEPREFIX/drive_c/users" ] && pass "prefix created" || fail "no prefix"
case "$HOME" in "${TMPDIR:-/var/tmp}"/sg-gate-home.*) pass "HOME is the gate's own ($HOME)" ;; *) fail "HOME is $HOME" ;; esac
n=0
for d in Desktop Documents Downloads Music Pictures Videos AppData/Roaming/Microsoft/Windows/Templates; do
    for u in "$WINEPREFIX"/drive_c/users/*/"$d"; do
        [ -e "$u" ] || continue
        r=$(readlink -f "$u")
        case "$r" in "$SG_GATE_HOME"*|"$WINEPREFIX"*) n=$((n+1)) ;; *) fail "$u resolves to $r" ;; esac
    done
done
[ "$n" -gt 0 ] && pass "$n user folders stay in the gate's HOME or the prefix"
sg_prefix_safe "$WINEPREFIX" && pass "sg_prefix_safe accepts the prefix" || fail "sg_prefix_safe refused"
case "$XDG_RUNTIME_DIR" in "$SG_GATE_HOME"/*) pass "XDG_RUNTIME_DIR is the gate's own" ;; *) fail "XDG_RUNTIME_DIR is $XDG_RUNTIME_DIR" ;; esac
[ "$(stat -c %a "$XDG_RUNTIME_DIR")" = 700 ] && pass "and private (0700)" || fail "runtime dir mode $(stat -c %a "$XDG_RUNTIME_DIR")"
[ "$SG_REAL_XDG_RUNTIME_DIR" = "$SG_SH_FAKE_SESSION" ] && pass "SG_REAL_XDG_RUNTIME_DIR keeps the person's" || fail "the kept runtime dir is $SG_REAL_XDG_RUNTIME_DIR"
[ "$WAYLAND_DISPLAY" = sg-gate-no-wayland ] && [ ! -e "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ] && pass "WAYLAND_DISPLAY names nothing" || fail "WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
[ -z "${DISPLAY:-}" ] && pass "DISPLAY (the person's) unset" || fail "DISPLAY=$DISPLAY"
case ";$WINEDLLOVERRIDES;" in *";winemenubuilder.exe=d;"*) pass "winemenubuilder is off" ;; *) fail "winemenubuilder on: $WINEDLLOVERRIDES" ;; esac
exit $rc
