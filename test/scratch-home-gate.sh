#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A gate's prefix must not reach the real HOME: its user folders (Desktop,
# Documents, ..., Templates) resolve into the gate's own HOME, and
# winemenubuilder is off. Mutation: SG_GATE_HOME=x HOME=<a home with
# Desktop, Documents...> skips the helper, and the folders link there.
WINE=${WINE:-wine}
T=$(mktemp -d "${TMPDIR:-/var/tmp}/scratch-home-gate.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
cleanup() { "${WINE%/wine}/wineserver" -k 2>/dev/null; rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM
rc=0
pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*"; rc=1; }
"$WINE" wineboot -i >/dev/null 2>&1
"${WINE%/wine}/wineserver" -w
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
case ";$WINEDLLOVERRIDES;" in *";winemenubuilder.exe=d;"*) pass "winemenubuilder is off" ;; *) fail "winemenubuilder on: $WINEDLLOVERRIDES" ;; esac
exit $rc
