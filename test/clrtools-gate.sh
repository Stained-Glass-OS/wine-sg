#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ngen, aspnet_regiis, servicemodelreg (6067, C3): they printed a FIXME and exited 0 whatever was asked, so an
# installer's "register ASP.NET with IIS" or "grant this account" appeared to succeed. Now:
#  - ngen install/uninstall/update/display/executeQueuedItems/queue status succeed saying what they did (no native
#    images: the runtime compiles on demand); an unknown action prints the usage and fails (-1);
#  - aspnet_regiis and servicemodelreg list nothing (-lv, -lk) and print their usage (-?), and refuse every change
#    (no IIS; no machine-wide security or configuration changes) with a message and exit code 1.
#   WINE=/opt/wine-sg/bin/wine test/clrtools-gate.sh   (mutants SG_MUTANT_NGEN_FAIL_UPDATE in programs/ngen/ngen_main.c,
#   SG_MUTANT_REGIIS_ACCEPT in programs/aspnet_regiis/main.c)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-clrtools.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# Wine Mono installs these as stand-ins of the builtins in its Framework directories (dotnetfakedlls.inf); the
# gate, without Wine Mono, puts the builtins in C:\ itself
D=$(dirname "$WINE")
C="$WINEPREFIX/drive_c"
for t in ngen aspnet_regiis servicemodelreg; do
    for f in "$D/../lib/wine/x86_64-windows/$t.exe" "$D/programs/$t/x86_64-windows/$t.exe"; do
        [ -f "$f" ] && { cp "$f" "$C/$t.exe"; break; }
    done
    [ -f "$C/$t.exe" ] || { fail "no $t.exe next to $WINE"; exit 1; }
done
run() {   # run WHAT EXPECTED-EXIT PATTERN COMMAND...
    what=$1 want=$2 pat=$3; shift 3
    (cd "$C" && timeout -s KILL 60 "$WINE" "$@" >"$T/out" 2>/dev/null </dev/null); rc=$?
    o=$(tr -d '\r' <"$T/out")
    if [ "$rc" = "$want" ] && printf '%s\n' "$o" | grep -q -- "$pat"; then pass "$what"
    else fail "$what: exit $rc (wanted $want), output: $(printf '%s' "$o" | tr '\n' '|' | cut -c1-200)"; fi
}
run "ngen install succeeds, saying no native image is generated" 0 "No native image" ngen.exe install foo.dll /queue
run "ngen uninstall" 0 "no native image" ngen.exe uninstall foo.dll
run "ngen update" 0 "up to date" ngen.exe update
run "ngen executeQueuedItems" 0 "up to date" ngen.exe executeQueuedItems
run "ngen display lists no native images" 0 "Native Images:" ngen.exe display
run "ngen queue status" 0 "no queued work" ngen.exe queue status
run "ngen of an unknown action: the usage, failure" 255 "Usage: ngen" ngen.exe frobnicate
run "aspnet_regiis -? prints the usage" 0 "aspnet_regiis.exe \[" aspnet_regiis.exe -?
run "aspnet_regiis -lv lists nothing" 0 "" aspnet_regiis.exe -lv
run "aspnet_regiis -i is refused: no IIS" 1 "Nothing was changed" aspnet_regiis.exe -i
run "aspnet_regiis -ga is refused" 1 "access rights" aspnet_regiis.exe -ga "NT AUTHORITY\\NETWORK SERVICE"
run "aspnet_regiis -pe is refused" 1 "Nothing was changed" aspnet_regiis.exe -pe connectionStrings -app /x
run "servicemodelreg -lv lists nothing" 0 "" servicemodelreg.exe -lv
run "servicemodelreg -ia is refused" 1 "Nothing was changed" servicemodelreg.exe -ia
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
